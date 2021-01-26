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

#include <pspdebug.h>
#include <pspkernel.h>
#include <pspsdk.h>
#include <psprtc.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "win.h"
#include "common/datatype.h"
#include "common/utils.h"
#include "fs.h"
#include "dbg.h"
#include "archive.h"
#include "buffer.h"
#include "freq_lock.h"
#include "power.h"
#include "image.h"
#include "scene.h"
#include "image_queue.h"
#include "ctrl.h"
#include "kubridge.h"
#include "common/utils.h"
#include "thread_lock.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

extern p_win_menu g_menu;

volatile u32 *cache_selidx = NULL;

static cache_image_t g_head;

cacher_context ccacher;

static bool cacher_cleared = true;
static u32 cache_img_cnt = 0;
static uint16_t curr_times = 0, avoid_times = 0;

static struct psp_mutex_t cacher_locker;

enum
{
	CACHE_EVENT_DELETED = 1,
	CACHE_EVENT_UNDELETED = 2
};

static SceUID cache_del_event = -1;

static inline int cache_lock(void)
{
	return xr_lock(&cacher_locker);
}

static inline int cache_unlock(void)
{
	return xr_unlock(&cacher_locker);
}

static void cache_clear(void);

void cache_on(bool on)
{
	if (on) {
		cache_img_cnt = 0;
		cache_next_image();
		ccacher.first_run = true;
		sceKernelSetEventFlag(cache_del_event, CACHE_EVENT_DELETED);

		ccacher.on = on;
	} else {
		ccacher.on = on;

		while (!cacher_cleared) {
			sceKernelDelayThread(100000);
		}
	}
}

// Ask cacher for next image
void cache_next_image(void)
{
	ccacher.selidx_moved = true;
	curr_times = avoid_times = 0;
}

static void cache_clear(void)
{
	cache_lock();

	while (ccacher.head->next != NULL) {
		cache_delete_first();
	}

	cacher_cleared = true;
	malloc_trim(0);
	cache_unlock();
}

void cache_set_forward(bool forward)
{
	cache_lock();

	if (ccacher.isforward != forward) {
		cache_clear();
		ccacher.first_run = true;
	}

	ccacher.isforward = forward;
	cache_unlock();
}

int cache_delete_first(void)
{
	cache_image_t *p;

	cache_lock();

	p = ccacher.head->next;

	if (p == NULL) {
		cache_unlock();

		return -1;
	}

	if (p->data != NULL) {
		dbg_printf(d, "%s: data 0x%08x", __func__, (unsigned) p->data);
		free(p->data);
		p->data = NULL;
	}

	if (p->status == CACHE_OK) {
		ccacher.memory_usage -= p->width * p->height * sizeof(pixel);
	}

	ccacher.head->next = p->next;
	free(p);

	if (ccacher.tail == p) {
		ccacher.tail = ccacher.head;
	}

	ccacher.caches_size--;

	sceKernelSetEventFlag(cache_del_event, CACHE_EVENT_DELETED);
	cache_unlock();

	return 0;
}

static cache_image_t *cache_get(const char *archname, const char *filename);

/**
 * @param selidx filelist中的文件位置
 * @param where 文件位置类型
 *
 */
static int cache_add_by_selidx(u32 selidx, int where)
{
	t_fs_filetype type;
	cache_image_t *img;
	const char *archname;
	const char *filename;
	u32 filesize;

	archname = config.shortpath;

	if (where == scene_in_dir) {
		filename = g_menu->root[selidx].shortname->ptr;
	} else {
		filename = g_menu->root[selidx].compname->ptr;
	}

	filesize = g_menu->root[selidx].data3;

	type = fs_file_get_type(filename);

	if (!fs_is_image(type)) {
		return -1;
	}

	if (cache_get(archname, filename) != NULL) {
		dbg_printf(d, "SERVER: %s: Image %s duplicate load, FIXME", __func__, filename);

		return -1;
	}

	cache_lock();

	img = (cache_image_t *) malloc(sizeof(*img));
	memset(img, 0, sizeof(*img));
	img->archname = archname;
	img->filename = filename;
	img->where = where;
	img->status = CACHE_INIT;
	img->selidx = selidx;
	img->filesize = filesize;
	img->next = NULL;

	if (ccacher.caches_size < ccacher.caches_cap) {
		ccacher.tail->next = img;
		ccacher.tail = img;
		ccacher.caches_size++;
		cacher_cleared = false;
	} else {
		dbg_printf(d, "SERVER: cannot add cache any more: size %u cap %u", ccacher.caches_size, ccacher.caches_cap);
		cache_unlock();

		return -1;
	}

	cache_unlock();

	return 0;
}

#ifdef DEBUG
void dbg_dump_cache(void)
{
	cache_image_t *p;
	u32 c;

	cache_lock();
	p = ccacher.head->next;

	for (c = 0; p != NULL; p = p->next) {
		if (p->status == CACHE_OK || p->status == CACHE_FAILED)
			c++;
	}

	dbg_printf(d, "CLIENT: Dumping cache[%u] %u/%ukb, %u finished",
			   ccacher.caches_size, (unsigned) ccacher.memory_usage / 1024, (unsigned) get_free_mem() / 1024, (unsigned) c);

	for (p = ccacher.head->next, c = 0; p != NULL; p = p->next) {
		dbg_printf(d, "%d: %u st %u res %d mem %lukb", ++c, (unsigned) p->selidx, p->status, p->result, p->width * p->height * sizeof(pixel) / 1024L);
	}

	cache_unlock();
}
#endif

