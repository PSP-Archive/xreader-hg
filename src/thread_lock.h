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

#ifndef THREAD_LOCK_H
#define THREAD_LOCK_H

struct psp_mutex_t
{
	volatile u32 l;
	int c;
	int thread_id;
};

int xr_lock(struct psp_mutex_t *e);
int xr_unlock(struct psp_mutex_t *e);
struct psp_mutex_t *xr_lock_init(struct psp_mutex_t *e);
void xr_lock_destroy(struct psp_mutex_t *e);

#endif
