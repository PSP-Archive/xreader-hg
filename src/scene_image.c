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

#include "config.h"

#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <pspdebug.h>
#include <psprtc.h>
#include <pspkernel.h>
#include <psppower.h>
#include "display.h"
#include "win.h"
#include "ctrl.h"
#include "fs.h"
#include "image.h"
#ifdef ENABLE_USB
#include "usb.h"
#endif
#include "power.h"
#include "bookmark.h"
#include "conf.h"
#include "charsets.h"
#include "location.h"
#include "text.h"
#include "bg.h"
#include "copy.h"
#include "common/qsort.h"
#include "common/utils.h"
#include "scene_impl.h"
#include "simple_gettext.h"
#include "math.h"
#include "freq_lock.h"
#include "dbg.h"
#include "image_queue.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

#ifdef ENABLE_IMAGE

u32 width, height, width_rotated = 0;
u32 height_rotated = 0, thumb_width = 0, thumb_height = 0;
u32 paintleft = 0, painttop = 0;
pixel *imgdata = NULL, *imgshow = NULL;
pixel bgcolor = 0, thumbimg[128 * 128];
u32 oldangle = 0;
char filename[PATH_MAX];
int curtop = 0, curleft = 0, xpos = 0, ypos = 0;
bool img_needrf = true, img_needrc = true, img_needrp = true;
static bool showinfo = false, thumb = false;
int imgh;
static bool slideshow = false;
static time_t now = 0, lasttime = 0;
extern void *framebuffer;
extern win_menu_predraw_data g_predraw;

static volatile int secticks = 0;
static int g_imgpaging_count = 0;
static int g_current_spd = 0;
static int destx = 0, desty = 0, srcx = 0, srcy = 0;
static bool in_move_z_mode = false;
static int z_mode_cnt = 0;

static inline void reset_image_show_ptr(void)
{
	if (imgshow != imgdata && imgshow != NULL) {
		free(imgshow);
	}

	imgshow = NULL;
}

static void reset_image_ptr(void)
{
	reset_image_show_ptr();
}

static void report_image_error(int status)
{
	char infomsg[80];
	const char *errstr;

	switch (status) {
		case 1:
		case 2:
		case 3:
			errstr = _("格式错误");
			break;
		case 4:
		case 5:
			errstr = _("内存不足");
			break;
		case 6:
			errstr = _("压缩档案损坏或密码错误");
			break;
		default:
			errstr = _("不明");
			break;
	}

	SPRINTF_S(infomsg, _("图像无法装载, 原因: %s"), errstr);
	win_msg(infomsg, COLOR_WHITE, COLOR_WHITE, config.msgbcolor);
	ctrl_waitrelease();
	dbg_printf(d, _("图像无法装载，原因: %s where = %d config.path %s filename %s"), errstr, where, config.path, filename);
	imgreading = false;
	reset_image_ptr();
}

static int cache_wait_avail()
{
	u32 key;

	while (ccacher.caches_size == 0) {
		key = ctrl_read();

		if (key != 0 && (key == config.imgkey[9] || key == config.imgkey2[9])) {
			return -1;
		}

		sceKernelDelayThread(10000);
	}

	return 0;
}

static int cache_wait_loaded()
{
	cache_image_t *img = ccacher.head->next;
	u32 key;

	while (img->status == CACHE_INIT) {
//      dbg_printf(d, "CLIENT: Wait image %u %s load finish", (unsigned) selidx, filename);
		key = ctrl_read();

		if (key != 0 && (key == config.imgkey[9] || key == config.imgkey2[9])) {
			return -1;
		}

		sceKernelDelayThread(10000);
	}

	return 0;
}

static int cache_get_image(u32 selidx)
{
	cache_image_t *img;
	u64 start, now;
	int ret;

	sceRtcGetCurrentTick(&start);

	if (cache_wait_avail() != 0) {
		return -1;
	}

	img = ccacher.head->next;

	DBG_ASSERT(d, "CLIENT: img->selidx == selidx", img->selidx == selidx);

	if (cache_wait_loaded() != 0) {
		return -1;
	}

	sceRtcGetCurrentTick(&now);

	if (img->status == CACHE_OK && img->result == 0) {
		dbg_printf(d, "CLIENT: Image %u load OK in %.3f s, %ux%u",
				   (unsigned) img->selidx, pspDiffTime(&now, &start), (unsigned) img->width, (unsigned) img->height);
		ret = 0;
	} else {
		dbg_printf(d,
				   "CLIENT: Image %u load FAILED in %.3f s, %ux%u, status %u, result %u",
				   (unsigned) img->selidx, pspDiffTime(&now, &start), (unsigned) img->width, (unsigned) img->height, img->status, img->result);
		ret = img->result;
	}

	if (ret == 0) {
		imgdata = img->data;
	} else {
		imgdata = NULL;
	}

	width = img->width;
	height = img->height;

	return ret;
}

static int scene_reloadimage(u32 selidx)
{
	int result;

	reset_image_ptr();

	if (where == scene_in_zip || where == scene_in_chm || where == scene_in_rar)
		STRCPY_S(filename, g_menu->root[selidx].compname->ptr);
	else {
		STRCPY_S(filename, config.shortpath);
		STRCAT_S(filename, g_menu->root[selidx].shortname->ptr);
	}

	result = cache_get_image(selidx);

	if (result != 0) {
		report_image_error(result);
		return -1;
	}

	STRCPY_S(config.lastfile, g_menu->root[selidx].compname->ptr);
	STRCPY_S(prev_path, config.path);
	STRCPY_S(prev_shortpath, config.shortpath);
	STRCPY_S(prev_lastfile, g_menu->root[selidx].compname->ptr);
	prev_where = where;
	oldangle = 0;

	return 0;
}

