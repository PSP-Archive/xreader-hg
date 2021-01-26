/*
 * This file is part of xReader.
 *
 * Copyright (C) 2008 hrimfaxi (outmatch@gmail.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <pspkernel.h>
#include <stdio.h>
#include <pspinit.h>
#include "common/utils.h"
#include "charsets.h"
#include "fat.h"
#include "dbg.h"
#include "thread_lock.h"
#include "xrPrx/xrPrx.h"
#include "scene.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

#define PSP_GO 4

static int fatfd = -1;
static t_fat_dbr dbr;
static t_fat_mbr mbr;
static u32 *fat_table = NULL;
static u64 dbr_pos = 0;
static u64 root_pos = 0;
static u64 data_pos = 0;
static u64 bytes_per_clus = 0;
static u32 loadcount = 0;
static u32 clus_max = 0;
static enum
{
	fat12,
	fat16,
	fat32
} fat_type = fat16;

static struct psp_mutex_t fat_l;
static bool fat_inited = false;

extern bool xrprx_loaded;

static inline SceUID _xrIoOpen(const char *file, int flags, SceMode mode)
{
	if(xrprx_loaded) {
		return xrIoOpen(file, flags, mode);
	}

	return sceIoOpen(file, flags, mode);
}

static inline SceOff _xrIoLseek(SceUID fd, SceOff offset, int whence)
{
	if(xrprx_loaded) {
		return xrIoLseek(fd, offset, whence);
	}

	return sceIoLseek(fd, offset, whence);
}

static inline int _xrIoRead(SceUID fd, void *data, SceSize size)
{
	if(xrprx_loaded) {
		return xrIoRead(fd, data, size);
	}

	return sceIoRead(fd, data, size);
}

static inline int _xrIoClose(SceUID fd)
{
	if(xrprx_loaded) {
		return xrIoClose(fd);
	}

	return sceIoClose(fd);
}

static void msstor_close(void)
{
	if (fatfd >= 0) {
		_xrIoClose(fatfd);
		fatfd = -1;
	}
}

static int is_on_ef0(void)
{
	int apitype;

	(void)apitype;

	if(psp_model != PSP_GO) {
		return 0;
	}

#ifdef _DEBUG
	return 1;
#else

	apitype = xrKernelInitApitype();

	if (apitype != 0x152) {
		return 0;
	}

	return 1;
#endif
}

static SceUID msstor_reopen(void)
{
	msstor_close();

	if (is_on_ef0()) {
		fatfd = _xrIoOpen("eflash0a0f0:", PSP_O_RDONLY, 0777);
	} else {
		fatfd = _xrIoOpen("msstor:", PSP_O_RDONLY, 0777);
	}

	return fatfd;
}

void fat_powerdown(void)
{
	dbg_printf(d, "%s", __func__);

	fat_lock();
	msstor_close();
}

void fat_powerup(void)
{
	dbg_printf(d, "%s", __func__);

	fatfd = msstor_reopen();
	fat_unlock();
}

void fat_lock(void)
{
	xr_lock(&fat_l);
}

void fat_unlock(void)
{
	xr_unlock(&fat_l);
}

static bool fat_init(void)
{
	u64 total_sec, fat_sec, root_sec, data_sec, data_clus;

	xr_lock_init(&fat_l);
	fat_lock();
	fatfd = msstor_reopen();
	if (fatfd < 0) {
		fat_unlock();
		return false;
	}
	if (_xrIoRead(fatfd, &mbr, sizeof(mbr)) != sizeof(mbr)) {
		msstor_close();
		fat_unlock();
		return false;
	}
	dbr_pos = mbr.dpt[0].start_sec * 0x200;
	if (_xrIoLseek(fatfd, dbr_pos, PSP_SEEK_SET) != dbr_pos || _xrIoRead(fatfd, &dbr, sizeof(dbr)) < sizeof(dbr)) {
		dbr_pos = 0;
		if (_xrIoLseek(fatfd, dbr_pos, PSP_SEEK_SET) != dbr_pos || _xrIoRead(fatfd, &dbr, sizeof(dbr)) < sizeof(dbr)) {
			msstor_close();
			fat_unlock();
			return false;
		}
	}

	total_sec = (dbr.total_sec == 0) ? dbr.big_total_sec : dbr.total_sec;
	fat_sec = (dbr.sec_per_fat == 0) ? dbr.ufat.fat32.sec_per_fat : dbr.sec_per_fat;
	root_sec = (dbr.root_entry * 32 + dbr.bytes_per_sec - 1) / dbr.bytes_per_sec;
	data_sec = total_sec - dbr.reserved_sec - (dbr.num_fats * fat_sec) - root_sec;
	data_clus = data_sec / dbr.sec_per_clus;

	if (data_clus < 4085) {
		fat_type = fat12;
		clus_max = 0x0FF0;
	} else if (data_clus < 65525) {
		fat_type = fat16;
		clus_max = 0xFFF0;
	} else {
		fat_type = fat32;
		clus_max = 0x0FFFFFF0;
	}

	bytes_per_clus = 1ull * dbr.sec_per_clus * dbr.bytes_per_sec;
	if (fat_type == fat32) {
		data_pos = 1ull * dbr_pos;
		data_pos += 1ull * (dbr.ufat.fat32.sec_per_fat * dbr.num_fats + dbr.reserved_sec) * dbr.bytes_per_sec;
		root_pos = data_pos;
		root_pos += 1ull * bytes_per_clus * dbr.ufat.fat32.root_clus;
	} else {
		root_pos = 1ull * dbr_pos;
		root_pos += 1ull * (dbr.num_fats * dbr.sec_per_fat + dbr.reserved_sec) * dbr.bytes_per_sec;
		data_pos = root_pos;
		data_pos += 1ull * dbr.root_entry * sizeof(t_fat_entry);
	}

	msstor_close();
	fat_inited = true;
	fat_unlock();

	return true;
}

static void fat_free(void)
{
	msstor_close();
	memset(&dbr, 0, sizeof(dbr));
	memset(&mbr, 0, sizeof(mbr));
	root_pos = 0;
	data_pos = 0;
	bytes_per_clus = 0;
	loadcount = 0;
	clus_max = 0;
	fat_type = fat16;
	fat_inited = false;
	xr_lock_destroy(&fat_l);
}

static bool convert_table_fat12(void)
{
	u16 *otable = malloc(dbr.sec_per_fat * dbr.bytes_per_sec);
	int i, j, entrycount;

	if (otable == NULL)
		return false;

	memcpy(otable, fat_table, dbr.sec_per_fat * dbr.bytes_per_sec);
	entrycount = dbr.sec_per_fat * dbr.bytes_per_sec / sizeof(u16) * 4 / 3;
	free(fat_table);
	fat_table = malloc(sizeof(*fat_table) * entrycount);

	if (fat_table == NULL) {
		free(otable);
		return false;
	}
	for (i = 0, j = 0; i < entrycount; i += 4, j += 3) {
		fat_table[i] = otable[j] & 0x0FFF;
		fat_table[i + 1] = ((otable[j] >> 12) & 0x000F) | ((otable[j + 1] & 0x00FF) << 4);
		fat_table[i + 2] = ((otable[j + 1] >> 8) & 0x00FF) | ((otable[j + 2] & 0x000F)
															  << 8);
		fat_table[i + 3] = otable[j + 2] >> 4;
	}
	free(otable);
	return true;
}

static bool convert_table_fat16(void)
{
	u16 *otable = malloc(dbr.sec_per_fat * dbr.bytes_per_sec);
	int i, entrycount;

	if (otable == NULL)
		return false;

	memcpy(otable, fat_table, dbr.sec_per_fat * dbr.bytes_per_sec);
	entrycount = dbr.sec_per_fat * dbr.bytes_per_sec / sizeof(u16);
	free(fat_table);
	fat_table = malloc(sizeof(*fat_table) * entrycount);

	if (fat_table == NULL) {
		free(otable);
		return false;
	}
	for (i = 0; i < entrycount; i++)
		fat_table[i] = otable[i];
	free(otable);
	return true;
}

static bool fat_load_table(void)
{
	u32 fat_table_size;

	if (loadcount > 0) {
		loadcount++;
		return true;
	}

	fatfd = msstor_reopen();

	if (fatfd < 0)
		return false;

	fat_table_size = ((fat_type == fat32) ? dbr.ufat.fat32.sec_per_fat : dbr.sec_per_fat) * dbr.bytes_per_sec;

	if (_xrIoLseek
		(fatfd, dbr_pos + dbr.reserved_sec * dbr.bytes_per_sec,
		 PSP_SEEK_SET) != dbr_pos + dbr.reserved_sec * dbr.bytes_per_sec || (fat_table = malloc(fat_table_size)) == NULL) {
		msstor_close();
		return false;
	}
	if ((_xrIoRead(fatfd, fat_table, fat_table_size) != fat_table_size)
		|| (fat_type == fat12 && !convert_table_fat12())
		|| (fat_type == fat16 && !convert_table_fat16())) {
		msstor_close();
		free(fat_table);
		fat_table = NULL;
		return false;
	}
	loadcount = 1;
	return true;
}

static void fat_free_table(void)
{
	if (loadcount > 0) {
		loadcount--;
		if (loadcount > 0)
			return;
		if (fat_table != NULL) {
			free(fat_table);
			fat_table = NULL;
		}
		msstor_close();
	}
}

static u8 fat_calc_chksum(p_fat_entry info)
{
	u32 i;
	u8 *n = (u8 *) & info->norm.filename[0];
	u8 chksum = 0;

	for (i = 0; i < 11; i++)
		chksum = ((chksum & 1) ? 0x80 : 0) + (chksum >> 1) + n[i];

	return chksum;
}

static bool fat_dir_list(u32 clus, u32 * count, p_fat_entry * entrys)
{
	if (clus == 0)
		return false;
	if (clus < 2) {
		*count = dbr.root_entry;
		if ((*entrys = malloc(*count * sizeof(**entrys))) == NULL)
			return false;
		if (_xrIoLseek(fatfd, root_pos, PSP_SEEK_SET) != root_pos || _xrIoRead(fatfd, *entrys, *count * sizeof(t_fat_entry)) != *count * sizeof(t_fat_entry)) {
			free(*entrys);
			return false;
		}
	} else {
		u32 epc;
		u32 ep;
		u32 c2;

		if (fat_table[clus] < 2)
			return false;

		c2 = clus;
		*count = 1;

		while (fat_table[c2] < clus_max && fat_table[c2] > 1) {
			c2 = fat_table[c2];
			(*count)++;
		}
		c2 = clus;
		epc = (bytes_per_clus / sizeof(t_fat_entry));
		ep = 0;

		(*count) *= epc;
		if ((*entrys = malloc(*count * sizeof(**entrys))) == NULL)
			return false;
		do {
			u64 epos = data_pos + 1ull * (c2 - 2) * bytes_per_clus;

			if (_xrIoLseek(fatfd, epos, PSP_SEEK_SET) != epos || _xrIoRead(fatfd, &(*entrys)[ep], bytes_per_clus) != bytes_per_clus) {
				free(*entrys);
				return false;
			}
			ep += epc;
			c2 = fat_table[c2];
		} while (c2 < clus_max && c2 > 1);
	}
	return true;
}

static bool fat_get_longname(p_fat_entry entrys, u32 cur, char *longnamestr)
{
	u16 chksum = fat_calc_chksum(&entrys[cur]);
	u32 j = cur;
	u16 longname[260];

	memset(longname, 0, 260 * sizeof(u16));
	while (j > 0) {
		u32 order;
		u32 ppos;
		u32 k;

		j--;
		if (entrys[j].norm.attr != 0x0F || entrys[j].longfile.checksum != chksum || entrys[j].norm.filename[0] == 0 || (u8) entrys[j].norm.filename[0] == 0xE5)
			return false;
		order = entrys[j].longfile.order & 0x3F;

		if (order > 20)
			return false;

		ppos = (order - 1) * 13;

		for (k = 0; k < 5; k++)
			longname[ppos++] = entrys[j].longfile.uni_name[k];
		for (k = 0; k < 6; k++)
			longname[ppos++] = entrys[j].longfile.uni_name2[k];
		for (k = 0; k < 2; k++)
			longname[ppos++] = entrys[j].longfile.uni_name3[k];
		if ((entrys[j].longfile.order & 0x40) > 0)
			break;
	}
	if (entrys[j].norm.attr != 0x0F || (entrys[j].longfile.order & 0x40) == 0)
		return false;
	longname[255] = 0;
	memset(longnamestr, 0, 256);
	charsets_ucs_conv((const u8 *) longname, sizeof(longname), (u8 *) longnamestr, 256);
	return true;
}

static void fat_get_shortname(p_fat_entry entry, char *shortnamestr)
{
	static bool chartable[256] = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 0x00
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 0x00
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 0x00
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 0x30
		0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,	// 0x40
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,	// 0x50
		0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,	// 0x60
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,	// 0x70
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 0x80
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 0x90
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 0xA0
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 0xB0
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 0xC0
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 0xD0
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 0xE0
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0	// 0xF0
	};
	u32 i = 0;
	u8 abit = 0;

	if ((entry->norm.flag & 0x08) > 0)
		abit = 0x20;
	while (i < 8 && entry->norm.filename[i] != 0x20) {
		*shortnamestr++ = entry->norm.filename[i] | ((chartable[(u8) entry->norm.filename[i]]) ? abit : 0);
		i++;
	}
	if (entry->norm.fileext[0] != 0x20) {
		*shortnamestr++ = '.';
		i = 0;
		while (i < 3 && entry->norm.fileext[i] != 0x20)
			*shortnamestr++ = entry->norm.fileext[i++] | abit;
	}
	*shortnamestr = 0;
}

extern bool fat_locate(const char *name, char *sname, u32 clus, p_fat_entry info)
{
	u32 count;
	p_fat_entry entrys;
	SceUID dl;
	char shortname[11];
	bool onlylong = false;
	u32 nlen;
	SceIoDirent sid;
	u32 i;

	if (!fat_dir_list(clus, &count, &entrys))
		return false;

	dl = sceIoDopen(sname);

	if (dl < 0)
		return false;

	nlen = strlen(name);

	if (nlen > 12)
		onlylong = true;
	else {
		char *dot = strrchr(name, '.');

		if ((dot == NULL && nlen < 9)
			|| (dot - name < 9 && nlen - 1 - (dot - name) < 4)) {
			memset(&shortname[0], 0x20, 11);
			memcpy(&shortname[0], name, (dot == NULL) ? nlen : (dot - name));
			if (dot != NULL)
				memcpy(&shortname[8], dot + 1, nlen - 1 - (dot - name));
		} else
			onlylong = true;
	}

	for (i = 0; i < count; i++) {
		if ((entrys[i].norm.attr & FAT_FILEATTR_VOLUME) == 0 && entrys[i].norm.filename[0] != 0 && (u8) entrys[i].norm.filename[0] != 0xE5) {
			int result;
			char longnames[256];

			memset(&sid, 0, sizeof(SceIoDirent));

			while ((result = sceIoDread(dl, &sid)) > 0 && (sid.d_stat.st_attr & 0x08) > 0);
			if (result == 0)
				break;
			if (entrys[i].norm.filename[0] == 0x05)
				entrys[i].norm.filename[0] = 0xE5;
			if (!onlylong && strnicmp(shortname, &entrys[i].norm.filename[0], 11) == 0) {
				if ((u8) entrys[i].norm.filename[0] == 0xE5)
					entrys[i].norm.filename[0] = 0x05;
				memcpy(info, &entrys[i], sizeof(t_fat_entry));
				free(entrys);
				if (psp_fw_version <= 0x03070110) {
					strcat_s(sname, 256, sid.d_name);
				} else {
					char short_name[256];

					fat_get_shortname(info, short_name);
					strcat_s(sname, 256, short_name);
				}
				if ((info->norm.attr & FAT_FILEATTR_DIRECTORY) > 0)
					strcat_s(sname, 256, "/");
				sceIoDclose(dl);
				return true;
			}
			if ((u8) entrys[i].norm.filename[0] == 0xE5)
				entrys[i].norm.filename[0] = 0x05;

			if (!fat_get_longname(entrys, i, longnames))
				continue;
			if (stricmp(name, longnames) == 0) {
				memcpy(info, &entrys[i], sizeof(t_fat_entry));
				free(entrys);
				if (psp_fw_version <= 0x03070110) {
					strcat_s(sname, 256, sid.d_name);
				} else {
					char short_name[256];

					fat_get_shortname(info, short_name);
					strcat_s(sname, 256, short_name);
				}
				if ((info->norm.attr & FAT_FILEATTR_DIRECTORY) > 0)
					strcat_s(sname, 256, "/");
				sceIoDclose(dl);
				return true;
			}
		}
	}
	free(entrys);
	sceIoDclose(dl);
	return false;
}

static u32 fat_dir_clus(const char *dir, char *shortdir)
{
	char rdir[256];
	char *partname;
	u32 clus;
	t_fat_entry entry;

	if (!fat_load_table() || fatfd < 0)
		return 0;

	STRCPY_S(rdir, dir);
	partname = strtok(rdir, "/\\");

	if (partname == NULL) {
		fat_free_table();
		return 0;
	}
	if (strcmp(partname, "ms0:") != 0 && strcmp(partname, "fatms:") != 0 && strcmp(partname, "fatms:") != 0 && strcmp(partname, "ef0:") != 0) {
		fat_free_table();
		return 0;
	}
	strcpy_s(shortdir, 256, partname);
	strcat_s(shortdir, 256, "/");
	partname = strtok(NULL, "/\\");
	clus = (fat_type == fat32) ? dbr.ufat.fat32.root_clus : 1;

	while (partname != NULL) {
		if (partname[0] != 0) {
			if (fat_locate(partname, shortdir, clus, &entry)) {
				clus = entry.norm.clus_high;
				clus = ((fat_type == fat32) ? (clus << 16) : 0) + entry.norm.clus;
			} else {
				fat_free_table();
				return 0;
			}
		}
		partname = strtok(NULL, "/\\");
	}
	fat_free_table();
	return clus;
}

extern u32 fat_readdir(const char *dir, char *sdir, p_fat_info * info)
{
	u32 clus;
	SceUID dl = 0;
	u32 ecount = 0;
	p_fat_entry entrys;
	u32 count = 0, cur = 0, i;
	SceIoDirent sid;

	fat_lock();

	if (!fat_inited) {
		fat_init();
	}

	if (!fat_inited) {
		fat_unlock();
		fat_free();

		return INVALID;
	}

	if (!fat_load_table() || fatfd < 0) {
		fat_unlock();
		fat_free();

		return INVALID;
	}

	clus = fat_dir_clus(dir, sdir);

	if (clus == 0 || (dl = sceIoDopen(sdir)) < 0) {
		fat_free_table();
		sceIoDclose(dl);
		fat_unlock();
		fat_free();

		return INVALID;
	}

	if (!fat_dir_list(clus, &ecount, &entrys)) {
		fat_free_table();
		sceIoDclose(dl);
		fat_unlock();
		fat_free();

		return INVALID;
	}

	for (i = 0; i < ecount; i++) {
		if ((entrys[i].norm.attr & FAT_FILEATTR_VOLUME) != 0
			|| entrys[i].norm.filename[0] == 0
			|| (u8) entrys[i].norm.filename[0] == 0xE5 || (entrys[i].norm.filename[0] == '.' && entrys[i].norm.filename[1] == 0x20))
			continue;
		count++;
	}
	if (count == 0 || (*info = malloc(count * sizeof(**info))) == NULL) {
		free(entrys);
		fat_free_table();
		sceIoDclose(dl);
		fat_unlock();
		fat_free();

		return INVALID;
	}

	for (i = 0; i < ecount; i++) {
		int result;
		p_fat_info inf;

		if ((entrys[i].norm.attr & FAT_FILEATTR_VOLUME) != 0
			|| entrys[i].norm.filename[0] == 0
			|| (u8) entrys[i].norm.filename[0] == 0xE5 || (entrys[i].norm.filename[0] == '.' && entrys[i].norm.filename[1] == 0x20)
			|| (entrys[i].norm.filename[0] == '.' && entrys[i].norm.filename[1] == '.' && entrys[i].norm.filename[2] == 0x20)
			)
			continue;
		memset(&sid, 0, sizeof(SceIoDirent));

		while ((result = sceIoDread(dl, &sid)) > 0 && ((sid.d_stat.st_attr & 0x08) > 0 || (sid.d_name[0] == '.' && sid.d_name[1] == 0)));

		if (result == 0)
			break;

		inf = &((*info)[cur]);

		fat_get_shortname(&entrys[i], inf->filename);
		if (inf->filename[0] == 0x05)
			inf->filename[0] = 0xE5;
		if (!fat_get_longname(entrys, i, inf->longname))
			STRCPY_S(inf->longname, inf->filename);
		if (psp_fw_version <= 0x03070110) {
			STRCPY_S(inf->filename, sid.d_name);
		}
		inf->filesize = entrys[i].norm.filesize;
		inf->cdate = entrys[i].norm.cr_date;
		inf->ctime = entrys[i].norm.cr_time;
		inf->mdate = entrys[i].norm.last_mod_date;
		inf->mtime = entrys[i].norm.last_mod_time;
		inf->clus = ((fat_type == fat32) ? (((u32) entrys[i].norm.clus_high) << 16) : 0) + entrys[i].norm.clus;
		inf->attr = entrys[i].norm.attr;
		cur++;
	}

	free(entrys);
	fat_free_table();
	sceIoDclose(dl);
	fat_unlock();
	fat_free();

	return cur;
}

/**
 * 将长文件名转换为8.3文件名
 * @param shortname 短文件名
 * @param longname 长文件名
 * @param size 短文件名缓冲字节大小
 * @return 是否成功
 * @note 参考: http://support.microsoft.com/kb/142982/zh-cn
 */
extern bool fat_longnametoshortname(char *shortname, const char *longname, u32 size)
{
	p_fat_info info = NULL;
	char dirname[PATH_MAX], spath[PATH_MAX], longfilename[PATH_MAX];
	char *p = NULL;
	u32 i, count;

	STRCPY_S(dirname, longname);
	if ((p = strrchr(dirname, '/')) != NULL) {
		*p = '\0';
		STRCPY_S(longfilename, p + 1);
	} else {
		STRCPY_S(longfilename, dirname);
	}

	count = fat_readdir(dirname, spath, &info);

	if (count == INVALID || info == NULL) {
		return false;
	}

	for (i = 0; i < count; ++i) {
		if (stricmp(info[i].longname, longfilename) == 0) {
			if (spath[strlen(spath) - 1] != '/')
				STRCAT_S(spath, "/");
			STRCAT_S(spath, info[i].filename);
			strcpy_s(shortname, size, spath);
			free(info);
			return true;
		}
	}

	free(info);
	return false;
}