static void copy_cache_image(cache_image_t * dst, const cache_image_t * src)
{
	memcpy(dst, src, sizeof(*src));
}

static void free_cache_image(cache_image_t * p)
{
	if (p == NULL)
		return;

	if (p->data != NULL) {
		free(p->data);
		p->data = NULL;
	}
}

int start_cache_next_image(void)
{
	cache_image_t *p = NULL;
	cache_image_t tmp;
	t_fs_filetype ft;
	u32 free_memory;
	int fid;

	if (avoid_times && curr_times++ < avoid_times) {
//      dbg_printf(d, "%s: curr_times %d avoid time %d", __func__, curr_times, avoid_times);
		return -1;
	}

	free_memory = get_free_mem();

	if (config.scale >= 100) {
		if (free_memory < 8 * 1024 * 1024) {
			return -1;
		}
	} else if (free_memory < 1024 * 1024) {
		return -1;
	}

	cache_lock();

	for (p = ccacher.head->next; p != NULL; p = p->next) {
		if (p->status == CACHE_INIT || p->status == CACHE_FAILED) {
			break;
		}
	}

	// if we ecounter FAILED cache, abort the caching, because user will quit when the image shows up
	if (p == NULL || p->status == CACHE_FAILED) {
		cache_unlock();
		return 0;
	}

	copy_cache_image(&tmp, p);
	cache_unlock();
	ft = fs_file_get_type(tmp.filename);
	fid = freq_enter_hotzone();

	if (tmp.where == scene_in_dir) {
		char fullpath[PATH_MAX];

		STRCPY_S(fullpath, tmp.archname);
		STRCAT_S(fullpath, tmp.filename);
		tmp.result = image_open_archive(fullpath, tmp.archname, ft, &tmp.width, &tmp.height, &tmp.data, &tmp.bgc, tmp.where);
	} else {
		tmp.result = image_open_archive(tmp.filename, tmp.archname, ft, &tmp.width, &tmp.height, &tmp.data, &tmp.bgc, tmp.where);
	}

	if (tmp.result == 0 && tmp.data != NULL && config.imgbrightness != 100) {
		pixel *t = tmp.data;
		short b = 100 - config.imgbrightness;
		u32 i;

		for (i = 0; i < tmp.height * tmp.width; i++) {
			*t = disp_grayscale(*t, 0, 0, 0, b);
			t++;
		}
	}

	freq_leave(fid);

	cache_lock();

	for (p = ccacher.head->next; p != NULL; p = p->next) {
		if (p->status == CACHE_INIT || p->status == CACHE_FAILED) {
			break;
		}
	}

	// recheck the first unloaded (and not failed) image, for we haven't locked cache for a while
	if (p == NULL || p->status == CACHE_FAILED) {
		free_cache_image(&tmp);
		cache_unlock();
		return 0;
	}

	if (tmp.result == 0) {
		u32 memory_used;

		memory_used = tmp.width * tmp.height * sizeof(pixel);

//      dbg_printf(d, "SERVER: Image %u finished loading", (unsigned)tmp.selidx);
//      dbg_printf(d, "%s: Memory usage %uKB", __func__, (unsigned) ccacher.memory_usage / 1024);
		ccacher.memory_usage += memory_used;
		cacher_cleared = false;
		tmp.status = CACHE_OK;
		copy_cache_image(p, &tmp);
		tmp.data = NULL;
		free_cache_image(&tmp);
		curr_times = avoid_times = 0;
	} else if ((tmp.result == 4 || tmp.result == 5)
			   || (tmp.where == scene_in_rar && tmp.result == 6)) {
		// out of memory
		// if unrar throwed a bad_cast exception when run out of memory, result can be 6 also.

		// is memory completely out of memory?
		if (ccacher.memory_usage == 0) {
//          dbg_printf(d, "SERVER: Image %u finished failed(%u), giving up", (unsigned)tmp.selidx, tmp.result);
			tmp.status = CACHE_FAILED;
			copy_cache_image(p, &tmp);
			p->data = NULL;
		} else {
			// retry later
//          dbg_printf(d, "SERVER: Image %u finished failed(%u), retring", (unsigned)tmp.selidx, tmp.result);
//          dbg_printf(d, "%s: Memory usage %uKB", __func__, (unsigned) ccacher.memory_usage / 1024);
			if (avoid_times) {
				avoid_times *= 2;
			} else {
				avoid_times = 1;
			}

			avoid_times = min(avoid_times, 32767);
			curr_times = 0;
		}

		free_cache_image(&tmp);
	} else {
//      dbg_printf(d, "SERVER: Image %u finished failed(%u)", (unsigned)tmp.selidx, tmp.result);
		tmp.status = CACHE_FAILED;
		copy_cache_image(p, &tmp);
		p->data = NULL;
		free_cache_image(&tmp);
	}

	cache_unlock();

	return 0;
}