static u32 scene_rotateimage(void)
{
	int ret;

	ret = image_rotate(imgdata, &width, &height, oldangle, (u32) config.rotate * 90);

	if (ret < 0) {
		win_msg("内存不足无法完成旋转!", COLOR_WHITE, COLOR_WHITE, config.msgbcolor);
		config.rotate = conf_rotate_0;
	}

	oldangle = (u32) config.rotate * 90;

	if (config.fit > 0 && (config.fit != conf_fit_custom || config.scale != 100)) {

		reset_image_show_ptr();

		if (config.fit == conf_fit_custom) {
			width_rotated = width * config.scale / 100;
			height_rotated = height * config.scale / 100;
		} else if (config.fit == conf_fit_width) {
			config.scale = PSP_SCREEN_WIDTH / width;

			if (config.scale > 200)
				config.scale = (config.scale / 50) * 50;
			else {
				config.scale = (config.scale / 10) * 10;
				if (config.scale < 10)
					config.scale = 10;
			}

			width_rotated = PSP_SCREEN_WIDTH;
			height_rotated = height * PSP_SCREEN_WIDTH / width;
		} else if (config.fit == conf_fit_dblwidth) {
			config.scale = 960 / width;

			if (config.scale > 200)
				config.scale = (config.scale / 50) * 50;
			else {
				config.scale = (config.scale / 10) * 10;
				if (config.scale < 10)
					config.scale = 10;
			}

			width_rotated = 960;
			height_rotated = height * 960 / width;
		} else if (config.fit == conf_fit_dblheight) {
			config.scale = imgh / height;

			if (config.scale > 200)
				config.scale = (config.scale / 50) * 50;
			else {
				config.scale = (config.scale / 10) * 10;
				if (config.scale < 10)
					config.scale = 10;
			}

			height_rotated = imgh * 2;
			width_rotated = width * imgh * 2 / height;
		} else {
			config.scale = imgh / height;

			if (config.scale > 200)
				config.scale = (config.scale / 50) * 50;
			else {
				config.scale = (config.scale / 10) * 10;
				if (config.scale < 10)
					config.scale = 10;
			}

			height_rotated = imgh;
			width_rotated = width * imgh / height;
		}

		imgshow = (pixel *) memalign(16, sizeof(pixel) * width_rotated * height_rotated);

		if (imgshow != NULL) {
			if (config.bicubic)
				image_zoom_bicubic(imgdata, width, height, imgshow, width_rotated, height_rotated);
			else
				image_zoom_bilinear(imgdata, width, height, imgshow, width_rotated, height_rotated);
		} else {
			imgshow = imgdata;
			width_rotated = width;
			height_rotated = height;
		}
	} else {
		config.scale = 100;
		imgshow = imgdata;
		width_rotated = width;
		height_rotated = height;
	}

	curleft = curtop = 0;
	xpos = (int) config.viewpos % 3;
	ypos = (int) config.viewpos / 3;

	if (width_rotated < PSP_SCREEN_WIDTH)
		paintleft = (PSP_SCREEN_WIDTH - width_rotated) / 2;
	else {
		paintleft = 0;
		switch (xpos) {
			case 1:
				curleft = (width_rotated - PSP_SCREEN_WIDTH) / 2;
				break;
			case 2:
				curleft = width_rotated - PSP_SCREEN_WIDTH;
				break;
		}
	}

	if (height_rotated < imgh)
		painttop = (imgh - height_rotated) / 2;
	else {
		painttop = 0;
		switch (ypos) {
			case 1:
				curtop = (height_rotated - imgh) / 2;
				break;
			case 2:
				curtop = height_rotated - imgh;
				break;
		}
	}

	if (width > height) {
		thumb_width = 128;
		thumb_height = height * 128 / width;
	} else {
		thumb_height = 128;
		thumb_width = width * 128 / height;
	}

	image_zoom_bilinear(imgdata, width, height, thumbimg, thumb_width, thumb_height);

	if (slideshow)
		lasttime = time(NULL);

	return 0;
}

static void scene_show_info(int selidx)
{
	char infostr[64];
	int ilen;

	if (config.fit == conf_fit_custom)
		SPRINTF_S(infostr, _("%dx%d  %d%%  旋转角度 %d  %s"),
				  (int) width_rotated, (int) height_rotated, (int) config.scale, (int) oldangle, config.bicubic ? _("三次立方") : _("两次线性"));
	else
		SPRINTF_S(infostr, _("%dx%d  %s  旋转角度 %d  %s"),
				  (int) width_rotated, (int) height_rotated, conf_get_fitname(config.fit), (int) oldangle, config.bicubic ? _("三次立方") : _("两次线性"));

	ilen = strlen(infostr);

	if (config.imginfobar) {
		disp_fillrect(0, PSP_SCREEN_HEIGHT - DISP_FONTSIZE, 479, 271, 0);
		disp_putnstring(0, PSP_SCREEN_HEIGHT - DISP_FONTSIZE,
						COLOR_WHITE, (const u8 *) g_menu->root[selidx].name, 960 / DISP_FONTSIZE - ilen - 1, 0, 0, DISP_FONTSIZE, 0);
		disp_putnstring(PSP_SCREEN_WIDTH -
						DISP_FONTSIZE / 2 * ilen, PSP_SCREEN_HEIGHT - DISP_FONTSIZE, COLOR_WHITE, (const u8 *) infostr, ilen, 0, 0, DISP_FONTSIZE, 0);
	} else {
		disp_fillrect(11, 11, 10 + DISP_FONTSIZE / 2 * ilen, 10 + DISP_FONTSIZE, 0);
		disp_putnstring(11, 11, COLOR_WHITE, (const u8 *) infostr, ilen, 0, 0, DISP_FONTSIZE, 0);
	}
}

