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

#ifndef _BG_H_
#define _BG_H_

#include "common/datatype.h"
#include "fs.h"

extern void bg_load(const char *filename, const char *archname, pixel bgcolor, t_fs_filetype ft, u32 grayscale, int where);
extern bool bg_display(void);
extern void bg_cancel(void);
extern void bg_cache(void);
extern void bg_restore(void);

#endif
