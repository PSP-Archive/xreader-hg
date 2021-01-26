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
#include "rar_speed_test.h"
#include "dbg.h"
#include "archive.h"
#include "buffer.h"
#include "freq_lock.h"
#include "power.h"
#include "image.h"
#include "scene.h"
#include "commons.h"
#include "image_queue.h"
#include "pspscreen.h"
#include "ctrl.h"
#include "xrhal.h"
#include "kubridge.h"

extern p_win_menuitem filelist;
extern dword filecount;
volatile dword *cache_selidx = NULL;

volatile cacher_context ccacher;

static volatile bool cacher_should_exit = false;
static volatile bool cacher_exited = false;
static volatile bool cacher_cleared = true;
static dword cache_img_cnt = 0;

static dword total_filesize;
static dword total_datasize;

enum
{
	CACHE_EVENT_DELETED = 1,
	CACHE_EVENT_UNDELETED = 2
};

static SceUID cache_del_event = -1;

static unsigned get_avail_memory(void)
{
	int memory;
	extern unsigned int get_free_mem(void);

	memory = get_free_mem();

	if (memory >= 1024 * 1024L) {
		// reverse for 1MB
		memory -= 1024 * 1024L;
	} else {
		memory = 0;
	}

	return memory;
}

static inline int cache_lock(void)
{
//  dbg_printf(d, "%s", __func__);
	return xrKernelWaitSema(ccacher.cacher_locker, 1, NULL);
}

static inline int cache_unlock(void)
{
//  dbg_printf(d, "%s", __func__);
	return xrKernelSignalSema(ccacher.cacher_locker, 1);
}

void cache_on(bool on)
{
	if (on) {
		cache_img_cnt = 0;
		cache_next_image();
		ccacher.first_run = true;
		xrKernelSetEventFlag(cache_del_event, CACHE_EVENT_DELETED);

		total_filesize = 0;
		total_datasize = 0;
	}

	ccacher.on = on;
}

// Ask cacher for next image
void cache_next_image(void)
{
	ccacher.selidx_moved = true;
}

static void cache_clear_without_lock()
{
	int i;

	for (i = 0; i < ccacher.caches_size; ++i) {
		if (ccacher.caches[i].data != NULL) {
			free(ccacher.caches[i].data);
		}

		if (ccacher.caches[i].status == CACHE_OK) {
			ccacher.memory_usage -=
				ccacher.caches[i].width * ccacher.caches[i].height *
				sizeof(pixel);
		}
	}

	ccacher.caches_size = 0;
}

static void cache_clear(void)
{
	cache_lock();
	cache_clear_without_lock();
	cache_unlock();
}

void cache_set_forward(bool forward)
{
	cache_lock();

	if (ccacher.isforward != forward) {
		cache_clear_without_lock();
		ccacher.first_run = true;
	}

	ccacher.isforward = forward;
	cache_unlock();
}

/**
 * 删除缓存, 并释放资源
 */
static int cache_delete_without_lock(cache_image_t * p)
{
	size_t pos;

	if (p == NULL)
		return -1;

	if (p->data != NULL) {
		free(p->data);
		p->data = NULL;
	}

	pos = p - ccacher.caches;

	if (p->status == CACHE_OK) {
		ccacher.memory_usage -= p->width * p->height * sizeof(pixel);
	}

	memmove(&ccacher.caches[pos], &ccacher.caches[pos + 1],
			(ccacher.caches_size - pos - 1) * sizeof(ccacher.caches[0]));
//  ccacher.caches = safe_realloc(ccacher.caches, sizeof(ccacher.caches[0]) * (ccacher.caches_size - 1));
	ccacher.caches_size--;

	return 0;
}

int cache_delete_first(void)
{
	cache_lock();

	if (ccacher.caches_size != 0 && ccacher.caches != NULL) {
		int ret;

		ret = cache_delete_without_lock(&ccacher.caches[0]);
		xrKernelSetEventFlag(cache_del_event, CACHE_EVENT_DELETED);
		cache_unlock();

		return ret;
	}

	cache_unlock();
	return -1;
}

static cache_image_t *cache_get(const char *archname, const char *filename);

/**
 * 根据路径添加缓存
 * 
 * @param archname 可能的档案名
 * @param filename 文件名
 * @param where 文件位置类型
 *
 */
