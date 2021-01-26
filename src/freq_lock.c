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

#include <stdio.h>
#include <string.h>
#include <pspkernel.h>
#include <psppower.h>
#include "scene.h"
#include "freq_lock.h"
#include "common/utils.h"
#include "power.h"
#include "conf.h"
#include "audiocore/musicmgr.h"
#include "dbg.h"
#include "thread_lock.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

struct _freq_lock_entry
{
	unsigned short cpu, bus;
	int id;
};

typedef struct _freq_lock_entry freq_lock_entry;

static freq_lock_entry *freqs = NULL;
static int freqs_cnt = 0;
static struct psp_mutex_t freq_l;

int freq_init()
{
	xr_lock_init(&freq_l);

	if (freqs_cnt != 0 || freqs != NULL) {
		free(freqs);
		freqs = NULL;
		freqs_cnt = 0;
	}

	return 0;
}

int freq_free()
{
	xr_lock_destroy(&freq_l);

	return 0;
}

int freq_lock()
{
	return xr_lock(&freq_l);
}

int freq_unlock()
{
	return xr_unlock(&freq_l);
}

static int generate_id()
{
	int id;
	bool found;

	id = -1;

	do {
		int i;

		id++;
		found = false;

		for (i = 0; i < freqs_cnt; ++i) {
			if (id == freqs[i].id) {
				found = true;
				break;
			}
		}
	} while (id < 0 || found == true);

	return id;
}

int freq_add(int cpu, int bus)
{
	int id;

	id = generate_id();

	if (freqs_cnt == 0) {
		freqs_cnt++;
		freqs = malloc(sizeof(freqs[0]));
		freqs[0].cpu = cpu;
		freqs[0].bus = bus;
		freqs[0].id = id;
	} else {
		freqs = safe_realloc(freqs, sizeof(freqs[0]) * (freqs_cnt + 1));

		if (freqs == NULL)
			return -1;

		freqs[freqs_cnt].cpu = cpu;
		freqs[freqs_cnt].bus = bus;
		freqs[freqs_cnt].id = id;
		freqs_cnt++;
	}

	return id;
}

static int dump_freq(void)
{
	char t[128] = { 0 };
	int i;

	sprintf(t, "[ ");

	for (i = 0; i < freqs_cnt; ++i) {
		sprintf(t + strlen(t), "%d/%d ", freqs[i].cpu, freqs[i].bus);
	}

	STRCAT_S(t, "]");

	dbg_printf(d, "%s: Dump freqs table: %s", __func__, t);

	return 0;
}

int dbg_freq()
{
	freq_lock();
	dump_freq();
	freq_unlock();

	return 0;
}

static int update_freq(void)
{
	int i, max, maxsum;
	int cpu = 0, bus = 0;

	for (i = 0, max = 0, maxsum = -1; i < freqs_cnt; ++i) {
		if (maxsum < freqs[i].cpu + freqs[i].bus) {
			maxsum = freqs[i].cpu + freqs[i].bus;
			max = i;
		}
	}

	if (maxsum > 0) {
		cpu = freqs[max].cpu;
		bus = freqs[max].bus;
	}

	cpu = max(cpu, freq_list[config.freqs[0]][0]);
	cpu = min(333, cpu);
	bus = max(bus, freq_list[config.freqs[0]][1]);
	bus = min(166, bus);

//  dbg_printf(d, "%s: should set cpu/bus to %d/%d", __func__, cpu, bus);

	if (scePowerGetCpuClockFrequency() != cpu || scePowerGetBusClockFrequency() != bus) {
		power_set_clock(cpu, bus);
		{
//          dbg_printf(d, "%s: cpu: %d, bus: %d", __func__, scePowerGetCpuClockFrequency(), scePowerGetBusClockFrequency());
		}
	}

	return 0;
}

int freq_update(void)
{
	freq_lock();
	update_freq();
	freq_unlock();

	return 0;
}

int freq_enter(int cpu, int bus)
{
	int idx;

//  dbg_printf(d, "%s: enter %d/%d", __func__, cpu, bus);

	freq_lock();

	idx = freq_add(cpu, bus);

	if (idx < 0) {
		freq_unlock();
		return -1;
	}

	update_freq();

	freq_unlock();

	return idx;
}

int freq_leave(int freq_id)
{
//  dbg_printf(d, "%s: leave %d", __func__, freq_id);

	int idx;

	freq_lock();

	for (idx = 0; idx < freqs_cnt; ++idx) {
		if (freqs[idx].id == freq_id) {
//          dbg_printf(d, "%s: removing freqs: %d/%d", __func__, freqs[idx].cpu, freqs[idx].bus);
			memmove(&freqs[idx], &freqs[idx + 1], sizeof(freqs[0]) * (freqs_cnt - idx - 1));
			freqs = safe_realloc(freqs, sizeof(freqs[0]) * (freqs_cnt - 1));
			freqs_cnt--;
			break;
		}
	}

	update_freq();

	freq_unlock();

	return 0;
}

int freq_enter_hotzone(void)
{
	int cpu, bus;

	cpu = freq_list[config.freqs[1]][0];
	bus = freq_list[config.freqs[1]][1];

	return freq_enter(cpu, bus);
}