static void scene_show_thumb(void)
{
	short b;
	u32 top = (PSP_SCREEN_HEIGHT - thumb_height) / 2, bottom = top + thumb_height;
	u32 thumbl = 0, thumbr = 0, thumbt = 0, thumbb = 0;

	if (config.imginfobar) {
		return;
	}

	if (paintleft > 0) {
		thumbl = 0;
		thumbr = thumb_width - 1;
	} else {
		thumbl = curleft * thumb_width / width_rotated;
		thumbr = (curleft + PSP_SCREEN_WIDTH - 1) * thumb_width / width_rotated;
	}

	if (painttop > 0) {
		thumbt = 0;
		thumbb = thumbb - 1;
	} else {
		thumbt = curtop * thumb_height / height_rotated;
		thumbb = (curtop + imgh - 1) * thumb_height / height_rotated;
	}

	disp_putimage(32, top, thumb_width, thumb_height, 0, 0, thumbimg);
	disp_line(34, bottom, 32 + thumb_width, bottom, 0);
	disp_line(32 + thumb_width, top + 2, 32 + thumb_width, bottom - 1, 0);
	disp_rectangle(33 + thumbl, top + thumbt + 1, 33 + thumbr, top + thumbb + 1, 0);
	b = 75 - config.imgbrightness > 0 ? 75 - config.imgbrightness : 0;
	disp_rectangle(32 + thumbl, top + thumbt, 32 + thumbr, top + thumbb, disp_grayscale(COLOR_WHITE, 0, 0, 0, b));
}

static int scene_printimage(int selidx)
{
	disp_waitv();
	disp_fillvram(bgcolor);
	disp_putimage(paintleft, painttop, width_rotated, height_rotated, curleft, curtop, imgshow);

	if ((config.thumb == conf_thumb_always || thumb)) {
		scene_show_thumb();
	}

	if (config.imginfobar || showinfo) {
		scene_show_info(selidx);
	}

	disp_flip();

	return 0;
}

static void image_up(void)
{
	if (curtop > 0) {
		curtop -= (int) config.imgmvspd * 2;
		if (curtop < 0)
			curtop = 0;
		img_needrp = true;
	}
}

static void image_down(void)
{
	if (height_rotated > imgh && curtop < height_rotated - imgh) {
		curtop += (int) config.imgmvspd * 2;
		if (curtop > height_rotated - imgh)
			curtop = height_rotated - imgh;
		img_needrp = true;
	}
}

static void image_left(void)
{
	if (curleft > 0) {
		curleft -= (int) config.imgmvspd * 2;
		if (curleft < 0)
			curleft = 0;
		img_needrp = true;
	}
}

static void image_right(void)
{
	if (width_rotated > PSP_SCREEN_WIDTH && curleft < width_rotated - PSP_SCREEN_WIDTH) {
		curleft += (int) config.imgmvspd * 2;
		if (curleft > width_rotated - PSP_SCREEN_WIDTH)
			curleft = width_rotated - PSP_SCREEN_WIDTH;
		img_needrp = true;
	}
}

static void image_move(u32 key)
{
	// cancel z mode
	in_move_z_mode = false;

	if ((key & config.imgkey[14] && !(key & config.imgkey[15]))
		|| (key & config.imgkey2[14] && !(key & config.imgkey2[15]))
		) {
		image_left();
	}

	if ((key & config.imgkey[15] && !(key & config.imgkey[14])) || (key & config.imgkey2[15] && !(key & config.imgkey2[14]))
		) {
		image_right();
	}

	if ((key & config.imgkey[12] && !(key & config.imgkey[13])) || (key & config.imgkey2[12] && !(key & config.imgkey2[13]))
		) {
		image_up();
	}

	if ((key & config.imgkey[13] && !(key & config.imgkey[12])) || (key & config.imgkey2[13] && !(key & config.imgkey2[12]))) {
		image_down();
	}

	thumb = (config.thumb == conf_thumb_scroll);
	img_needrp = thumb | img_needrp;
}

static bool image_paging_movedown(void)
{
	if (curtop + imgh < height_rotated) {
		curtop += max(imgh - (int) config.imgpagereserve, 0);

		if (curtop + imgh > height_rotated)
			curtop = height_rotated - imgh;

		img_needrp = true;

		return false;
	}

	return true;
}

static bool image_paging_moveup(void)
{
	if (curtop > 0) {
		curtop -= max(imgh - (int) config.imgpagereserve, 0);

		if (curtop < 0)
			curtop = 0;

		img_needrp = true;

		return false;
	}

	return true;
}

static bool image_paging_moveright(void)
{
	if (curleft + PSP_SCREEN_WIDTH < width_rotated) {
		curleft += PSP_SCREEN_WIDTH;

		if (curleft + PSP_SCREEN_WIDTH > width_rotated)
			curleft = width_rotated - PSP_SCREEN_WIDTH;

		img_needrp = true;

		return false;
	}

	return true;
}

static bool image_paging_moveleft(void)
{
	if (curleft > 0) {
		curleft -= PSP_SCREEN_WIDTH;

		if (curleft < 0)
			curleft = 0;

		img_needrp = true;

		return false;
	}

	return true;
}

static bool image_paging_movedown_smooth(void)
{
	if (curtop + imgh < height_rotated) {
		curtop += config.image_scroll_chgn_speed ? (int) g_current_spd : (int)
			config.imgpaging_spd;

		if (curtop > height_rotated - imgh)
			curtop = height_rotated - imgh;

		img_needrp = true;

		return false;
	}

	return true;
}

static bool image_paging_moveup_smooth(void)
{
	if (curtop > 0) {
		curtop -= config.image_scroll_chgn_speed ? (int) g_current_spd : (int)
			config.imgpaging_spd;

		if (curtop < 0)
			curtop = 0;

		img_needrp = true;

		return false;
	}

	return true;
}

static bool image_paging_moveright_smooth(void)
{
	if (curleft + PSP_SCREEN_WIDTH < width_rotated) {
		curleft += config.image_scroll_chgn_speed ? (int) g_current_spd : (int)
			config.imgpaging_spd;

		if (curleft > width_rotated - PSP_SCREEN_WIDTH)
			curleft = width_rotated - PSP_SCREEN_WIDTH;

		img_needrp = true;

		return false;
	}

	return true;
}

static bool image_paging_moveleft_smooth(void)
{
	if (curleft > 0) {
		curleft -= config.image_scroll_chgn_speed ? (int) g_current_spd : (int)
			config.imgpaging_spd;

		if (curleft < 0)
			curleft = 0;

		img_needrp = true;

		return false;
	}

	return true;
}

