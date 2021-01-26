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

#pragma once

#include "unrar.h"

typedef struct
{
	int idx, size;
	u8 *buf;
} t_image_rar, *p_image_rar;

extern void extract_archive_file_into_buffer(buffer ** buf, const char *archname, const char *archpath, t_fs_filetype filetype);

extern void extract_rar_file_into_image(t_image_rar * image, const char *archname, const char *archpath);

extern HANDLE reopen_rar_with_passwords(struct RAROpenArchiveData *arcdata);
