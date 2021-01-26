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

#include <pspkernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common/utils.h"
#include "copy.h"
#include "fs.h"
#include "dbg.h"
#include "archive.h"
#include "buffer.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

extern bool copy_file(const char *src, const char *dest, t_copy_cb cb, t_copy_overwritecb ocb, void *data)
{
	u8 *buf = malloc(1024 * 1024);
	int fd1, fd2;
	int readbytes;

	if (buf == NULL)
		return false;

	if (ocb != NULL) {
		fd2 = sceIoOpen(dest, PSP_O_RDONLY, 0777);
		if (fd2 >= 0) {
			if (!ocb(dest, data)) {
				sceIoClose(fd2);
				return false;
			}
			sceIoClose(fd2);
		}
	}

	fd1 = sceIoOpen(src, PSP_O_RDONLY, 0777);

	if (fd1 < 0) {
		if (cb != NULL)
			cb(src, dest, false, data);
		return false;
	}
	fd2 = sceIoOpen(dest, PSP_O_CREAT | PSP_O_RDWR, 0777);
	if (fd2 < 0) {
		if (cb != NULL)
			cb(src, dest, false, data);
		sceIoClose(fd1);
		return false;
	}

	while ((readbytes = sceIoRead(fd1, buf, 1024 * 1024)) > 0)
		if (sceIoWrite(fd2, buf, readbytes) != readbytes) {
			if (cb != NULL)
				cb(src, dest, false, data);
			sceIoClose(fd1);
			sceIoClose(fd2);
			return true;
		}
	free(buf);
	if (cb != NULL)
		cb(src, dest, true, data);
	sceIoClose(fd1);
	sceIoClose(fd2);
	return true;
}

extern u32 copy_dir(const char *src, const char *dest, t_copy_cb cb, t_copy_overwritecb ocb, void *data)
{
	int dl = sceIoDopen(src);
	u32 result = 0;
	SceIoDirent sid;

	if (dl < 0) {
		if (cb != NULL)
			cb(src, dest, false, data);
		return 0;
	}

	sceIoMkdir(dest, 0777);
	memset(&sid, 0, sizeof(SceIoDirent));

	while (sceIoDread(dl, &sid)) {
		char copysrc[260], copydest[260];

		if (sid.d_name[0] == '.')
			continue;

		SPRINTF_S(copysrc, "%s/%s", src, sid.d_name);
		SPRINTF_S(copydest, "%s/%s", dest, sid.d_name);
		if (FIO_S_ISDIR(sid.d_stat.st_mode)) {
			result += copy_dir(copysrc, copydest, cb, ocb, data);
			continue;
		}
		if (copy_file(copysrc, copydest, cb, ocb, data))
			++result;
		memset(&sid, 0, sizeof(SceIoDirent));
	}
	sceIoDclose(dl);
	return result;
}

extern bool move_file(const char *src, const char *dest, t_copy_cb cb, t_copy_overwritecb ocb, void *data)
{
	bool result = copy_file(src, dest, cb, ocb, data);

	if (result)
		utils_del_file(src);
	return result;
}

extern u32 move_dir(const char *src, const char *dest, t_copy_cb cb, t_copy_overwritecb ocb, void *data)
{
	u32 result = copy_dir(src, dest, cb, ocb, data);

	if (result > 0)
		utils_del_dir(src);
	return result;
}

static t_fs_filetype get_archive_type(const char *path)
{
	if (strlen(path) >= strlen(".rar")
		&& stricmp(path + strlen(path) - strlen(".rar"), ".rar") == 0)
		return fs_filetype_rar;

	if (strlen(path) >= strlen(".zip")
		&& stricmp(path + strlen(path) - strlen(".zip"), ".zip") == 0)
		return fs_filetype_zip;

	if (strlen(path) >= strlen(".chm")
		&& stricmp(path + strlen(path) - strlen(".chm"), ".chm") == 0)
		return fs_filetype_chm;

	if (strlen(path) >= strlen(".umd")
		&& stricmp(path + strlen(path) - strlen(".umd"), ".umd") == 0)
		return fs_filetype_umd;

	if (strlen(path) >= strlen(".pdb")
		&& stricmp(path + strlen(path) - strlen(".pdb"), ".pdb") == 0)
		return fs_filetype_pdb;

	return fs_filetype_unknown;
}

extern bool extract_archive_file(const char *archname, const char *archpath, const char *dest, t_copy_cb cb, t_copy_overwritecb ocb, void *data)
{
	t_fs_filetype ft;
	SceUID fd;
	bool result = false;
	buffer *archdata = NULL;
	int buffer_cache;
	char *ptr;

	if (archname == NULL || archpath == NULL || dest == NULL)
		return false;

	ft = get_archive_type(archname);

	if (ft == fs_filetype_unknown)
		return false;

	if (ocb != NULL) {
		SceUID fd;

		fd = sceIoOpen(dest, PSP_O_RDONLY, 0777);
		if (fd >= 0) {
			if (!ocb(dest, data)) {
				sceIoClose(fd);
				return false;
			}
			sceIoClose(fd);
		}
	}

	dbg_printf(d, "extract_archive_file: %s %s %s, ft = %d", archname, archpath, dest, ft);

	fd = sceIoOpen(dest, PSP_O_CREAT | PSP_O_RDWR, 0777);

	if (fd < 0)
		return false;

	extract_archive_file_into_buffer(&archdata, archname, archpath, ft);

	if (archdata == NULL || archdata->ptr == NULL)
		goto exit;

	buffer_cache = archdata->used >= 1024 * 1024 ? 1024 * 1024 : archdata->used;

	ptr = archdata->ptr;

	while (buffer_cache > 0) {
		int bytes = sceIoWrite(fd, ptr, buffer_cache);

		if (bytes < 0) {
			goto exit;
		}
		buffer_cache = archdata->used - bytes >= 1024 * 1024 ? 1024 * 1024 : archdata->used - bytes;
		ptr += bytes;
	}

	result = true;

  exit:
	sceIoClose(fd);
	if (archdata != NULL) {
		buffer_free(archdata);
	}

	return result;
}
