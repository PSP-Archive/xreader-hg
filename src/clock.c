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

#include <pspkernel.h>
#include <psppower.h>
#include <pspsdk.h>
#include "scene.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

int getCpuClock()
{
	return scePowerGetCpuClockFrequency();
}

void setBusClock(int bus)
{
	if (bus >= 54 && bus <= 111 && psp_fw_version < 0x03070110)
		scePowerSetBusClockFrequency(bus);
}

void sceSetCpuClock(int cpu, int bus)
{
	if (psp_fw_version < 0x03070110) {
		scePowerSetCpuClockFrequency(cpu);
		if (scePowerGetCpuClockFrequency() < cpu)
			scePowerSetCpuClockFrequency(++cpu);
	} else {
		scePowerSetClockFrequency(cpu, cpu, cpu / 2);
		if (scePowerGetCpuClockFrequency() < cpu) {
			cpu++;
			scePowerSetClockFrequency(cpu, cpu, cpu / 2);
		}
	}
}
