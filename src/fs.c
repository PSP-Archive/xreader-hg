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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pspuser.h>
#include <unzip.h>
#include <chm_lib.h>
#include <unrar.h>
#include "common/utils.h"
#include "html.h"
#include "display.h"
#include "charsets.h"
#include "fat.h"
#include "fs.h"
#include "scene.h"
#include "conf.h"
#include "win.h"
#include "passwdmgr.h"
#include "osk.h"
#include "bg.h"
#include "image.h"
#include "archive.h"
#include "freq_lock.h"
#include "audiocore/musicdrv.h"
#include "dbg.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

typedef struct
{
	const char *ext;
	t_fs_filetype ft;
} t_fs_filetype_entry;

typedef struct
{
	const char *fname;
	t_fs_filetype ft;
} t_fs_specfiletype_entry;

t_fs_filetype_entry ft_table[] = {
	{"lrc", fs_filetype_txt},
	{"txt", fs_filetype_txt},
	{"log", fs_filetype_txt},
	{"ini", fs_filetype_txt},
	{"cfg", fs_filetype_txt},
	{"conf", fs_filetype_txt},
	{"inf", fs_filetype_txt},
	{"xml", fs_filetype_txt},
	{"cpp", fs_filetype_txt},
	{"in", fs_filetype_txt},
	{"ac", fs_filetype_txt},
	{"m4", fs_filetype_txt},
	{"am", fs_filetype_txt},
	{"mak", fs_filetype_txt},
	{"exp", fs_filetype_txt},
	{"sh", fs_filetype_txt},
	{"asm", fs_filetype_txt},
	{"s", fs_filetype_txt},
	{"patch", fs_filetype_txt},
	{"c", fs_filetype_txt},
	{"h", fs_filetype_txt},
	{"hpp", fs_filetype_txt},
	{"cc", fs_filetype_txt},
	{"cxx", fs_filetype_txt},
	{"pas", fs_filetype_txt},
	{"bas", fs_filetype_txt},
	{"py", fs_filetype_txt},
	{"mk", fs_filetype_txt},
	{"rc", fs_filetype_txt},
	{"pl", fs_filetype_txt},
	{"cgi", fs_filetype_txt},
	{"bat", fs_filetype_txt},
	{"js", fs_filetype_txt},
	{"vbs", fs_filetype_txt},
	{"vb", fs_filetype_txt},
	{"cs", fs_filetype_txt},
	{"css", fs_filetype_txt},
	{"csv", fs_filetype_txt},
	{"php", fs_filetype_txt},
	{"php3", fs_filetype_txt},
	{"asp", fs_filetype_txt},
	{"aspx", fs_filetype_txt},
	{"java", fs_filetype_txt},
	{"jsp", fs_filetype_txt},
	{"awk", fs_filetype_txt},
	{"tcl", fs_filetype_txt},
	{"y", fs_filetype_txt},
	{"html", fs_filetype_html},
	{"htm", fs_filetype_html},
	{"shtml", fs_filetype_html},
#ifdef ENABLE_IMAGE
	{"png", fs_filetype_png},
	{"gif", fs_filetype_gif},
	{"jpg", fs_filetype_jpg},
	{"jpeg", fs_filetype_jpg},
	{"thm", fs_filetype_jpg},
	{"bmp", fs_filetype_bmp},
	{"tga", fs_filetype_tga},
#endif
	{"zip", fs_filetype_zip},
	{"cbz", fs_filetype_zip},
	{"gz", fs_filetype_gz},
	{"umd", fs_filetype_umd},
	{"pdb", fs_filetype_pdb},
	{"chm", fs_filetype_chm},
	{"rar", fs_filetype_rar},
	{"cbr", fs_filetype_rar},
	{"pbp", fs_filetype_prog},
	{"ebm", fs_filetype_ebm},
	{"iso", fs_filetype_iso},
	{"cso", fs_filetype_iso},
#ifdef ENABLE_TTF
	{"ttf", fs_filetype_font},
	{"ttc", fs_filetype_font},
#endif
	{NULL, fs_filetype_unknown}
};