static u32 cache_get_next_image(u32 pos, bool forward)
{
	if (forward) {
		do {
			if (pos < g_menu->size - 1) {
				pos++;
			} else {
				pos = 0;
			}
		} while (!fs_is_image((t_fs_filetype) g_menu->root[pos].data));
	} else {
		do {
			if (pos > 0) {
				pos--;
			} else {
				pos = g_menu->size - 1;
			}
		} while (!fs_is_image((t_fs_filetype) g_menu->root[pos].data));
	}

	return pos;
}

/* Count how many img in archive */
static u32 count_img(void)
{
	u32 i = 0;

	if (g_menu->size == 0 || g_menu->root == NULL)
		return 0;

	if (cache_img_cnt != 0)
		return cache_img_cnt;

	for (i = 0; i < g_menu->size; ++i) {
		if (fs_is_image((t_fs_filetype) g_menu->root[i].data))
			cache_img_cnt++;
	}

	return cache_img_cnt;
}

static int start_cache(u32 selidx)
{
	int re;
	u32 pos;
	u32 size;

	if (ccacher.first_run) {
		ccacher.first_run = false;
	} else {
		SceUInt timeout = 10000;

		// wait until user notify cache delete
		re = sceKernelWaitEventFlag(cache_del_event, CACHE_EVENT_DELETED, PSP_EVENT_WAITAND, NULL, &timeout);

		if (re == SCE_KERNEL_ERROR_WAIT_TIMEOUT) {
			return 0;
		}

		sceKernelSetEventFlag(cache_del_event, CACHE_EVENT_UNDELETED);
	}

	cache_lock();

	pos = selidx;
	size = ccacher.caches_size;

	while (size-- > 0) {
		pos = cache_get_next_image(pos, ccacher.isforward);
	}

	re = min(ccacher.caches_cap, count_img()) - ccacher.caches_size;
	dbg_printf(d, "SERVER: start pos %u selidx %u caches_size %u re %u", (unsigned) pos, (unsigned) selidx, (unsigned) ccacher.caches_size, (unsigned) re);

	if (re == 0) {
		cache_unlock();
		return 0;
	}
	// dbg_printf(d, "SERVER: Wait for new selidx: memory usage %uKB", (unsigned) ccacher.memory_usage / 1024);
	// dbg_printf(d, "SERVER: %d images to cache, selidx %u, caches_size %u", re, (unsigned)selidx, (unsigned)ccacher.caches_size);

	while (re-- > 0) {
		dbg_printf(d, "SERVER: add cache image %u", (unsigned) pos);
		cache_add_by_selidx(pos, where);
		pos = cache_get_next_image(pos, ccacher.isforward);
	}

	cache_unlock();

	return re;
}

int cache_routine(void)
{
	if (ccacher.on) {
		if (!ccacher.selidx_moved && ccacher.on) {
			// cache next image
			start_cache_next_image();

			return 0;
		}

		if (!ccacher.on) {
			return 0;
		}

		start_cache(*cache_selidx);
		ccacher.selidx_moved = false;
	} else {
		// clean up all remain cache now
		cache_clear();
	}

	sceKernelDelayThread(100000);

	return 0;
}

int cache_init(void)
{
	xr_lock_init(&cacher_locker);
	ccacher.caches_size = 0;
	ccacher.caches_cap = 0;

	cache_del_event = sceKernelCreateEventFlag("cache_del_event", 0, 0, 0);
	ccacher.head = ccacher.tail = &g_head;

	return 0;
}

int cache_setup(unsigned max_cache_img, u32 * c_selidx)
{
	cache_selidx = c_selidx;
	ccacher.caches_cap = max_cache_img;
	cacher_cleared = true;

	return 0;
}

void cache_free(void)
{
	cache_on(false);

	xr_lock_destroy(&cacher_locker);

	if (cache_del_event >= 0) {
		sceKernelDeleteEventFlag(cache_del_event);
		cache_del_event = -1;
	}
}

/**
 * 得到缓存信息
 */
static cache_image_t *cache_get(const char *archname, const char *filename)
{
	cache_image_t *p;

	for (p = ccacher.head->next; p != NULL; p = p->next) {
		if (((archname == p->archname && archname == NULL)
			 || !strcmp(p->archname, archname))
			&& !strcmp(p->filename, filename)) {
			return p;
		}
	}

	return NULL;
}

int cache_get_loaded_size()
{
	cache_image_t *p = ccacher.head->next;
	int c;

	for (c = 0; p != NULL; p = p->next) {
		if (p->status == CACHE_OK)
			c++;
	}

	return c;
}

void cache_reload_all(void)
{
	cache_image_t *p;

	cache_lock();

	for (p = ccacher.head->next; p != NULL; p = p->next) {
		if (p->status == CACHE_OK) {
			free_cache_image(p);
			ccacher.memory_usage -= p->width * p->height * sizeof(pixel);
		}

		p->status = CACHE_INIT;
	}

	cache_unlock();
}
