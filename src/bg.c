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

#ifdef ENABLE_BG

#include <stdlib.h>
#include <string.h>
#include "display.h"
#include "image.h"
#include "bg.h"
#include "config.h"
#include "scene.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

static pixel *bg_start = (pixel *) (0x44000000 + PSP_SCREEN_SCANLINE * PSP_SCREEN_HEIGHT * PIXEL_BYTES * 2);
static u32 imgwidth, imgheight;

static void bg_set_color(pixel * bg_addr, u32 bgcolor)
{
	pixel *bg_buf, *bg_end;

	for (bg_buf = bg_addr, bg_end = bg_addr + 0x22000; bg_buf < bg_end; bg_buf++)
		*bg_buf = bgcolor;
}

/// mix image and bgcolor to bg buffer
static void bg_set_image_grayscale(pixel * img_buf, u32 top, u32 left, u32 width, u32 height, u32 bgcolor, u32 grayscale)
{
	pixel *bg_buf, *bg_end, *bg_line, *bg_lineend;
	u32 r = RGB_R(bgcolor), g = RGB_G(bgcolor), b = RGB_B(bgcolor);

	for (bg_buf = bg_start + top * PSP_SCREEN_SCANLINE + left, bg_end = bg_buf + height * PSP_SCREEN_SCANLINE; bg_buf < bg_end; bg_buf += PSP_SCREEN_SCANLINE)
		for (bg_line = bg_buf, bg_lineend = bg_buf + width; bg_line < bg_lineend; bg_line++) {
			*bg_line = disp_grayscale(*img_buf, r, g, b, grayscale);
			img_buf++;
		}
}

extern void bg_load(const char *filename, const char *archname, pixel bgcolor, t_fs_filetype ft, u32 grayscale, int where)
{
	pixel *imgdata, *imgshow = NULL, *img_buf;
	u32 width, height, w2, h2, left, top;
	pixel bgc;
	int result;

	if (archname == NULL || archname[0] == '\0' || where == scene_in_dir)
		result = image_open_normal(filename, ft, &width, &height, &imgdata, &bgc);
	else
		result = image_open_archive(filename, archname, ft, &width, &height, &imgdata, &bgc, where);

	if (result != 0) {
		// if load bg image fail, continue to set bgcolor
		bg_set_color(bg_start, bgcolor);
		config.have_bg = true;
		return;
	}
	if (width > PSP_SCREEN_WIDTH) {
		h2 = height * PSP_SCREEN_WIDTH / width;
		if (h2 > PSP_SCREEN_HEIGHT) {
			h2 = PSP_SCREEN_HEIGHT;
			w2 = width * PSP_SCREEN_HEIGHT / height;
		} else
			w2 = PSP_SCREEN_WIDTH;
	} else if (height > PSP_SCREEN_HEIGHT) {
		h2 = PSP_SCREEN_HEIGHT;
		w2 = width * PSP_SCREEN_HEIGHT / height;
	} else {
		h2 = height;
		w2 = width;
	}
	if (width != w2 || height != h2) {
		imgshow = malloc(sizeof(pixel) * w2 * h2);
		if (imgshow == NULL) {
			free(imgdata);
			return;
		}
		image_zoom_bicubic(imgdata, width, height, imgshow, w2, h2);
	} else
		imgshow = imgdata;

	if (w2 < PSP_SCREEN_WIDTH)
		left = (PSP_SCREEN_WIDTH - w2) / 2;
	else
		left = 0;
	if (h2 < PSP_SCREEN_HEIGHT)
		top = (PSP_SCREEN_HEIGHT - h2) / 2;
	else
		top = 0;

	bg_set_color(bg_start, bgcolor);

	img_buf = imgshow;

	bg_set_image_grayscale(img_buf, top, left, w2, h2, bgcolor, grayscale);

	config.have_bg = true;

	if (imgshow != imgdata)
		free(imgshow);
	imgwidth = w2;
	imgheight = h2;
	free(imgdata);
}

extern bool bg_display(void)
{
	if (config.have_bg) {
		memcpy(vram_draw, bg_start, PSP_SCREEN_SCANLINE * PSP_SCREEN_HEIGHT * PIXEL_BYTES);
		return true;
	}
	return false;
}

extern void bg_cancel(void)
{
	config.have_bg = false;
}

static u8 *_cache = NULL;

extern void bg_cache(void)
{
	if (!config.have_bg)
		return;
	if (_cache != NULL)
		free(_cache);
	_cache = malloc(PSP_SCREEN_SCANLINE * PSP_SCREEN_HEIGHT * PIXEL_BYTES);
	if (_cache == NULL)
		return;
	memcpy(_cache, bg_start, PSP_SCREEN_SCANLINE * PSP_SCREEN_HEIGHT * PIXEL_BYTES);
}

extern void bg_restore(void)
{
	if (!config.have_bg)
		return;
	if (_cache == NULL)
		return;
	memcpy(bg_start, _cache, PSP_SCREEN_SCANLINE * PSP_SCREEN_HEIGHT * PIXEL_BYTES);
	free(_cache);
	_cache = NULL;
}

#endif