t_fs_specfiletype_entry ft_spec_table[] = {
	{"Makefile", fs_filetype_txt},
	{"LICENSE", fs_filetype_txt},
	{"TODO", fs_filetype_txt},
	{"Configure", fs_filetype_txt},
	{"Changelog", fs_filetype_txt},
	{"Readme", fs_filetype_txt},
	{"Version", fs_filetype_txt},
	{"INSTALL", fs_filetype_txt},
	{"CREDITS", fs_filetype_txt},
	{NULL, fs_filetype_unknown}
};

int MAX_ITEM_NAME_LEN = 40;
p_umd_chapter p_umdchapter = NULL;
p_win_menu g_menu = NULL;

void filename_to_itemname(p_win_menuitem item, const char *filename)
{
	if ((item->width = strlen(filename)) > MAX_ITEM_NAME_LEN) {
		mbcsncpy_s(((unsigned char *) item->name), MAX_ITEM_NAME_LEN - 2, ((const unsigned char *) filename), -1);
		if (strlen(item->name) < MAX_ITEM_NAME_LEN - 3) {
			mbcsncpy_s(((unsigned char *) item->name), MAX_ITEM_NAME_LEN, ((const unsigned char *) filename), -1);
			STRCAT_S(item->name, "..");
		} else
			STRCAT_S(item->name, "...");
		item->width = MAX_ITEM_NAME_LEN;
	} else {
		STRCPY_S(item->name, filename);
	}
}

static p_win_menu menu_renew(p_win_menu * menu)
{
	if (*menu != NULL) {
		win_menu_destroy(*menu);
		*menu = NULL;
	}

	*menu = win_menu_new();

	return *menu;
}

extern u32 fs_list_device(const char *dir, const char *sdir, u32 icolor, u32 selicolor, u32 selrcolor, u32 selbcolor)
{
	enum {
		PSP_GO = 4,
	};
	
	t_win_menuitem item;

	strcpy_s((char *) sdir, 256, dir);

	if (menu_renew(&g_menu) == NULL) {
		return 0;
	}

	win_menuitem_new(&item);
	STRCPY_S(item.name, "<MemoryStick>");
	buffer_copy_string(item.compname, "ms0:");
	buffer_copy_string(item.shortname, "ms0:");
	item.data = (void *) fs_filetype_dir;
	item.width = 13;
	item.selected = false;
	item.icolor = icolor;
	item.selicolor = selicolor;
	item.selrcolor = selrcolor;
	item.selbcolor = selbcolor;
	win_menu_add(g_menu, &item);

	if (config.hide_flash == false) {
		win_menuitem_new(&item);
		STRCPY_S(item.name, "<NandFlash 0>");
		buffer_copy_string(item.compname, "flash0:");
		item.data = (void *) fs_filetype_dir;
		item.width = 13;
		item.selected = false;
		item.icolor = icolor;
		item.selicolor = selicolor;
		item.selrcolor = selrcolor;
		item.selbcolor = selbcolor;
		win_menu_add(g_menu, &item);

		win_menuitem_new(&item);
		STRCPY_S(item.name, "<NandFlash 1>");
		buffer_copy_string(item.compname, "flash1:");
		item.data = (void *) fs_filetype_dir;
		item.width = 13;
		item.selected = false;
		item.icolor = icolor;
		item.selicolor = selicolor;
		item.selrcolor = selrcolor;
		item.selbcolor = selbcolor;
		win_menu_add(g_menu, &item);
	}

#ifdef _DEBUG
	if(psp_model == PSP_GO) {
		win_menuitem_new(&item);
		STRCPY_S(item.name, "<Internal Memory>");
		buffer_copy_string(item.compname, "ef0:");
		buffer_copy_string(item.shortname, "ef0:");
		item.data = (void *) fs_filetype_dir;
		item.width = sizeof("<Internal Memory>")-1;
		item.selected = false;
		item.icolor = icolor;
		item.selicolor = selicolor;
		item.selrcolor = selrcolor;
		item.selbcolor = selbcolor;
		win_menu_add(g_menu, &item);
	}
#endif

	return g_menu->size;
}

