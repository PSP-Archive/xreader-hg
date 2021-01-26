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

#ifndef _LOCATION_H_
#define _LOCATION_H_

#include "common/datatype.h"

typedef void (*t_location_enum_func) (u32 index, char *comppath, char *shortpath, char *compname, char *name, bool isreading, void *data);

extern void location_init(const char *filename, int *slotaval);
extern bool location_get(u32 index, char *comppath, char *shortpath, char *compname, char *name, bool * isreading);
extern bool location_set(u32 index, char *comppath, char *shortpath, char *compname, char *name, bool isreading);
extern bool location_enum(t_location_enum_func func, void *data);

#endif