static bool image_paging_updown(bool is_forward)
{
	switch (ypos) {
		case 0:
		case 1:
			if (is_forward) {
				if (!image_paging_movedown()) {
					return false;
				}
			} else {
				if (!image_paging_moveup()) {
					return false;
				}
			}
			break;
		case 2:
			if (is_forward) {
				if (!image_paging_moveup()) {
					return false;
				}
			} else {
				if (!image_paging_movedown()) {
					return false;
				}
			}
			break;
	}

	if (is_forward)
		curtop = (height_rotated > imgh && ypos == 2) ? height_rotated - imgh : 0;
	else
		curtop = (height_rotated > imgh && ypos < 2) ? height_rotated - imgh : 0;

	switch (xpos) {
		case 0:
		case 1:
			if (is_forward) {
				if (!image_paging_moveright()) {
					return false;
				}
			} else {
				if (!image_paging_moveleft()) {
					return false;
				}
			}
			break;
		case 2:
			if (is_forward) {
				if (!image_paging_moveleft()) {
					return false;
				}
			} else {
				if (!image_paging_moveright()) {
					return false;
				}
			}
			break;
	}

	return true;
}

static bool image_paging_leftright(bool is_forward)
{
	switch (xpos) {
		case 0:
		case 1:
			if (is_forward) {
				if (!image_paging_moveright()) {
					return false;
				}
			} else {
				if (!image_paging_moveleft()) {
					return false;
				}
			}
			break;
		case 2:
			if (is_forward) {
				if (!image_paging_moveleft()) {
					return false;
				}
			} else {
				if (!image_paging_moveright()) {
					return false;
				}
			}
			break;
	}

	if (is_forward)
		curleft = (width_rotated > PSP_SCREEN_WIDTH && xpos == 2) ? width_rotated - PSP_SCREEN_WIDTH : 0;
	else
		curleft = (width_rotated > PSP_SCREEN_WIDTH && xpos < 2) ? width_rotated - PSP_SCREEN_WIDTH : 0;

	switch (ypos) {
		case 0:
		case 1:
			if (is_forward) {
				if (!image_paging_movedown()) {
					return false;
				}
			} else {
				if (!image_paging_moveup()) {
					return false;
				}
			}
			break;
		case 2:
			if (is_forward) {
				if (!image_paging_moveup()) {
					return false;
				}
			} else {
				if (!image_paging_movedown()) {
					return false;
				}
			}
			break;
	}

	return true;
}

static bool is_need_delay(void)
{
	static u64 start, end;

	if (g_imgpaging_count == 0)
		sceRtcGetCurrentTick(&start);

	sceRtcGetCurrentTick(&end);

	if (pspDiffTime(&start, &end) >= 0.1) {
		g_imgpaging_count++;
		sceRtcGetCurrentTick(&start);
	}

	if (config.imgpaging_duration <= 1 || g_imgpaging_count < config.imgpaging_duration) {
		if (config.image_scroll_chgn_speed && config.imgpaging_duration > 1) {
			if (g_imgpaging_count <= config.imgpaging_duration / 2)
				g_current_spd = config.imgpaging_spd * 2 * g_imgpaging_count / config.imgpaging_duration;
			else
				g_current_spd =
					config.imgpaging_spd - config.imgpaging_spd * 2 * (g_imgpaging_count - config.imgpaging_duration / 2) / config.imgpaging_duration;

			g_current_spd = max(g_current_spd, 1);
		} else
			g_current_spd = config.imgpaging_spd;

		return false;
	} else {
		if (g_imgpaging_count >= config.imgpaging_duration + config.imgpaging_interval) {
			g_imgpaging_count = 0;
		}

		sceKernelDelayThread(100000);

		return true;
	}

	return false;
}

static bool splashz(void)
{
	static u64 start, end;
	double s, t, t2;

	if (z_mode_cnt == 0)
		sceRtcGetCurrentTick(&start);

	sceRtcGetCurrentTick(&end);

	if (pspDiffTime(&start, &end) >= 0.1) {
		z_mode_cnt++;
		sceRtcGetCurrentTick(&start);
	}

	s = sqrt(1. * (destx - srcx) * (destx - srcx) + (desty - srcy) * (desty - srcy));

	if (config.imgpaging_spd)
		t = s / config.imgpaging_spd;
	else
		t = 0;

	if (z_mode_cnt <= t + 1) {
		if (s) {
			if (destx > srcx)
				t2 = srcx + z_mode_cnt * (destx - srcx) * config.imgpaging_spd / s;
			else
				t2 = srcx - z_mode_cnt * (srcx - destx) * config.imgpaging_spd / s;
			curleft = (int) t2;
			if (destx - srcx)
				curtop = srcy + (desty - srcy) * (t2 - srcx) / (destx - srcx);
			else
				goto finish;
		} else {
			goto finish;
		}

		if (curleft < 0) {
			curleft = 0;
		}

		if (curleft + PSP_SCREEN_WIDTH > width_rotated) {
			curleft = width_rotated - PSP_SCREEN_WIDTH;
		}

		if (curtop < 0) {
			curtop = 0;
		}

		if (curtop > height_rotated - imgh) {
			curtop = height_rotated - imgh;
		}

		img_needrp = true;
	} else {
	  finish:
		in_move_z_mode = false;
		z_mode_cnt = 0;
		srcx = 0;
		srcy = 0;
		curleft = destx;
		curtop = desty;
		destx = 0;
		desty = 0;
	}

	return false;
}

#define MAGNETIC_X_BORDER (PSP_SCREEN_WIDTH / 4)
#define MAGNETIC_Y_BORDER (PSP_SCREEN_HEIGHT / 4)

static bool z_mode_up(void)
{
	desty = curtop + max(imgh - (int) config.imgpagereserve, 0);

	if (desty + imgh > height_rotated)
		desty = height_rotated - imgh;

	if (config.magnetic_scrolling) {
		if (desty > height_rotated - imgh - MAGNETIC_Y_BORDER)
			desty = height_rotated - imgh;
	}

	if (curtop + imgh < height_rotated) {
		return false;
	}

	return true;
}

