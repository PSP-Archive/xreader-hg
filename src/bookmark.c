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

#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <pspkernel.h>
#include "common/utils.h"
#include "fat.h"
#include "bookmark.h"
#include <stdio.h>
#include <psptypes.h>
#include "dbg.h"
#include "scene.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

struct _bm_index
{
	u32 flag;
	u32 hash[32];
} __attribute__ ((packed));
typedef struct _bm_index t_bm_index, *p_bm_index;
static u32 flagbits[32] = {
	0x00000001, 0x00000002, 0x00000004, 0x00000008, 0x00000010, 0x00000020,
	0x00000040, 0x00000080,
	0x00000100, 0x00000200, 0x00000400, 0x00000800, 0x00001000, 0x00002000,
	0x00004000, 0x00008000,
	0x00010000, 0x00020000, 0x00040000, 0x00080000, 0x00100000, 0x00200000,
	0x00400000, 0x00800000,
	0x01000000, 0x02000000, 0x04000000, 0x08000000, 0x10000000, 0x20000000,
	0x40000000, 0x80000000,
};

static char bmfile[PATH_MAX];
static t_bookmark g_oldbm;

extern u32 bookmark_encode(const char *filename)
{
	register u32 h;

	for (h = 5381; *filename != 0; ++filename) {
		h += h << 5;
		h ^= *(const unsigned char *) filename;
	}

	return h;
}

extern void bookmark_init(const char *fn)
{
	STRCPY_S(bmfile, fn);
}

static p_bookmark bookmark_open_hash(u32 hash)
{
	int fd;
	u32 count;
	p_bookmark bm = calloc(1, sizeof(*bm));
	t_bm_index bi;
	u32 i, j;

	if (bm == NULL)
		return NULL;
	memset(bm->row, 0xFF, 10 * sizeof(u32));
	bm->index = INVALID;
	bm->hash = hash;

	fd = sceIoOpen(bmfile, PSP_O_RDONLY, 0777);

	if (fd < 0)
		return bm;

	if (sceIoRead(fd, &count, sizeof(u32)) < sizeof(u32)) {
		sceIoClose(fd);
		return bm;
	}

	for (i = 0; i < count; i++) {
		if (sceIoRead(fd, &bi, sizeof(t_bm_index)) < sizeof(t_bm_index)) {
			sceIoClose(fd);
			return bm;
		}
		for (j = 0; j < 32; j++)
			if ((bi.flag & flagbits[j]) > 0 && bi.hash[j] == bm->hash) {
				u32 cur_pos;

				bm->index = i * 32 + j;
				sceIoLseek(fd, j * 10 * sizeof(u32), PSP_SEEK_CUR);
				cur_pos = sceIoLseek(fd, 0, PSP_SEEK_CUR);
				dbg_printf(d, "%s: Reading bookmark at 0x%08X", __func__, cur_pos);
				sceIoRead(fd, &bm->row[0], 10 * sizeof(u32));
				sceIoClose(fd);
				return bm;
			}
		sceIoLseek(fd, 32 * 10 * sizeof(u32), PSP_SEEK_CUR);
	}
	sceIoClose(fd);
	return bm;
}

static void cache_invalidate_hack(void)
{
	int fd;
	int t[20];
	char conffile[PATH_MAX];

	SPRINTF_S(conffile, "%s%s%d%s", scene_appdir(), "config", 0, ".ini");

	fd = sceIoOpen(conffile, PSP_O_RDONLY, 0777);

	if (fd >= 0) {
		sceIoRead(fd, t, sizeof(t));
		sceIoClose(fd);
	}
}

extern p_bookmark bookmark_open(const char *filename)
{
	p_bookmark bm;

	if (!filename)
		return NULL;

	cache_invalidate_hack();
	bm = bookmark_open_hash(bookmark_encode(filename));
	memcpy(&g_oldbm, bm, sizeof(g_oldbm));
	return bm;
}

