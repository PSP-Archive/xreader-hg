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

#ifndef _SCENE_H_
#define _SCENE_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include "common/datatype.h"

	extern void scene_init(void);
	extern void scene_exit(void);
	extern const char *scene_appdir(void);

	extern u32 get_bgcolor_by_time(void);

	enum SceneWhere
	{
		scene_in_dir,
		scene_in_zip,
		scene_in_umd,
		scene_in_chm,
		scene_in_rar
	};

	extern enum SceneWhere where;

	typedef struct
	{
		int size;
		bool zipped;
	} t_fonts;

	extern int freq_list[][2];

	extern int psp_model;
	extern int psp_fw_version;
	void debug_malloc(void);

#ifdef DMALLOC
	extern unsigned dmark;
#endif

	void debug_malloc(void);

#ifdef __cplusplus
}
#endif

#endif