static int cache_add_by_path(const char *archname, const char *filename,
							 int where, dword selidx, dword filesize)
{
	t_fs_filetype type;
	cache_image_t img;

	type = fs_file_get_type(filename);

	if (!fs_is_image(type)) {
		return -1;
	}

	if (cache_get(archname, filename) != NULL) {
		dbg_printf(d, "SERVER: %s: Image %s duplicate load, FIXME", __func__,
				   filename);

		return -1;
	}

	cache_lock();

	memset(&img, 0, sizeof(img));
	img.archname = archname;
	img.filename = filename;
	img.where = where;
	img.status = CACHE_INIT;
	img.selidx = selidx;
	img.filesize = filesize;

	if (ccacher.caches_size < ccacher.caches_cap) {
		ccacher.caches[ccacher.caches_size] = img;
		ccacher.caches_size++;
	} else {
		dbg_printf(d, "SERVER: cannot add cache any more: size %u cap %u",
				   ccacher.caches_size, ccacher.caches_cap);
		cache_unlock();

		return -1;
	}
	cache_unlock();

	return 0;
}

void dbg_dump_cache(void)
{
	cache_lock();

	cache_image_t *p = ccacher.caches;
	dword c;

	for (c = 0; p != ccacher.caches + ccacher.caches_size; ++p) {
		if (p->status == CACHE_OK || p->status == CACHE_FAILED)
			c++;
	}

	dbg_printf(d, "CLIENT: Dumping cache[%u] %u/%ukb, %u finished",
			   ccacher.caches_size, (unsigned) ccacher.memory_usage / 1024,
			   (unsigned) get_avail_memory() / 1024, (unsigned) c);

	for (p = ccacher.caches; p != ccacher.caches + ccacher.caches_size; ++p) {
		dbg_printf(d, "%d: %u st %u res %d mem %lukb", p - ccacher.caches,
				   (unsigned) p->selidx, p->status, p->result,
				   p->width * p->height * sizeof(pixel) / 1024L);
	}

	cache_unlock();
}