static bool z_mode_down(void)
{
	desty = curtop - max(imgh - (int) config.imgpagereserve, 0);

	if (desty < 0)
		desty = 0;

	if (config.magnetic_scrolling) {
		if (desty < MAGNETIC_Y_BORDER)
			desty = 0;
	}

	if (curtop > 0) {
		return false;
	}

	return true;
}

static bool z_mode_left(void)
{
	destx = curleft - max(PSP_SCREEN_WIDTH - (int) config.imgpagereserve, 0);

	if (destx < 0) {
		destx = 0;
	}

	if (config.magnetic_scrolling) {
		if (destx < MAGNETIC_X_BORDER)
			destx = 0;
	}

	if (curleft > 0) {
		return false;
	}

	return true;
}

static bool z_mode_right(void)
{
	destx = curleft + max(PSP_SCREEN_WIDTH - (int) config.imgpagereserve, 0);

	if (destx + PSP_SCREEN_WIDTH > width_rotated) {
		destx = width_rotated - PSP_SCREEN_WIDTH;
	}

	if (config.magnetic_scrolling) {
		if (destx > width_rotated - PSP_SCREEN_WIDTH - MAGNETIC_X_BORDER)
			destx = width_rotated - PSP_SCREEN_WIDTH;
	}

	if (curleft + PSP_SCREEN_WIDTH < width_rotated) {
		return false;
	}

	return true;
}

bool enter_z_mode(bool is_forward, bool leftright)
{
	in_move_z_mode = true;
	srcx = curleft;
	srcy = curtop;

	if (leftright) {
		if (is_forward) {
			destx = (width_rotated > PSP_SCREEN_WIDTH && xpos == 2) ? width_rotated - PSP_SCREEN_WIDTH : 0;
		} else {
			destx = (width_rotated > PSP_SCREEN_WIDTH && xpos < 2) ? width_rotated - PSP_SCREEN_WIDTH : 0;
		}
	} else {
		if (is_forward) {
			desty = (height_rotated > imgh && ypos == 2) ? height_rotated - imgh : 0;
		} else {
			desty = (height_rotated > imgh && ypos < 2) ? height_rotated - imgh : 0;
		}
	}

	if (leftright) {
		switch (ypos) {
			case 0:
			case 1:
				if (is_forward) {
					if (!z_mode_up())
						return false;
				} else {
					if (!z_mode_down())
						return false;
				}
				break;
			case 2:
				if (is_forward) {
					if (!z_mode_down())
						return false;
				} else {
					if (!z_mode_up())
						return false;
				}
				break;
		}
	} else {
		switch (xpos) {
			case 0:
			case 1:
				if (is_forward) {
					if (!z_mode_right())
						return false;
				} else {
					if (!z_mode_left())
						return false;
				}
				break;
			case 2:
				if (is_forward) {
					if (!z_mode_left())
						return false;
				} else {
					if (!z_mode_right())
						return false;
				}
				break;
		}
	}

	in_move_z_mode = false;
	srcx = 0;
	srcy = 0;
	curleft = 0;
	curtop = 0;
	destx = 0;
	desty = 0;

	return true;
}

static bool image_paging_leftright_smooth(bool is_forward)
{
	if (in_move_z_mode)
		return splashz();

	if (is_need_delay())
		return false;

	switch (xpos) {
		case 0:
		case 1:
			if (is_forward) {
				if (!image_paging_moveright_smooth()) {
					return false;
				}
			} else {
				if (!image_paging_moveleft_smooth()) {
					return false;
				}
			}
			break;
		case 2:
			if (is_forward) {
				if (!image_paging_moveleft_smooth()) {
					return false;
				}
			} else {
				if (!image_paging_moveright_smooth()) {
					return false;
				}
			}
			break;
	}

	g_imgpaging_count = config.imgpaging_duration;
	sceKernelDelayThread(100000 * config.imgpaging_duration);

	if (!in_move_z_mode) {
		return enter_z_mode(is_forward, true);
	}

	return true;
}

static bool image_paging_updown_smooth(bool is_forward)
{
	if (in_move_z_mode)
		return splashz();

	if (is_need_delay())
		return false;

	switch (ypos) {
		case 0:
		case 1:
			if (is_forward) {
				if (!image_paging_movedown_smooth()) {
					return false;
				}
			} else {
				if (!image_paging_moveup_smooth()) {
					return false;
				}
			}
			break;
		case 2:
			if (is_forward) {
				if (!image_paging_moveup_smooth()) {
					return false;
				}
			} else {
				if (!image_paging_movedown_smooth()) {
					return false;
				}
			}
			break;
	}

	g_imgpaging_count = config.imgpaging_duration;
	sceKernelDelayThread(100000 * config.imgpaging_duration);

	if (!in_move_z_mode) {
		return enter_z_mode(is_forward, false);
	}

	return true;
}

/**
 * 处理图像卷动方式
 * @param is_forward 是否为前进
 * @param type 图像卷动方式
 * @return 是否需要重新加载图像
 */
static bool image_paging(bool is_forward, t_conf_imgpaging type)
{
	switch (type) {
		case conf_imgpaging_direct:
			return true;
			break;
		case conf_imgpaging_updown:
			return image_paging_updown(is_forward);
			break;
		case conf_imgpaging_leftright:
			return image_paging_leftright(is_forward);
			break;
		case conf_imgpaging_updown_smooth:
			return image_paging_updown_smooth(is_forward);
			break;
		case conf_imgpaging_leftright_smooth:
			return image_paging_leftright_smooth(is_forward);
			break;
		default:
			break;
	}

	return false;
}

static void next_image(u32 * selidx, bool * should_exit)
{
	u32 orgidx = *selidx;

	cache_set_forward(true);
	ctrl_waitrelease();

	do {
		if (*selidx < g_menu->size - 1)
			(*selidx)++;
		else {
			if (config.img_no_repeat == false) {
				*selidx = 0;
			} else {
				*should_exit = true;
				return;
			}
		}
	} while (!fs_is_image((t_fs_filetype) g_menu->root[*selidx].data));

	if (*selidx != orgidx)
		img_needrf = img_needrc = img_needrp = true;

	in_move_z_mode = false;
	z_mode_cnt = 0;

	cache_next_image();
	cache_delete_first();
}