extern void bookmark_save(p_bookmark bm)
{
	int fd;
	u32 count;
	t_bm_index bi;

	if (!bm)
		return;

	if (!memcmp(&g_oldbm, bm, sizeof(g_oldbm)))
		return;

	fd = sceIoOpen(bmfile, PSP_O_CREAT | PSP_O_RDWR, 0777);

	if (fd < 0)
		return;

	if (sceIoRead(fd, &count, sizeof(u32)) < sizeof(u32)) {
		u32 *temp;
		u32 cur_pos;

		count = 1;
		sceIoLseek(fd, 0, PSP_SEEK_SET);
		sceIoWrite(fd, &count, sizeof(u32));
		memset(&bi, 0, sizeof(t_bm_index));
		sceIoWrite(fd, &bi, sizeof(t_bm_index));
		temp = calloc(32 * 10, sizeof(*temp));

		memset(temp, 0, 32 * 10 * sizeof(u32));
		cur_pos = sceIoLseek(fd, 0, PSP_SEEK_CUR);

		dbg_printf(d, "%s: Writing bookmark at 0x%08X", __func__, cur_pos);
		sceIoWrite(fd, temp, 32 * 10 * sizeof(u32));
		free(temp);
	}
	if (bm->index == INVALID) {
		sceIoLseek(fd, sizeof(u32) + (count - 1) * (sizeof(t_bm_index) + 32 * 10 * sizeof(u32)), PSP_SEEK_SET);
		sceIoRead(fd, &bi, sizeof(t_bm_index));
		if (bi.flag != 0xFFFFFFFF) {
			u32 j;

			for (j = 0; j < 32; j++)
				if ((bi.flag & flagbits[j]) == 0) {
					u32 cur_pos;

					bi.flag |= flagbits[j];
					bi.hash[j] = bm->hash;
					bm->index = (count - 1) * 32 + j;
					sceIoLseek(fd, sizeof(u32) + (count - 1) * (sizeof(t_bm_index) + 32 * 10 * sizeof(u32)), PSP_SEEK_SET);
					sceIoWrite(fd, &bi, sizeof(t_bm_index));
					sceIoLseek(fd, j * 10 * sizeof(u32), PSP_SEEK_CUR);
					cur_pos = sceIoLseek(fd, 0, PSP_SEEK_CUR);

					dbg_printf(d, "%s: Writing bookmark at 0x%08X", __func__, cur_pos);
					sceIoWrite(fd, &bm->row[0], 10 * sizeof(u32));
					break;
				}
		} else {
			u32 cur_pos;
			u32 *temp;

			sceIoLseek(fd, sizeof(u32) + count * (sizeof(t_bm_index) + 32 * 10 * sizeof(u32)), PSP_SEEK_SET);
			memset(&bi, 0, sizeof(t_bm_index));
			bi.flag = 1;
			bi.hash[0] = bm->hash;
			bm->index = count * 32;
			sceIoWrite(fd, &bi, sizeof(t_bm_index));
			cur_pos = sceIoLseek(fd, 0, PSP_SEEK_CUR);

			dbg_printf(d, "%s: Writing bookmark at 0x%08X", __func__, cur_pos);
			sceIoWrite(fd, &bm->row[0], 10 * sizeof(u32));
			temp = calloc(31 * 10, sizeof(*temp));

			memset(temp, 0, 31 * 10 * sizeof(u32));
			sceIoWrite(fd, temp, 31 * 10 * sizeof(u32));
			free(temp);
			sceIoLseek(fd, 0, PSP_SEEK_SET);
			count++;
			sceIoWrite(fd, &count, sizeof(u32));
		}
	} else {
		u32 cur_pos;
		int ret;

		sceIoLseek(fd,
				   sizeof(u32) +
				   (bm->index / 32) * (sizeof(t_bm_index) + 32 * 10 * sizeof(u32)) + sizeof(t_bm_index) + ((bm->index % 32) * 10 * sizeof(u32)), PSP_SEEK_SET);

		cur_pos = sceIoLseek(fd, 0, PSP_SEEK_CUR);

		dbg_printf(d, "%s: Writing bookmark at 0x%08X", __func__, cur_pos);

		ret = sceIoWrite(fd, &bm->row[0], 10 * sizeof(u32));

		if (ret < 0) {
			dbg_printf(d, "%s: writing failed %08x", __func__, ret);
		}
	}

	sceIoClose(fd);
}

extern void bookmark_delete(p_bookmark bm)
{
	int fd = sceIoOpen(bmfile, PSP_O_RDWR, 0777);
	t_bm_index bi;

	if (fd < 0)
		return;
	sceIoLseek(fd, sizeof(u32) + (bm->index / 32) * (sizeof(t_bm_index) + 32 * 10 * sizeof(u32)), PSP_SEEK_SET);

	memset(&bi, 0, sizeof(t_bm_index));
	sceIoRead(fd, &bi, sizeof(t_bm_index));
	bi.flag &= ~flagbits[bm->index % 32];
	bi.hash[bm->index % 32] = 0;
	sceIoLseek(fd, sizeof(u32) + (bm->index / 32) * (sizeof(t_bm_index) + 32 * 10 * sizeof(u32)), PSP_SEEK_SET);
	sceIoWrite(fd, &bi, sizeof(t_bm_index));
	sceIoClose(fd);
}

extern void bookmark_close(p_bookmark bm)
{
	if (bm != NULL)
		free(bm);
}

extern bool bookmark_export(p_bookmark bm, const char *filename)
{
	int fd;

	if (!bm || !filename)
		return false;

	fd = sceIoOpen(filename, PSP_O_CREAT | PSP_O_WRONLY, 0777);

	if (fd < 0)
		return false;

	sceIoWrite(fd, &bm->hash, sizeof(u32));
	sceIoWrite(fd, &bm->row[0], 10 * sizeof(u32));
	sceIoClose(fd);
	return true;
}

extern bool bookmark_import(const char *filename)
{
	int fd;
	u32 hash;
	p_bookmark bm = NULL;

	if (!filename)
		return false;

	fd = sceIoOpen(filename, PSP_O_RDONLY, 0777);

	if (fd < 0)
		return false;

	if (sceIoRead(fd, &hash, sizeof(u32)) < sizeof(u32)) {
		sceIoClose(fd);
		return false;
	}

	if ((bm = bookmark_open_hash(hash)) == NULL) {
		sceIoClose(fd);
		return false;
	}
	memset(bm->row, 0xFF, 10 * sizeof(u32));
	sceIoRead(fd, &bm->row[0], 10 * sizeof(u32));
	bookmark_save(bm);
	bookmark_close(bm);
	sceIoClose(fd);
	return true;
}