static int add_parent_to_menu(p_win_menu menu, u32 icolor, u32 selicolor, u32 selrcolor, u32 selbcolor)
{
	t_win_menuitem item;

	if (menu == NULL) {
		return -1;
	}

	win_menuitem_new(&item);
	STRCPY_S(item.name, "<..>");
	buffer_copy_string(item.compname, "..");
	buffer_copy_string(item.shortname, "..");
	item.data = (void *) fs_filetype_dir;
	item.width = 4;
	item.selected = false;
	item.icolor = icolor;
	item.selicolor = selicolor;
	item.selrcolor = selrcolor;
	item.selbcolor = selbcolor;
	win_menu_add(menu, &item);

	return 0;
}

extern u32 fs_flashdir_to_menu(const char *dir, const char *sdir, u32 icolor, u32 selicolor, u32 selrcolor, u32 selbcolor)
{
	int fid;
	SceIoDirent info;
	int fd;
	t_win_menuitem item;

	if (menu_renew(&g_menu) == NULL) {
		return 0;
	}

	fid = freq_enter_hotzone();
	strcpy_s((char *) sdir, 256, dir);
	fd = sceIoDopen(dir);

	if (fd < 0) {
		freq_leave(fid);
		return 0;
	}

	add_parent_to_menu(g_menu, icolor, selicolor, selrcolor, selbcolor);

	memset(&info, 0, sizeof(SceIoDirent));

	while (sceIoDread(fd, &info) > 0) {
		win_menuitem_new(&item);

		if ((info.d_stat.st_mode & FIO_S_IFMT) == FIO_S_IFDIR) {
			if (info.d_name[0] == '.' && info.d_name[1] == 0) {
				win_menuitem_free(&item);
				continue;
			}

			if (strcmp(info.d_name, "..") == 0) {
				win_menuitem_free(&item);
				continue;
			}

			item.data = (void *) fs_filetype_dir;
			buffer_copy_string(item.compname, info.d_name);
			item.name[0] = '<';

			if ((item.width = strlen(info.d_name) + 2) > MAX_ITEM_NAME_LEN) {
				mbcsncpy_s((unsigned char *) &item.name[1], MAX_ITEM_NAME_LEN - 4, (const unsigned char *) info.d_name, -1);
				STRCAT_S(item.name, "...>");
				item.width = MAX_ITEM_NAME_LEN;
			} else {
				mbcsncpy_s((unsigned char *) &item.name[1], MAX_ITEM_NAME_LEN - 1, (const unsigned char *) info.d_name, -1);
				STRCAT_S(item.name, ">");
			}
		} else {
			t_fs_filetype ft = fs_file_get_type(info.d_name);

			item.data = (void *) ft;
			buffer_copy_string(item.compname, info.d_name);
			buffer_copy_string(item.shortname, info.d_name);
			filename_to_itemname(&item, info.d_name);
		}

		item.icolor = icolor;
		item.selicolor = selicolor;
		item.selrcolor = selrcolor;
		item.selbcolor = selbcolor;
		item.selected = false;
		item.data2[0] = ((info.d_stat.st_ctime.year - 1980) << 9) + (info.d_stat.st_ctime.month << 5) + info.d_stat.st_ctime.day;
		item.data2[1] = (info.d_stat.st_ctime.hour << 11) + (info.d_stat.st_ctime.minute << 5) + info.d_stat.st_ctime.second / 2;
		item.data2[2] = ((info.d_stat.st_mtime.year - 1980) << 9) + (info.d_stat.st_mtime.month << 5) + info.d_stat.st_mtime.day;
		item.data2[3] = (info.d_stat.st_mtime.hour << 11) + (info.d_stat.st_mtime.minute << 5) + info.d_stat.st_mtime.second / 2;
		item.data3 = info.d_stat.st_size;

		win_menu_add(g_menu, &item);
	}

	sceIoDclose(fd);
	freq_leave(fid);

	return g_menu->size;
}