static void prev_image(u32 * selidx)
{
	u32 orgidx = *selidx;

	cache_set_forward(false);
	ctrl_waitrelease();

	do {
		if (*selidx > 0)
			(*selidx)--;
		else
			*selidx = g_menu->size - 1;
	} while (!fs_is_image((t_fs_filetype) g_menu->root[*selidx].data));

	if (*selidx != orgidx)
		img_needrf = img_needrc = img_needrp = true;

	in_move_z_mode = false;
	z_mode_cnt = 0;

	cache_next_image();
	cache_delete_first();
}

static bool slideshow_move = false;

static int image_handle_input(u32 * selidx, u32 key)
{
	slideshow_move = false;

#ifdef ENABLE_ANALOG
	if (config.img_enable_analog) {
		int x, y, orgtop = curtop, orgleft = curleft;

		if (ctrl_analog(&x, &y)) {
			slideshow_move = true;
			x = x / 31 * (int) config.imgmvspd / 2;
			y = y / 31 * (int) config.imgmvspd / 2;
			curtop += y;

			if (curtop + imgh > height_rotated)
				curtop = (int) height_rotated - imgh;

			if (curtop < 0)
				curtop = 0;

			curleft += x;

			if (curleft + PSP_SCREEN_WIDTH > width_rotated)
				curleft = (int) width_rotated - PSP_SCREEN_WIDTH;

			if (curleft < 0)
				curleft = 0;

			thumb = (config.thumb == conf_thumb_scroll);
			img_needrp = (thumb || orgtop != curtop || orgleft != curleft);
		}
	}
#endif

	if (key == 0)
		goto next;

	if (!slideshow || (key != CTRL_FORWARD && key != 0))
		secticks = 0;

	if (key == (PSP_CTRL_SELECT | PSP_CTRL_START)) {
		return exit_confirm();
	} else if (key == PSP_CTRL_SELECT) {
		bool lastbicubic = config.bicubic;

		img_needrp = true;

		if (scene_options(selidx)) {
			imgreading = false;
			reset_image_ptr();

			return *selidx;
		}

		if (lastbicubic != config.bicubic)
			img_needrc = true;

		if (config.imginfobar)
			imgh = PSP_SCREEN_HEIGHT - DISP_FONTSIZE;
		else
			imgh = PSP_SCREEN_HEIGHT;

	} else if (key == PSP_CTRL_START) {
		scene_mp3bar();
		img_needrp = true;
	} else if (key == config.imgkey[1] || key == config.imgkey2[1]
			   || key == CTRL_FORWARD) {
		bool should_exit = false;

		if (config.imgpaging == conf_imgpaging_updown || config.imgpaging == conf_imgpaging_leftright)
			sceKernelDelayThread(200000);

		if (!image_paging(true, config.imgpaging))
			goto next;

		next_image(selidx, &should_exit);

		if (should_exit) {
			return *selidx;
		}
	} else if (key == config.imgkey[0] || key == config.imgkey2[0]
			   || key == CTRL_BACK) {
		if (config.imgpaging == conf_imgpaging_updown || config.imgpaging == conf_imgpaging_leftright)
			sceKernelDelayThread(200000);

		if (!image_paging(false, config.imgpaging))
			goto next;

		prev_image(selidx);
	} else if (key == config.imgkey[2] || key == config.imgkey2[2]) {
		ctrl_waitrelease();

		if (config.fit == conf_fit_custom)
			config.fit = conf_fit_none;
		else
			config.fit++;

		img_needrc = img_needrp = true;
	} else if (key == config.imgkey[10] || key == config.imgkey2[10]) {
		config.bicubic = !config.bicubic;
		img_needrc = img_needrp = true;
	} else if (key == config.imgkey[11] || key == config.imgkey2[11]) {
		if (!slideshow) {
			slideshow = true;
			lasttime = time(NULL);
			ctrl_waitrelease();
		} else {
			slideshow = false;
			win_msg(_("幻灯片播放已经停止！"), COLOR_WHITE, COLOR_WHITE, config.msgbcolor);
			ctrl_waitrelease();
		}
	} else if (key == config.imgkey[7] || key == config.imgkey2[7]) {
		SceCtrlData ctl;
		int t = 0;

		config.imginfobar = !config.imginfobar;

		if (config.imginfobar)
			imgh = PSP_SCREEN_HEIGHT - DISP_FONTSIZE;
		else
			imgh = PSP_SCREEN_HEIGHT;

		if (height_rotated > imgh && curtop > height_rotated - imgh)
			curtop = height_rotated - imgh;

		img_needrc = (config.fit == conf_fit_height);
		img_needrp = true;

		do {
			sceCtrlReadBufferPositive(&ctl, 1);
			sceKernelDelayThread(10000);
			t += 10000;
		} while (ctl.Buttons != 0 && t <= 500000);
	} else if (key == config.imgkey[9] || key == config.imgkey2[9]
			   || key == CTRL_PLAYPAUSE) {
		if (slideshow) {
			slideshow = false;
			win_msg(_("幻灯片播放已经停止！"), COLOR_WHITE, COLOR_WHITE, config.msgbcolor);
			ctrl_waitrelease();
		} else {
			imgreading = false;
			reset_image_ptr();

			return *selidx;
		}
	} else if (key == config.imgkey[8] || key == config.imgkey2[8]) {
		if (!slideshow && !showinfo) {
			img_needrp = true;
			showinfo = true;
		}
	} else if (key == config.imgkey[5] || key == config.imgkey2[5]) {
		if (config.rotate == conf_rotate_0)
			config.rotate = conf_rotate_270;
		else
			config.rotate--;

		ctrl_waitreleasekey(key);
		img_needrc = img_needrp = true;
	} else if (key == config.imgkey[6] || key == config.imgkey2[6]) {
		if (config.rotate == conf_rotate_270)
			config.rotate = conf_rotate_0;
		else
			config.rotate++;

		ctrl_waitreleasekey(key);
		img_needrc = img_needrp = true;
	} else if (key == config.imgkey[3] || key == config.imgkey2[3]) {
		if (config.scale > 200)
			config.scale -= 50;
		else if (config.scale > 10)
			config.scale -= 10;
		else
			goto next;

		config.fit = conf_fit_custom;
		img_needrc = img_needrp = true;
		ctrl_waitreleasekey(key);
	} else if (key == config.imgkey[4] || key == config.imgkey2[4]) {
		if (config.scale < 200)
			config.scale += 10;
		else if (config.scale < 1000)
			config.scale += 50;
		else
			goto next;

		config.fit = conf_fit_custom;
		img_needrc = img_needrp = true;
		ctrl_waitreleasekey(key);
	} else if (key &
			   (config.imgkey[12] | config.imgkey[13] | config.imgkey[14] | config.imgkey[15] | config.imgkey2[12] | config.
				imgkey2[13] | config.imgkey2[14] | config.imgkey2[15])
			   && !(key &
					~(config.imgkey[12] | config.imgkey[13] | config.imgkey[14] | config.imgkey[15] | config.imgkey2[12] | config.imgkey2[13] | config.
					  imgkey2[14] | config.imgkey2[15]
					))) {
		slideshow_move = true;
		image_move(key);
	} else
		img_needrf = img_needrc = false;

  next:
	return -1;
}

