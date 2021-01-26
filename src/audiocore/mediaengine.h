#ifndef MEDIAENGINE_H
#define MEDIAENGINE_H

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


enum CodecType {
	VAUDIO  =       0x01,
	AVCODEC =       0x01 << 1,
	ATRAC3PLUS =    0x01 << 2,
	COOLEYEBRIDGE = 0x01 << 3,
	ASFPARSER =     0x01 << 4
};

int load_me_prx(int mode);
int unload_prx(void);

#endif
