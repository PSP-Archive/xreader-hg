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

#ifndef IMAGE_QUEUE_H
#define IMAGE_QUEUE_H

enum
{
	CACHE_INIT = 0,
	CACHE_OK = 1,
	CACHE_FAILED = 2
};

typedef struct _cache_image_t
{
	const char *archname;
	const char *filename;
	int where;
	int status;
	pixel *data;
	pixel bgc;
	u32 width;
	u32 height;
	int result;
	u32 selidx;
	u32 filesize;
	buffer_array *exif_array;
	struct _cache_image_t *next;
} cache_image_t;

typedef struct _cacher_context
{
	bool on;
	bool first_run;
	bool isforward;
	u32 memory_usage;
	bool selidx_moved;

	cache_image_t *head, *tail;
	size_t caches_cap, caches_size;
} cacher_context;

int cache_init(void);
int cache_setup(unsigned max_cache_img, u32 * c_selidx);
void cache_free(void);
void dbg_dump_cache(void);
int cache_get_size();
void cache_set_forward(bool forward);
void cache_next_image(void);
void cache_on(bool on);
int cache_delete_first(void);
int cache_get_loaded_size();
int cache_routine(void);
void cache_reload_all(void);

int image_queue_test(void);

extern cacher_context ccacher;

#endif