// New style fat system custom reading
extern u32 fs_dir_to_menu(const char *dir, char *sdir, u32 icolor, u32 selicolor, u32 selrcolor, u32 selbcolor, bool showhidden, bool showunknown)
{
	int fid;
	p_fat_info info;
	u32 count;
	u32 i;
	t_win_menuitem item;

	if (menu_renew(&g_menu) == NULL) {
		return 0;
	}

	fid = freq_enter_hotzone();
	count = fat_readdir(dir, sdir, &info);

	if (count == INVALID) {
		freq_leave(fid);
		return 0;
	}

	add_parent_to_menu(g_menu, icolor, selicolor, selrcolor, selbcolor);

	for (i = 0; i < count; i++) {
		win_menuitem_new(&item);

		if (!showhidden && (info[i].attr & FAT_FILEATTR_HIDDEN) > 0) {
			win_menuitem_free(&item);
			continue;
		}

		if (info[i].attr & FAT_FILEATTR_DIRECTORY) {
			item.data = (void *) fs_filetype_dir;
			buffer_copy_string(item.shortname, info[i].filename);
			buffer_copy_string(item.compname, info[i].longname);
			item.name[0] = '<';
			if ((item.width = strlen(info[i].longname) + 2) > MAX_ITEM_NAME_LEN) {
				strncpy_s(&item.name[1], NELEMS(item.name) - 1, info[i].longname, MAX_ITEM_NAME_LEN - 5);
				item.name[MAX_ITEM_NAME_LEN - 4] = item.name[MAX_ITEM_NAME_LEN - 3] = item.name[MAX_ITEM_NAME_LEN - 2] = '.';
				item.name[MAX_ITEM_NAME_LEN - 1] = '>';
				item.name[MAX_ITEM_NAME_LEN] = 0;
				item.width = MAX_ITEM_NAME_LEN;
			} else {
				strncpy_s(&item.name[1], NELEMS(item.name) - 1, info[i].longname, MAX_ITEM_NAME_LEN);
				item.name[item.width - 1] = '>';
				item.name[item.width] = 0;
			}
		} else {
			t_fs_filetype ft;

			if (info[i].filesize == 0) {
				win_menuitem_free(&item);
				continue;
			}

			ft = fs_file_get_type(info[i].longname);

			if (!showunknown && ft == fs_filetype_unknown) {
				win_menuitem_free(&item);
				continue;
			}

			item.data = (void *) ft;
			buffer_copy_string(item.shortname, info[i].filename);
			buffer_copy_string(item.compname, info[i].longname);
			filename_to_itemname(&item, info[i].longname);
		}
		item.icolor = icolor;
		item.selicolor = selicolor;
		item.selrcolor = selrcolor;
		item.selbcolor = selbcolor;
		item.selected = false;
		item.data2[0] = info[i].cdate;
		item.data2[1] = info[i].ctime;
		item.data2[2] = info[i].mdate;
		item.data2[3] = info[i].mtime;
		item.data3 = info[i].filesize;
		win_menu_add(g_menu, &item);
	}

	free(info);
	freq_leave(fid);

	return g_menu->size;
}

