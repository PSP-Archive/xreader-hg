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

#ifndef _BOOKMARK_H_
#define _BOOKMARK_H_

#include "common/datatype.h"

struct _bookmark
{
	u32 row[10];
	u32 index, hash;
} __attribute__ ((packed));
typedef struct _bookmark t_bookmark, *p_bookmark;

extern void bookmark_init(const char *fn);
extern p_bookmark bookmark_open(const char *filename);
extern void bookmark_save(p_bookmark bm);
extern void bookmark_delete(p_bookmark bm);
extern void bookmark_close(p_bookmark bm);
extern bool bookmark_export(p_bookmark bm, const char *filename);
extern bool bookmark_import(const char *filename);

#endif
