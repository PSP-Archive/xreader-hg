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

extern p_win_menuitem filelist;

extern dword filecount;

extern volatile cacher_context ccacher;

static bool draw_image = false;

bool user_ask_for_exit = false;
bool user_ask_for_next_image = false;
bool user_ask_for_prev_image = false;

void parse_input()
{
	dword key = ctrl_read();

	if (key == PSP_CTRL_LTRIGGER) {
		dbg_printf(d, "user has asked for prev image");
		user_ask_for_prev_image = true;
	}
	if (key == PSP_CTRL_RTRIGGER) {
		dbg_printf(d, "user has asked for next image");
		user_ask_for_next_image = true;
	}
	if (key == PSP_CTRL_CROSS) {
		dbg_printf(d, "user has asked for exit");
		user_ask_for_exit = true;
	}
	if (key == PSP_CTRL_START)
		dbg_dump_cache();

//  ctrl_waitrelease();
}

int cache_wait_avail()
{
	while (ccacher.caches_size == 0) {
		xrKernelDelayThread(10000);
	}

	return 0;
}

void cache_wait_loaded()
{
	cache_image_t *img = &ccacher.caches[0];

	while (img->status == CACHE_INIT) {
//      dbg_printf(d, "CLIENT: Wait image %u %s load finish", (unsigned) selidx, filename);
		xrKernelDelayThread(10000);
	}
}

int get_image(dword selidx)
{
	cache_image_t *img;
	u64 start, now;
	int ret;

	xrRtcGetCurrentTick(&start);

	cache_wait_avail();

	img = &ccacher.caches[0];

	DBG_ASSERT(d, "CLIENT: img->selidx == selidx", img->selidx == selidx);

	cache_wait_loaded();

	xrRtcGetCurrentTick(&now);

	if (img->status == CACHE_OK && img->result == 0) {
		dbg_printf(d, "CLIENT: Image %u load OK in %.3f s, %ux%u",
				   (unsigned) img->selidx, pspDiffTime(&now, &start),
				   (unsigned) img->width, (unsigned) img->height);
		ret = 0;
	} else {
		dbg_printf(d,
				   "CLIENT: Image %u load FAILED in %.3f s, %ux%u, status %u, result %u",
				   (unsigned) img->selidx, pspDiffTime(&now, &start),
				   (unsigned) img->width, (unsigned) img->height, img->status,
				   img->result);
		ret = -1;
	}

	// Draw image
	if (draw_image) {
		if (img->data) {
			disp_flip();
			disp_putimage(0, 0, img->width, img->height, 0, 0, img->data);
			disp_flip();
		}
	}
//  xrKernelDelayThread(1000000);

	return ret;
}

void update_selidx(volatile dword * selidx, dword count, bool is_forward)
{
	if (is_forward) {
		do {
			if (*selidx < count - 1) {
				(*selidx)++;
			} else {
				*selidx = 0;
			}
		} while (!fs_is_image((t_fs_filetype) filelist[*selidx].data));
	} else {
		do {
			if (*selidx > 0) {
				(*selidx)--;
			} else {
				*selidx = count - 1;
			}
		} while (!fs_is_image((t_fs_filetype) filelist[*selidx].data));
	}
}

dword selidx = 0;

int test_cache()
{
	bool is_forward = true, is_needrf = true;

	selidx = 0;

	while (!fs_is_image((t_fs_filetype) filelist[selidx].data)
		   && selidx < filecount) {
		selidx++;
	}

	if (selidx == filecount)
		return -1;

	ctrl_init();
	disp_init();
	cache_set_forward(true);
	// enable cache now
	cache_on(true);

	user_ask_for_exit = false;

	while (!user_ask_for_exit) {
		if (user_ask_for_next_image) {
			is_forward = true;
			is_needrf = true;
			cache_set_forward(is_forward);
			user_ask_for_next_image = false;
			update_selidx(&selidx, filecount, is_forward);
			cache_next_image();
		}

		if (user_ask_for_prev_image) {
			is_forward = false;
			is_needrf = true;
			cache_set_forward(is_forward);
			user_ask_for_prev_image = false;
			update_selidx(&selidx, filecount, is_forward);
			cache_next_image();
		}

		if (is_needrf) {
			cache_delete_first();

			if (get_image(selidx) != 0) {
				user_ask_for_exit = true;
			}

			is_needrf = false;
		}

		parse_input();
	}

	// disable cache now
	cache_on(false);

	return 0;
}

int image_queue_test(void)
{
	int fid;
	static int i = 0;

	dbg_printf(d, "Start image queue test");
	extern dword selidx;
	cache_init(&selidx);

	fid = freq_enter_hotzone();

	if (filelist != NULL) {
		win_item_destroy(&filelist, &filecount);
	}

	where = scene_in_rar;

#if 1
	if (i == 0)
		SPRINTF_S(config.shortpath, "ms0:/PIC.rar");
	else
		SPRINTF_S(config.shortpath, "ms0:/PIC%d.rar", i + 1);

	++i;
#else
	SPRINTF_S(config.shortpath, "ms0:/PIC%d.rar", 3);
#endif
	filecount = fs_rar_to_menu(config.shortpath, &filelist, 0, 0, 0, 0);

	freq_leave(fid);

	if (filelist == NULL) {
		dbg_printf(d, "Cannot open %s", config.shortpath);
		cache_free();

		return -1;
	}

	test_cache();

	if (filelist != NULL) {
		win_item_destroy(&filelist, &filecount);
	}

	cache_free();

	return 0;
}