static void scene_image_delay_action(void)
{
	if (config.dis_scrsave)
		scePowerTick(0);
}

static int scene_slideshow_forward(u32 * selidx)
{
	bool should_exit = false;

	if (config.imgpaging == conf_imgpaging_updown || config.imgpaging == conf_imgpaging_leftright) {
		sceKernelDelayThread(200000);
	}

	if (!image_paging(true, config.imgpaging)) {
		return -1;
	}

	next_image(selidx, &should_exit);

	if (should_exit) {
		return *selidx;
	}

	return -1;
}

u32 scene_readimage(u32 selidx)
{
	u64 timer_start, timer_end;
	u64 slide_start, slide_end;

	width_rotated = 0, height_rotated = 0, thumb_width = 0, thumb_height = 0, paintleft = 0, painttop = 0;
	imgdata = NULL, imgshow = NULL;
	oldangle = 0;
	curtop = 0, curleft = 0, xpos = 0, ypos = 0;
	img_needrf = true, img_needrc = true, img_needrp = true, showinfo = false, thumb = false;
	slideshow = false;
	now = 0, lasttime = 0;
	imgreading = true;

	if (config.imginfobar)
		imgh = PSP_SCREEN_HEIGHT - DISP_FONTSIZE;
	else
		imgh = PSP_SCREEN_HEIGHT;

	cache_setup(config.max_cache_img, &selidx);
	cache_set_forward(true);
	cache_on(true);

	sceRtcGetCurrentTick(&timer_start);

	while (1) {
		u64 dbgnow, dbglasttick;
		u32 key = 0;
		int ret;

		if (img_needrf) {
			int fid;
			u32 ret;

			fid = freq_enter_hotzone();
			sceRtcGetCurrentTick(&dbglasttick);
			ret = scene_reloadimage(selidx);

			if (ret == -1) {
				freq_leave(fid);
				break;
			}

			img_needrf = false;
			sceRtcGetCurrentTick(&dbgnow);
			dbg_printf(d, _("装载图像时间: %.2f秒"), pspDiffTime(&dbgnow, &dbglasttick));
			freq_leave(fid);
		}

		if (img_needrc) {
			int fid;

			fid = freq_enter_hotzone();
			sceRtcGetCurrentTick(&dbglasttick);
			scene_rotateimage();
			img_needrc = false;
			sceRtcGetCurrentTick(&dbgnow);
			dbg_printf(d, _("旋转图像时间: %.2f秒"), pspDiffTime(&dbgnow, &dbglasttick));
			freq_leave(fid);
		}

		if (img_needrp) {
			scene_printimage(selidx);
			img_needrp = false;
		}

		now = time(NULL);

		if (config.thumb == conf_thumb_scroll && thumb) {
			thumb = false;
			img_needrp = true;
		}

		key = ctrl_read_cont();
		ret = image_handle_input(&selidx, key);

		if (ret == -1 && slideshow && !slideshow_move) {
			if (key == PSP_CTRL_CIRCLE) {
			} else if (key != 0 && (key == config.imgkey[1] || key == config.imgkey2[1]
									|| key == CTRL_FORWARD)) {
				bool should_exit = false;

				next_image(&selidx, &should_exit);

				if (should_exit) {
					break;
				}
			} else if (key != 0 && (key == config.imgkey[0] || key == config.imgkey2[0]
									|| key == CTRL_BACK)) {
				prev_image(&selidx);
			} else {
				scePowerTick(0);
				if (config.imgpaging == conf_imgpaging_direct || config.imgpaging == conf_imgpaging_updown || config.imgpaging == conf_imgpaging_leftright) {
					if (now - lasttime >= config.slideinterval) {
						lasttime = now;
						ret = scene_slideshow_forward(&selidx);
					}
				} else {
					sceRtcGetCurrentTick(&slide_end);
					if (pspDiffTime(&slide_end, &slide_start) >= 0.1) {
						sceRtcGetCurrentTick(&slide_start);
					} else {
						lasttime = now;
						ret = scene_slideshow_forward(&selidx);
					}
				}
			}
		}

		if (showinfo && (key & PSP_CTRL_CIRCLE) == 0) {
			img_needrp = true;
			showinfo = false;
		}

		sceRtcGetCurrentTick(&timer_end);

		if (pspDiffTime(&timer_end, &timer_start) >= 1.0) {
			sceRtcGetCurrentTick(&timer_start);
			secticks++;
		}

		if (config.autosleep != 0 && secticks > 60 * config.autosleep) {
			power_down();
			scePowerRequestSuspend();
			secticks = 0;
		}

		if (ret != -1) {
			selidx = ret;
			break;
		}

		scene_image_delay_action();
	}

	reset_image_show_ptr();

	cache_on(false);
	imgreading = false;

	return selidx;
}