int start_cache_next_image(void)
{
	cache_image_t *p = NULL;
	cache_image_t tmp;
	t_fs_filetype ft;
	static bool alarmed = false;
	dword used_memory, free_memory;

	free_memory = get_avail_memory();

	if (kuKernelGetModel() == PSP_MODEL_SLIM_AND_LITE) {
		used_memory = 45 * 1024 * 1024L - free_memory;
	} else {
		used_memory = 13 * 1024 * 1024L - free_memory;
	}

	if (ccacher.memory_usage > used_memory) {
		if (!alarmed)
			dbg_printf(d, "SERVER: %s: Memory usage %uKB, refuse to cache",
					   __func__, (unsigned) ccacher.memory_usage / 1024);

		alarmed = true;

		return -1;
	}

	if (alarmed)
		dbg_printf(d, "SERVER: %s: Memory usage %uKB, OK to go now", __func__,
				   (unsigned) ccacher.memory_usage / 1024);

	cache_lock();

	// Select first unloaded image
	for (p = ccacher.caches; p != ccacher.caches + ccacher.caches_size; ++p) {
		if (p->status == CACHE_INIT || p->status == CACHE_FAILED) {
			break;
		}
	}

	// if we ecounter FAILED cache, abort the caching, because user will quit when the image shows up
	if (p == ccacher.caches + ccacher.caches_size || p->status == CACHE_FAILED) {
		cache_unlock();
		return 0;
	}

	memcpy(&tmp, p, sizeof(tmp));
	cache_unlock();

	// Predict memory usage
	{
		double ratio;

		if (total_datasize != 0) {
			ratio = (double) total_filesize / total_datasize;
		} else {
			ratio = 0;
		}

		dbg_printf(d, "Total compress ratio: %.2f", ratio);

		if (ratio != 0) {
			dword predict_usage;

			predict_usage = (dword) (tmp.filesize / ratio);

			if (predict_usage >= free_memory) {
				// if it is the first image in the queue, then give a chance
				if (ccacher.memory_usage == 0) {
					goto load;
				}

				if (!alarmed)
					dbg_printf(d,
							   "SERVER: %s: memory predict usage %dkb overload(%dkb free), refuse to cache",
							   __func__, (unsigned) predict_usage / 1024,
							   (unsigned) free_memory / 1024);

				alarmed = true;

				return -1;
			}
		}
	}

  load:
	alarmed = false;
	ft = fs_file_get_type(tmp.filename);
	dbg_switch(d, 0);
	int fid = freq_enter_hotzone();

	tmp.result =
		image_open_archive(tmp.filename, tmp.archname, ft, &tmp.width,
						   &tmp.height, &tmp.data, &tmp.bgc, tmp.where);
	freq_leave(fid);
	dbg_switch(d, 1);

	if (tmp.result == 0) {
		dword memory_used;

		memory_used = tmp.width * tmp.height * sizeof(pixel);
//      dbg_printf(d, "SERVER: Image %u finished loading", (unsigned)tmp.selidx);
		ccacher.memory_usage += memory_used;
//      dbg_printf(d, "%s: Memory usage %uKB", __func__, (unsigned) ccacher.memory_usage / 1024);
		total_filesize += tmp.filesize;
		total_datasize += memory_used;
		tmp.status = CACHE_OK;
	} else if (tmp.result == 4 || tmp.result == 5) {
		// out of memory

		// is memory completely out of memory?
		if (ccacher.memory_usage == 0) {
//          dbg_printf(d, "SERVER: Image %u finished failed(%u), giving up", (unsigned)tmp.selidx, tmp.result);
			tmp.status = CACHE_FAILED;
		} else {
			// retry later
//          dbg_printf(d, "SERVER: Image %u finished failed(%u), retring", (unsigned)tmp.selidx, tmp.result);
//          dbg_printf(d, "%s: Memory usage %uKB", __func__, (unsigned) ccacher.memory_usage / 1024);
		}

		if (tmp.data != NULL) {
			free(tmp.data);
			tmp.data = NULL;
		}
	} else {
//      dbg_printf(d, "SERVER: Image %u finished failed(%u)", (unsigned)tmp.selidx, tmp.result);
		tmp.status = CACHE_FAILED;
	}

	cache_lock();

	for (p = ccacher.caches; p != ccacher.caches + ccacher.caches_size; ++p) {
		if (p->status == CACHE_INIT || p->status == CACHE_FAILED) {
			break;
		}
	}

	// if we ecounter FAILED cache, abort the caching, because user will quit when the image shows up
	if (p == ccacher.caches + ccacher.caches_size || p->status == CACHE_FAILED) {
		cache_unlock();
		return 0;
	}

	memcpy(p, &tmp, sizeof(tmp));
	cache_unlock();

	return 0;
}

static dword cache_get_next_image(dword pos, bool forward)
{
	if (forward) {
		do {
			if (pos < filecount - 1) {
				pos++;
			} else {
				pos = 0;
			}
		} while (!fs_is_image((t_fs_filetype) filelist[pos].data));
	} else {
		do {
			if (pos > 0) {
				pos--;
			} else {
				pos = filecount - 1;
			}
		} while (!fs_is_image((t_fs_filetype) filelist[pos].data));
	}

	return pos;
}

/* Count how many img in archive */
static dword count_img(void)
{
	if (filecount == 0 || filelist == NULL)
		return 0;

	dword i = 0;

	if (cache_img_cnt != 0)
		return cache_img_cnt;

	for (i = 0; i < filecount; ++i) {
		if (fs_is_image((t_fs_filetype) filelist[i].data))
			cache_img_cnt++;
	}

	return cache_img_cnt;
}

static int start_cache(dword selidx)
{
	int re;
	dword pos;
	dword size;

	cache_lock();

	if (ccacher.first_run) {
		ccacher.first_run = false;
	} else {
		// wait until user notify cache delete
		xrKernelWaitEventFlag(cache_del_event, CACHE_EVENT_DELETED,
							  PSP_EVENT_WAITAND, NULL, NULL);
		xrKernelSetEventFlag(cache_del_event, CACHE_EVENT_UNDELETED);
	}

	pos = selidx;
	size = ccacher.caches_size;

	while (size-- > 0) {
		pos = cache_get_next_image(pos, ccacher.isforward);
	}

	re = min(ccacher.caches_cap, count_img()) - ccacher.caches_size;
	dbg_printf(d, "SERVER: start pos %u selidx %u caches_size %u re %u",
			   (unsigned) pos, (unsigned) selidx,
			   (unsigned) ccacher.caches_size, (unsigned) re);
	cache_unlock();

	if (re == 0) {
		return 0;
	}
	// dbg_printf(d, "SERVER: Wait for new selidx: memory usage %uKB", (unsigned) ccacher.memory_usage / 1024);
	// dbg_printf(d, "SERVER: %d images to cache, selidx %u, caches_size %u", re, (unsigned)selidx, (unsigned)ccacher.caches_size);

	while (re-- > 0) {
		dbg_printf(d, "SERVER: add cache image %u", (unsigned) pos);
		cache_add_by_path(config.shortpath, filelist[pos].compname->ptr, where,
						  pos, filelist[pos].data3);
		pos = cache_get_next_image(pos, ccacher.isforward);
	}

	return re;
}

