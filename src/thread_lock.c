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
#include <pspsdk.h>
#include <pspkernel.h>
#include "thread_lock.h"

#define CURRENT_THREAD sceKernelGetThreadId()

static u32 my_InterlockedExchange(u32 volatile *dst, u32 exchange)
{
	unsigned int flags = pspSdkDisableInterrupts();
	u32 origvalue = *dst;

	*dst = exchange;

	pspSdkEnableInterrupts(flags);
	return origvalue;
}

int xr_lock(struct psp_mutex_t *s)
{
	if (s == NULL) {
		return -1;
	}

	for (;;) {
		if (s->l != 0) {
			if (s->thread_id == CURRENT_THREAD) {
				++s->c;
				return 0;
			}
		} else {
			if (!my_InterlockedExchange(&s->l, 1)) {
				s->thread_id = CURRENT_THREAD;
				s->c = 1;
				return 0;
			}
		}

		sceKernelDelayThread(1000);
	}

	return -1;
}

int xr_unlock(struct psp_mutex_t *s)
{
	if (s == NULL) {
		return -1;
	}

	if (--s->c == 0) {
		s->thread_id = 0;
		my_InterlockedExchange(&s->l, 0);
	}

	return 0;
}

struct psp_mutex_t *xr_lock_init(struct psp_mutex_t *s)
{
	if (s == NULL) {
		return s;
	}

	s->l = 0;
	s->c = 0;
	s->thread_id = CURRENT_THREAD;

	return s;
}

void xr_lock_destroy(struct psp_mutex_t *s)
{
	if (s == NULL) {
		return;
	}

	s->l = 0;
	s->c = 0;
	s->thread_id = 0;
}