static t_win_menu_op scene_imgkey_menucb(u32 key, p_win_menuitem item, u32 * count, u32 max_height, u32 * topindex, u32 * index)
{
	u32 key1, key2;
	SceCtrlData ctl;
	int i;

	switch (key) {
		case (PSP_CTRL_SELECT | PSP_CTRL_START):
			return exit_confirm();
		case PSP_CTRL_CIRCLE:
			disp_duptocache();
			disp_waitv();
			prompt_press_any_key();
			disp_flip();

			ctrl_waitrelease();
			do {
				sceCtrlReadBufferPositive(&ctl, 1);
				key1 = (ctl.Buttons & ~PSP_CTRL_SELECT) & ~PSP_CTRL_START;
			} while (key1 == 0);
			key2 = key1;
			while ((key2 & key1) == key1) {
				key1 = key2;
				sceCtrlReadBufferPositive(&ctl, 1);
				key2 = (ctl.Buttons & ~PSP_CTRL_SELECT) & ~PSP_CTRL_START;
			}
			if (config.imgkey[*index] == key1 || config.imgkey2[*index] == key1)
				return win_menu_op_force_redraw;

			for (i = 0; i < NELEMS(config.imgkey); i++) {
				if (i == *index)
					continue;
				if (config.imgkey[i] == key1) {
					config.imgkey[i] = config.imgkey2[*index];
					if (config.imgkey[i] == 0) {
						config.imgkey[i] = config.imgkey2[i];
						config.imgkey2[i] = 0;
					}
					break;
				}
				if (config.imgkey2[i] == key1) {
					config.imgkey2[i] = config.imgkey2[*index];
					break;
				}
			}
			config.imgkey2[*index] = config.imgkey[*index];
			config.imgkey[*index] = key1;
			do {
				sceCtrlReadBufferPositive(&ctl, 1);
			} while (ctl.Buttons != 0);
			return win_menu_op_force_redraw;
		case PSP_CTRL_TRIANGLE:
			config.imgkey[*index] = config.imgkey2[*index];
			config.imgkey2[*index] = 0;
			return win_menu_op_redraw;
		case PSP_CTRL_SQUARE:
			return win_menu_op_cancel;
		default:;
	}

	return win_menu_defcb(key, item, count, max_height, topindex, index);
}

static void scene_imgkey_predraw(p_win_menuitem item, u32 index, u32 topindex, u32 max_height)
{
	char keyname[256];
	int left, right, upper, bottom, lines = 0;
	u32 i;

	default_predraw(&g_predraw, _("按键设置   △ 删除"), max_height, &left, &right, &upper, &bottom, 8 * DISP_FONTSIZE + 4);

	for (i = topindex; i < topindex + max_height; i++) {
		conf_get_keyname(config.imgkey[i], keyname);
		if (config.imgkey2[i] != 0) {
			char keyname2[256];

			conf_get_keyname(config.imgkey2[i], keyname2);
			STRCAT_S(keyname, " | ");
			STRCAT_S(keyname, keyname2);
		}
		disp_putstring(left + (right - left) / 2, upper + 2 + (lines + 1 + g_predraw.linespace) * (1 + DISP_FONTSIZE), COLOR_WHITE, (const u8 *) keyname);
		lines++;
	}
}

u32 scene_imgkey(u32 * selidx)
{
	win_menu_predraw_data prev;
	t_win_menuitem item[16];
	u32 i, index;

	memcpy(&prev, &g_predraw, sizeof(win_menu_predraw_data));

	STRCPY_S(item[0].name, _("上一张图"));
	STRCPY_S(item[1].name, _("下一张图"));
	STRCPY_S(item[2].name, _("缩放模式"));
	STRCPY_S(item[3].name, _("缩小图片"));
	STRCPY_S(item[4].name, _("放大图片"));
	STRCPY_S(item[5].name, _("左旋90度"));
	STRCPY_S(item[6].name, _("右旋90度"));
	STRCPY_S(item[7].name, _("  信息栏"));
	STRCPY_S(item[8].name, _("显示信息"));
	STRCPY_S(item[9].name, _("退出浏览"));
	STRCPY_S(item[10].name, _("缩放引擎"));
	STRCPY_S(item[11].name, _("幻灯播放"));
	STRCPY_S(item[12].name, _("上"));
	STRCPY_S(item[13].name, _("下"));
	STRCPY_S(item[14].name, _("左"));
	STRCPY_S(item[15].name, _("右"));

	g_predraw.max_item_len = win_get_max_length(item, NELEMS(item));

	for (i = 0; i < NELEMS(item); i++) {
		item[i].width = g_predraw.max_item_len;
		item[i].selected = false;
		item[i].icolor = config.menutextcolor;
		item[i].selicolor = config.selicolor;
		item[i].selrcolor = config.menubcolor;
		item[i].selbcolor = config.selbcolor;
		item[i].data = NULL;
	}

	if (DISP_FONTSIZE >= 14)
		g_predraw.item_count = 12;
	else
		g_predraw.item_count = NELEMS(item);

	g_predraw.x = 240;
	g_predraw.y = 123;
	g_predraw.left = g_predraw.x - DISP_FONTSIZE * g_predraw.max_item_len / 2;
	g_predraw.upper = g_predraw.y - DISP_FONTSIZE * g_predraw.item_count / 2;
	g_predraw.linespace = 0;

	while ((index =
			win_menu(g_predraw.left,
					 g_predraw.upper, g_predraw.max_item_len,
					 g_predraw.item_count, item, g_predraw.item_count, 0,
					 g_predraw.linespace, config.menubcolor, true, scene_imgkey_predraw, NULL, scene_imgkey_menucb)) != INVALID);

	memcpy(&g_predraw, &prev, sizeof(win_menu_predraw_data));

	return 0;
}
#endif
