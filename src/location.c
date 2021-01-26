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

#include <string.h>
#include <pspkernel.h>
#include "location.h"
#include "common/utils.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

static char fn[PATH_MAX];
static bool slot[10];

struct _location
{
	char comppath[PATH_MAX];
	char shortpath[PATH_MAX];
	char compname[PATH_MAX];
	char name[PATH_MAX];
	bool isreading;
} __attribute__ ((packed));
typedef struct _location t_location;

extern void location_init(const char *filename, int *slotaval)
{
	int fd;

	STRCPY_S(fn, filename);
	memset(slot, 0, sizeof(bool) * 10);
	fd = sceIoOpen(fn, PSP_O_RDONLY, 0777);

	if (fd < 0) {
		u8 tempdata[sizeof(t_location) * 10 + sizeof(bool) * 10];

		if ((fd = sceIoOpen(fn, PSP_O_CREAT | PSP_O_RDWR, 0777)) < 0)
			return;

		memset(tempdata, 0, sizeof(t_location) * 10 + sizeof(bool) * 10);
		sceIoWrite(fd, tempdata, sizeof(t_location) * 10 + sizeof(bool) * 10);
	}
	sceIoRead(fd, slot, sizeof(bool) * 10);
	memcpy(slotaval, slot, sizeof(bool) * 10);
	sceIoClose(fd);
}

extern bool location_enum(t_location_enum_func func, void *data)
{
	int fd = sceIoOpen(fn, PSP_O_RDONLY, 0777);
	int i;

	if (fd < 0)
		return false;

	if (sceIoLseek32(fd, sizeof(bool) * 10, PSP_SEEK_SET) != sizeof(bool) * 10) {
		sceIoClose(fd);
		return false;
	}
	for (i = 0; i < 10; i++) {
		t_location l;

		memset(&l, 0, sizeof(t_location));
		if (sceIoRead(fd, &l, sizeof(t_location)) != sizeof(t_location))
			break;
		if (slot[i])
			func(i, l.comppath, l.shortpath, l.compname, l.name, l.isreading, data);
	}
	sceIoClose(fd);
	return true;
}

extern bool location_get(u32 index, char *comppath, char *shortpath, char *compname, char *name, bool * isreading)
{
	int fd;
	t_location l;

	if (!slot[index])
		return false;

	fd = sceIoOpen(fn, PSP_O_RDONLY, 0777);

	if (fd < 0)
		return false;
	if (sceIoLseek32(fd, sizeof(t_location) * index + sizeof(bool) * 10, PSP_SEEK_SET) != sizeof(t_location) * index + sizeof(bool) * 10) {
		sceIoClose(fd);
		return false;
	}

	memset(&l, 0, sizeof(t_location));
	sceIoRead(fd, &l, sizeof(t_location));
	sceIoClose(fd);
	strcpy_s(comppath, PATH_MAX, l.comppath);
	strcpy_s(shortpath, PATH_MAX, l.shortpath);
	strcpy_s(compname, PATH_MAX, l.compname);
	strcpy_s(name, PATH_MAX, l.name);
	*isreading = l.isreading;

	return true;
}

extern bool location_set(u32 index, char *comppath, char *shortpath, char *compname, char *name, bool isreading)
{
	int fd;
	u32 pos;
	t_location t;

	if ((fd = sceIoOpen(fn, PSP_O_RDWR, 0777)) < 0 && (fd = sceIoOpen(fn, PSP_O_CREAT | PSP_O_RDWR, 0777)) < 0)
		return false;

	pos = sceIoLseek32(fd, sizeof(t_location) * index + sizeof(bool) * 10, PSP_SEEK_SET);

	if (pos < sizeof(t_location) * index + sizeof(bool) * 10) {
		u8 tempdata[sizeof(t_location) * 10 + sizeof(bool) * 10 - pos];

		memset(tempdata, 0, sizeof(t_location) * 10 + sizeof(bool) * 10 - pos);
		sceIoWrite(fd, tempdata, sizeof(t_location) * 10 + sizeof(bool) * 10 - pos);
		if (sceIoLseek32(fd, sizeof(t_location) * index + sizeof(bool) * 10, PSP_SEEK_SET) < sizeof(t_location) * index + sizeof(bool) * 10) {
			sceIoClose(fd);
			return false;
		}
	}

	memset(&t, 0, sizeof(t_location));
	STRCPY_S(t.comppath, comppath);
	STRCPY_S(t.shortpath, shortpath);
	STRCPY_S(t.compname, compname);
	STRCPY_S(t.name, name);
	t.isreading = isreading;
	sceIoWrite(fd, &t, sizeof(t_location));
	sceIoLseek32(fd, sizeof(bool) * index, PSP_SEEK_SET);
	slot[index] = true;
	sceIoWrite(fd, &slot[index], sizeof(bool));
	sceIoClose(fd);
	return true;
}