extern u32 fs_zip_to_menu(const char *zipfile, u32 icolor, u32 selicolor, u32 selrcolor, u32 selbcolor)
{
	int fid;
	unzFile unzf;
	t_win_menuitem item;

	if (menu_renew(&g_menu) == NULL) {
		return 0;
	}

	fid = freq_enter_hotzone();
	unzf = unzOpen(zipfile);

	if (unzf == NULL) {
		freq_leave(fid);
		return 0;
	}

	add_parent_to_menu(g_menu, icolor, selicolor, selrcolor, selbcolor);

	if (unzGoToFirstFile(unzf) != UNZ_OK) {
		unzClose(unzf);
		freq_leave(fid);

		return g_menu->size;
	}

	do {
		char fname[PATH_MAX];
		unz_file_info file_info;
		t_fs_filetype ft;
		char t[20];

		if (unzGetCurrentFileInfo(unzf, &file_info, fname, PATH_MAX, NULL, 0, NULL, 0) != UNZ_OK)
			break;

		if (file_info.uncompressed_size == 0)
			continue;

		ft = fs_file_get_type(fname);

		if (ft == fs_filetype_chm || ft == fs_filetype_zip || ft == fs_filetype_rar)
			continue;

		win_menuitem_new(&item);

		item.data = (void *) ft;
		buffer_copy_string(item.compname, fname);

		SPRINTF_S(t, "%u", (unsigned int) file_info.uncompressed_size);
		buffer_copy_string(item.shortname, t);
		filename_to_itemname(&item, fname);
		item.selected = false;
		item.icolor = icolor;
		item.selicolor = selicolor;
		item.selrcolor = selrcolor;
		item.selbcolor = selbcolor;
		item.data3 = file_info.uncompressed_size;
		win_menu_add(g_menu, &item);
	} while (unzGoToNextFile(unzf) == UNZ_OK);

	unzClose(unzf);
	freq_leave(fid);

	return g_menu->size;
}

extern u32 fs_rar_to_menu(const char *rarfile, u32 icolor, u32 selicolor, u32 selrcolor, u32 selbcolor)
{
	int fid;
	struct RAROpenArchiveData arcdata;
	struct RARHeaderDataEx header;
	int ret;
	HANDLE hrar;
	t_fs_filetype ft;
	t_win_menuitem item;

	if (menu_renew(&g_menu) == NULL) {
		return 0;
	}

	fid = freq_enter_hotzone();

	arcdata.ArcName = (char *) rarfile;
	arcdata.OpenMode = RAR_OM_LIST;
	arcdata.CmtBuf = NULL;
	arcdata.CmtBufSize = 0;

	hrar = RAROpenArchive(&arcdata);

	if (hrar == 0) {
		freq_leave(fid);
		return 0;
	}

	add_parent_to_menu(g_menu, icolor, selicolor, selrcolor, selbcolor);

	do {
		char t[20];

		if ((ret = RARReadHeaderEx(hrar, &header)) != 0) {
			if (ret != ERAR_UNKNOWN)
				break;
			RARCloseArchive(hrar);
			if ((hrar = reopen_rar_with_passwords(&arcdata)) == 0)
				break;
			if (RARReadHeaderEx(hrar, &header) != 0)
				break;
		}
		if (header.UnpSize == 0)
			continue;

		ft = fs_file_get_type(header.FileName);

		if (ft == fs_filetype_chm || ft == fs_filetype_zip || ft == fs_filetype_rar)
			continue;

		win_menuitem_new(&item);
		item.data = (void *) ft;

		if (header.Flags & 0x200) {
			char str[1024];
			const u8 *uni;

			memset(str, 0, 1024);
			uni = (u8 *) header.FileNameW;
			charsets_utf32_conv(uni, sizeof(header.FileNameW), (u8 *) str, sizeof(str));
			buffer_copy_string_len(item.compname, header.FileName, 256);
			filename_to_itemname(&item, str);
		} else {
			buffer_copy_string_len(item.compname, header.FileName, 256);
			filename_to_itemname(&item, header.FileName);
		}

		SPRINTF_S(t, "%u", (unsigned int) header.UnpSize);
		buffer_copy_string(item.shortname, t);
		item.selected = false;
		item.icolor = icolor;
		item.selicolor = selicolor;
		item.selrcolor = selrcolor;
		item.selbcolor = selbcolor;
		item.data3 = header.UnpSize;
		win_menu_add(g_menu, &item);
	} while (RARProcessFile(hrar, RAR_SKIP, NULL, NULL) == 0);

	RARCloseArchive(hrar);
	freq_leave(fid);

	return g_menu->size;
}

u32 fs_empty_dir(u32 icolor, u32 selicolor, u32 selrcolor, u32 selbcolor)
{
	if (menu_renew(&g_menu) == NULL) {
		return 0;
	}

	add_parent_to_menu(g_menu, icolor, selicolor, selrcolor, selbcolor);

	return g_menu->size;
}

