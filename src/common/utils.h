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

#ifndef _UTILS_H_
#define _UTILS_H_

#include "strsafe.h"

#include "datatype.h"

#ifdef __cplusplus
extern "C"
{
#endif

	extern u32 utils_dword2string(u32 dw, char *dest, u32 width);
	extern bool utils_string2dword(const char *src, u32 * dw);
	extern bool utils_string2double(const char *src, double *db);
	extern const char *utils_fileext(const char *filename);
	extern bool utils_del_file(const char *file);
	extern u32 utils_del_dir(const char *dir);
	bool utils_is_file_exists(const char *filename);
	void *safe_realloc(void *ptr, size_t size);
	extern unsigned int get_free_mem(void);
	void *calloc_64(size_t nmemb, size_t size);

#define UNUSED(x) ((void)(x))

#ifdef __cplusplus
}
#endif

#endif
