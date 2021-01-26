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
#include <pspusb.h>
#include "psp_utils.h"

extern int psp_load_start_module(const char *path)
{
	int mid;
	int result;

	mid = sceKernelLoadModule(path, 0, 0);

	if (mid == 0x80020139) {
		return 0;				// already loaded 
	}

	if (mid < 0) {
		return mid;
	}

	result = sceKernelStartModule(mid, 0, 0, 0, 0);
	if (result < 0) {
		return result;
	}

	return mid;
}