typedef struct
{
	u32 icolor;
	u32 selicolor;
	u32 selrcolor;
	u32 selbcolor;
} t_fs_chm_enum, *p_fs_chm_enum;

static int chmEnum(struct chmFile *h, struct chmUnitInfo *ui, void *context)
{
	t_win_menuitem item;
	t_fs_filetype ft;
	char fname[PATH_MAX] = "";
	char t[20];

	int size = strlen(ui->path);

	if (size == 0 || ui->path[size - 1] == '/')
		return CHM_ENUMERATOR_CONTINUE;

	ft = fs_file_get_type(ui->path);

	if (ft == fs_filetype_chm || ft == fs_filetype_zip || ft == fs_filetype_rar || ft == fs_filetype_umd || ft == fs_filetype_pdb)
		return CHM_ENUMERATOR_CONTINUE;

	win_menuitem_new(&item);
	buffer_copy_string(item.compname, ui->path);
	SPRINTF_S(t, "%u", (unsigned int) ui->length);
	buffer_copy_string(item.shortname, t);
	if (ui->path[0] == '/') {
		strncpy_s(fname, NELEMS(fname), ui->path + 1, 256);
	} else {
		strncpy_s(fname, NELEMS(fname), ui->path, 256);
	}

	charsets_utf8_conv((unsigned char *) fname, sizeof(fname), (unsigned char *) fname, sizeof(fname));

	item.data = (void *) ft;
	filename_to_itemname(&item, fname);
	item.selected = false;
	item.icolor = ((p_fs_chm_enum) context)->icolor;
	item.selicolor = ((p_fs_chm_enum) context)->selicolor;
	item.selrcolor = ((p_fs_chm_enum) context)->selrcolor;
	item.selbcolor = ((p_fs_chm_enum) context)->selbcolor;
	item.data3 = ui->length;
	win_menu_add(g_menu, &item);

	return CHM_ENUMERATOR_CONTINUE;
}

extern u32 fs_chm_to_menu(const char *chmfile, u32 icolor, u32 selicolor, u32 selrcolor, u32 selbcolor)
{
	int fid;
	struct chmFile *chm;
	t_fs_chm_enum cenum;

	if (menu_renew(&g_menu) == NULL) {
		return 0;
	}

	fid = freq_enter_hotzone();
	chm = chm_open(chmfile);

	if (chm == NULL) {
		freq_leave(fid);
		return 0;
	}

	add_parent_to_menu(g_menu, icolor, selicolor, selrcolor, selbcolor);

	cenum.icolor = icolor;
	cenum.selicolor = selicolor;
	cenum.selrcolor = selrcolor;
	cenum.selbcolor = selbcolor;
	chm_enumerate(chm, CHM_ENUMERATE_NORMAL | CHM_ENUMERATE_FILES, chmEnum, (void *) &cenum);
	chm_close(chm);
	freq_leave(fid);

	return g_menu->size;
}