static int thread_cacher(SceSize args, void *argp)
{
	while (!cacher_should_exit) {
		if (ccacher.on) {
			cacher_cleared = false;

			while (ccacher.selidx_moved == false && !cacher_should_exit
				   && ccacher.on) {
				// cache next image
				start_cache_next_image();
				xrKernelDelayThread(1000000);
			}

			if (cacher_should_exit)
				break;

			if (!ccacher.on) {
				continue;
			}

			start_cache(*cache_selidx);
			ccacher.selidx_moved = false;
		} else {
			// clean up all remain cache now
			cache_clear();
			cacher_cleared = true;
		}

		xrKernelDelayThread(100000);
	}

	cacher_exited = true;
	xrKernelExitDeleteThread(0);

	return 0;
}

int cache_init(unsigned max_cache_img, dword * c_selidx)
{
	int ret;

	dbg_printf(d, "set max memory usage to %ukb",
			   (unsigned) get_avail_memory() / 1024);

	ccacher.cacher_locker = xrKernelCreateSema("Cache Mutex", 0, 1, 1, 0);
	ccacher.cacher_thread =
		xrKernelCreateThread("thread_cacher", thread_cacher, 90, 0x10000, 0,
							 NULL);

	if (ccacher.cacher_thread < 0) {
		dbg_printf(d, "create thread failed: 0x%08x",
				   (unsigned) ccacher.cacher_thread);

		return -1;
	}

	cache_selidx = c_selidx;

	cacher_cleared = true;
	cacher_should_exit = false;
	cacher_exited = false;

	ccacher.caches_size = 0;
	ccacher.caches_cap = max_cache_img;
	ccacher.caches = calloc(ccacher.caches_cap, sizeof(ccacher.caches[0]));

	cache_del_event = xrKernelCreateEventFlag("cache_del_event", 0, 0, 0);

	if ((ret = xrKernelStartThread(ccacher.cacher_thread, 0, NULL)) < 0) {
		dbg_printf(d, "start thread failed: 0x%08x", (unsigned) ret);

		return -1;
	}

	return 0;
}

void cache_free(void)
{
	cache_on(false);

	while (!cacher_cleared) {
		xrKernelDelayThread(100000);
	}

	if (ccacher.cacher_thread >= 0) {
		cacher_should_exit = true;

		while (!cacher_exited) {
			xrKernelDelayThread(100000);
		}

		ccacher.cacher_thread = -1;
	}

	if (ccacher.cacher_locker >= 0) {
		xrKernelDeleteSema(ccacher.cacher_locker);
		ccacher.cacher_locker = -1;
	}

	if (cache_del_event >= 0) {
		xrKernelDeleteEventFlag(cache_del_event);
		cache_del_event = -1;
	}

	if (ccacher.caches != NULL) {
		free(ccacher.caches);
		ccacher.caches = NULL;
	}

	ccacher.caches_size = ccacher.caches_cap = 0;
}

/**
 * 得到缓存信息
 */
static cache_image_t *cache_get(const char *archname, const char *filename)
{
	cache_image_t *p;

	if (ccacher.caches == NULL || ccacher.caches_size == 0 || filename == NULL) {
		return NULL;
	}

	for (p = ccacher.caches; p != ccacher.caches + ccacher.caches_size; ++p) {
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
	cache_image_t *p = ccacher.caches;
	int c;

	for (c = 0; p != ccacher.caches + ccacher.caches_size; ++p) {
		if (p->status == CACHE_OK)
			c++;
	}

	return c;
}
