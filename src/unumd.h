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

#ifndef _UNUMD_
#define _UNUMD_
#include "common/datatype.h"
#include "buffer.h"
#include <stdio.h>

struct UMDHeaderData
{
	char Mark;
	unsigned short hdType;
	u8 Flag;
	u8 Length;
} __attribute__ ((packed));

struct UMDHeaderDataEx
{
	char Mark;
	unsigned int CheckNum;
	unsigned int Length;
} __attribute__ ((packed));

struct t_chapter
{
	size_t chunk_pos;
	size_t chunk_offset;
	size_t length;
	buffer *name;
} __attribute__ ((packed));

struct _t_umd_chapter
{
	buffer *umdfile;
	unsigned short umd_type;
	unsigned short umd_mode;
	unsigned int chapter_count;
	size_t filesize;
	size_t content_pos;
	struct t_chapter *pchapters;
	//buffer*           pcontents;
} __attribute__ ((packed));
typedef struct _t_umd_chapter t_umd_chapter, *p_umd_chapter;

//extern p_umd_chapter p_umdchapter;
extern p_umd_chapter umd_chapter_init();
extern void umd_chapter_reset(p_umd_chapter pchap);
extern void umd_chapter_free(p_umd_chapter pchap);
extern int umd_readdata(char **pf, buffer ** buf);
extern int umd_getchapter(char **pf, p_umd_chapter * pchapter);
extern int parse_umd_chapters(const char *umdfile, p_umd_chapter * pchapter);
extern int locate_umd_img(const char *umdfile, size_t file_offset, FILE ** fp);
extern int read_umd_chapter_content(const char *umdfile, u_int index, p_umd_chapter pchapter
									/*,size_t file_offset,size_t length */ ,
									buffer ** pbuf);
#endif