extern u32 fs_umd_to_menu(const char *umdfile, u32 icolor, u32 selicolor, u32 selrcolor, u32 selbcolor)
{
	buffer *pbuf = NULL;
	u32 cur_count = 1;
	int fid;
	t_win_menuitem item;

	if (menu_renew(&g_menu) == NULL) {
		return 0;
	}

	fid = freq_enter_hotzone();

	add_parent_to_menu(g_menu, icolor, selicolor, selrcolor, selbcolor);

	do {
		size_t stlen = 0;
		u_int i = 1;
		struct t_chapter *p;
		char pos[20] = { 0 };

		if (!p_umdchapter || (p_umdchapter->umdfile->ptr && strcmp(p_umdchapter->umdfile->ptr, umdfile))) {
			if (p_umdchapter)
				umd_chapter_reset(p_umdchapter);
			p_umdchapter = umd_chapter_init();
			if (!p_umdchapter)
				break;
			buffer_copy_string(p_umdchapter->umdfile, umdfile);
			cur_count = parse_umd_chapters(umdfile, &p_umdchapter);
		} else
			cur_count = p_umdchapter->chapter_count + 1;

		if (cur_count > 1) {
			p = p_umdchapter->pchapters;
			pbuf = buffer_init();

			if (!pbuf || buffer_prepare_copy(pbuf, 256) < 0)
				break;

			for (i = 1; i < cur_count; i++) {
				stlen = p[i - 1].name->used - 1;
				stlen = charsets_ucs_conv((const u8 *) p[i - 1].name->ptr, stlen, (u8 *) pbuf->ptr, pbuf->size);
				SPRINTF_S(pos, "%d", p[i - 1].length);
				win_menuitem_new(&item);
				buffer_copy_string_len(item.shortname, pos, 20);
				buffer_copy_string_len(item.compname, pbuf->ptr, (stlen > 256) ? 256 : stlen);
				filename_to_itemname(&item, item.compname->ptr);
				if (1 != p_umdchapter->umd_type) {
					if (0 == p_umdchapter->umd_mode)
						item.data = (void *) fs_filetype_bmp;
					else if (1 == p_umdchapter->umd_mode)
						item.data = (void *) fs_filetype_jpg;
					else
						item.data = (void *) fs_filetype_gif;
				} else
					item.data = (void *) fs_filetype_txt;
				item.data2[0] = p[i - 1].chunk_pos & 0xFFFF;
				item.data2[1] = (p[i - 1].chunk_pos >> 16) & 0xFFFF;
				item.data2[2] = p[i - 1].chunk_offset & 0xFFFF;
				item.data2[3] = (p[i - 1].chunk_offset >> 16) & 0xFFFF;
				item.data3 = p[i - 1].length;
#if 0
				printf("%d pos:%d,%d,%d-%d,%d\n", i, p[i - 1].chunk_pos, item.data2[0], item.data2[1], item.data2[2], item.data2[3]);
#endif
				item.selected = false;
				item.icolor = icolor;
				item.selicolor = selicolor;
				item.selrcolor = selrcolor;
				item.selbcolor = selbcolor;
				//buffer_free(p[i - 1].name);
				win_menu_add(g_menu, &item);
			}
#if 0
			printf("%s umd file:%s type:%d,mode:%d,chapter count:%ld\n", __func__, umdfile, p_umdchapter->umd_type, p_umdchapter->umd_mode, cur_count);
#endif
		}
	} while (false);

	if (pbuf)
		buffer_free(pbuf);

	freq_leave(fid);

	return g_menu->size;
}

extern t_fs_filetype fs_file_get_type(const char *filename)
{
	const char *ext = utils_fileext(filename);
	t_fs_filetype_entry *entry = ft_table;
	t_fs_specfiletype_entry *entry2 = ft_spec_table;

	if (ext) {
		while (entry->ext != NULL) {
			if (stricmp(ext, entry->ext) == 0)
				return entry->ft;
			entry++;
		}
	}

	while (entry2->fname != NULL) {
		const char *shortname = strrchr(filename, '/');

		if (!shortname)
			shortname = filename;
		else
			shortname++;
		if (stricmp(shortname, entry2->fname) == 0)
			return entry2->ft;
		entry2++;
	}

#ifdef ENABLE_MUSIC
	if (fs_is_music(filename, filename)) {
		return fs_filetype_music;
	}
#endif

	return fs_filetype_unknown;
}

#ifdef ENABLE_IMAGE
extern bool fs_is_image(t_fs_filetype ft)
{
	return ft == fs_filetype_jpg || ft == fs_filetype_gif || ft == fs_filetype_png || ft == fs_filetype_tga || ft == fs_filetype_bmp;
}
#endif

extern bool fs_is_txtbook(t_fs_filetype ft)
{
	return ft == fs_filetype_txt || ft == fs_filetype_html || ft == fs_filetype_gz;
}

#ifdef ENABLE_MUSIC
extern bool fs_is_music(const char *spath, const char *lpath)
{
	bool res;

	res = musicdrv_chk_file(spath) != NULL ? true : false;

	if (res)
		return res;

	res = musicdrv_chk_file(lpath) != NULL ? true : false;

	return res;
}
#endif
