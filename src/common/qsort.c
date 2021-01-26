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

#include <string.h>
#include "qsort.h"
#include "freq_lock.h"
#include "scene.h"

__inline void swap_data(void *data1, void *data2, int datasize)
{
	u8 temp[datasize];

	memcpy(temp, data1, datasize);
	memcpy(data1, data2, datasize);
	memcpy(data2, temp, datasize);
}

void _quicksort(void *data, int left, int right, int datasize,
				qsort_compare compare)
{
	int i, last;
	u8(*qdata)[datasize];

	if (left >= right)
		return;

	qdata = (u8(*)[datasize]) data;
	if (right - left > 1)
		swap_data(&qdata[left], &qdata[(left + right) / 2], datasize);
	last = left;
	for (i = left + 1; i <= right; i++)
		if (compare(&qdata[i], &qdata[left]) < 0)
			swap_data(&qdata[++last], &qdata[i], datasize);
	swap_data(&qdata[left], &qdata[last], datasize);
	_quicksort(data, left, last - 1, datasize, compare);
	_quicksort(data, last + 1, right, datasize, compare);
}

extern void quicksort(void *data, int left, int right, int datasize,
					  qsort_compare compare)
{
	int fid;

	fid = freq_enter_hotzone();
	_quicksort(data, left, right, datasize, compare);
	freq_leave(fid);
}
