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

#ifndef _CTRL_H_
#define _CTRL_H_

#include <pspctrl.h>
#ifdef ENABLE_HPRM
#include <psphprm.h>
#endif
#include "common/datatype.h"

#define CTRL_FORWARD 0x10000000
#define CTRL_BACK 0x20000000
#define CTRL_PLAYPAUSE 0x40000000
#ifdef ENABLE_ANALOG
#define CTRL_ANALOG 0x80000000
#endif

extern void ctrl_init(void);
extern void ctrl_destroy(void);

#ifdef ENABLE_ANALOG
extern bool ctrl_analog(int *x, int *y);
#endif
extern u32 ctrl_read_cont(void);
extern u32 ctrl_read(void);
extern void ctrl_waitreleaseintime(int i);
extern void ctrl_waitrelease(void);
extern u32 ctrl_waitany(void);
extern u32 ctrl_waitkey(u32 keymask);
extern u32 ctrl_waitmask(u32 keymask);
extern u32 ctrl_waitlyric(void);
extern int ctrl_waitreleasekey(u32 key);

#ifdef ENABLE_HPRM
extern u32 ctrl_hprm(void);
extern u32 ctrl_hprm_raw(void);
#endif
extern u32 ctrl_waittime(u32 t);

#ifdef ENABLE_HPRM
extern void ctrl_enablehprm(bool enable);
#endif

#endif
