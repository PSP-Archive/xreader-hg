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

#ifndef XR_PRX
#define XR_PRX

#ifdef __cplusplus
extern "C"
{
#endif

	int xrKernelInitApitype(void);
	SceUID xrIoOpen(const char *file, int flags, SceMode mode);
	SceOff xrIoLseek(SceUID fd, SceOff offset, int whence);
	int xrIoRead(SceUID fd, void *data, SceSize size);
	int xrIoClose(SceUID fd);
	void xrDisplaySetBrightness(int brightness, int unk1);
	void xrDisplayGetBrightness(int *brightness, int *unk1);
	int xrDisplaySetMaxBrightness(int newlevel);

#ifdef __cplusplus
}
#endif

#endif
