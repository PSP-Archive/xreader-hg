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

#ifndef _POWER_H_
#define _POWER_H_

#include "common/datatype.h"

extern void power_set_clock(u32 cpu, u32 bus);
extern void power_get_clock(u32 * cpu, u32 * bus);
extern void power_get_battery(int *percent, int *lifetime, int *tempe, int *volt);
extern const char *power_get_battery_charging(void);
extern void power_down(void);
extern void power_up(void);

#endif
