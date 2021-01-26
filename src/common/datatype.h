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

#ifndef _DATATYPE_H_
#define _DATATYPE_H_

#include <psptypes.h>

#ifndef bool
#ifndef __cplusplus
typedef unsigned char bool;
#endif
#endif

#ifndef true
#ifndef __cplusplus
#define true 1
#endif
#endif

#ifndef false
#ifndef __cplusplus
#define false 0
#endif
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

#ifndef null
#define null ((void *)0)
#endif

#ifndef min
#ifndef __cplusplus
#define min(a,b) (((a)<(b))?(a):(b))
#endif
#endif

#ifndef max
#ifndef __cplusplus
#define max(a,b) (((a)>(b))?(a):(b))
#endif
#endif

#ifndef INVALID
#define INVALID ((u32)-1)
#endif

#define PATH_MAX 1024

#endif
