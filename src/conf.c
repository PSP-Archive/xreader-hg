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

#include <unistd.h>
#include <string.h>
#include <pspkernel.h>
#include <pspctrl.h>
#include <psputility_sysparam.h>
#include "common/utils.h"
#include "scene.h"
#include "display.h"
#include "version.h"
#include "conf.h"
#include "iniparser.h"
#include "simple_gettext.h"
#include "dbg.h"
#include "kubridge.h"
#include "passwdmgr.h"
#include "audiocore/buffered_reader.h"
#include "xrPrx.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

static char conf_filename[PATH_MAX];

extern void conf_set_file(const char *filename)
{
	STRCPY_S(conf_filename, filename);
}

extern void conf_get_keyname(u32 key, char *res)
{
	res[0] = 0;
	if ((key & PSP_CTRL_CIRCLE) > 0)
		strcat_s(res, 256, "○");
	if ((key & PSP_CTRL_CROSS) > 0) {
		if (res[0] != 0)
			strcat_s(res, 256, "+×");
		else
			strcat_s(res, 256, "×");
	}
	if ((key & PSP_CTRL_SQUARE) > 0) {
		if (res[0] != 0)
			strcat_s(res, 256, "+□");
		else
			strcat_s(res, 256, "□");
	}
	if ((key & PSP_CTRL_TRIANGLE) > 0) {
		if (res[0] != 0)
			strcat_s(res, 256, "+△");
		else
			strcat_s(res, 256, "△");
	}
	if ((key & PSP_CTRL_LTRIGGER) > 0) {
		if (res[0] != 0)
			strcat_s(res, 256, "+L");
		else
			strcat_s(res, 256, "L");
	}
	if ((key & PSP_CTRL_RTRIGGER) > 0) {
		if (res[0] != 0)
			strcat_s(res, 256, "+R");
		else
			strcat_s(res, 256, "R");
	}
	if ((key & PSP_CTRL_UP) > 0) {
		if (res[0] != 0)
			strcat_s(res, 256, "+↑");
		else
			strcat_s(res, 256, "↑");
	}
	if ((key & PSP_CTRL_DOWN) > 0) {
		if (res[0] != 0)
			strcat_s(res, 256, "+↓");
		else
			strcat_s(res, 256, "↓");
	}
	if ((key & PSP_CTRL_LEFT) > 0) {
		if (res[0] != 0)
			strcat_s(res, 256, "+←");
		else
			strcat_s(res, 256, "←");
	}
	if ((key & PSP_CTRL_RIGHT) > 0) {
		if (res[0] != 0)
			strcat_s(res, 256, "+→");
		else
			strcat_s(res, 256, "→");
	}
	if ((key & PSP_CTRL_SELECT) > 0) {
		if (res[0] != 0)
			strcat_s(res, 256, "+SELECT");
		else
			strcat_s(res, 256, "SELECT");
	}
	if ((key & PSP_CTRL_START) > 0) {
		if (res[0] != 0)
			strcat_s(res, 256, "+START");
		else
			strcat_s(res, 256, "START");
	}
}

static char *get_system_language(void)
{
	int result;

	sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE, &result);

	switch (result) {
		case PSP_SYSTEMPARAM_LANGUAGE_ENGLISH:
			return "en_US";
		case PSP_SYSTEMPARAM_LANGUAGE_CHINESE_TRADITIONAL:
			return "zh_TW";
		case PSP_SYSTEMPARAM_LANGUAGE_CHINESE_SIMPLIFIED:
			return "zh_CN";
	}

	return "zh_CN";
}

static void conf_default(p_conf conf)
{
	int brightness = 100;

	memset(conf, 0, sizeof(t_conf));
	STRCPY_S(conf->path, "ms0:/");
	STRCPY_S(conf->shortpath, "ms0:/");
	STRCPY_S(conf->lastfile, "");
	STRCPY_S(conf->bgarch, "");
	STRCPY_S(conf->bgfile, scene_appdir());
	STRCAT_S(conf->bgfile, "bg.png");
	conf->bgwhere = scene_in_zip;
	conf->confver = XREADER_VERSION_NUM;
	conf->forecolor = 0xFFFFFFFF;
	conf->giftranscolor = 0xFFFFFFFF;
	conf->bgcolor = 0;
	conf->have_bg = true;
	conf->rowspace = 2;
	conf->wordspace = 0;
	conf->borderspace = 0;
	conf->vertread = 0;
	conf->infobar = conf_infobar_info;
	conf->infobar_style = 0;
	conf->rlastrow = false;
	conf->autobm = true;
	conf->encode = conf_encode_gbk;
	conf->fit = conf_fit_none;
	conf->imginfobar = false;
	conf->scrollbar = false;
	conf->scale = 0;
	conf->rotate = conf_rotate_0;
	conf->enable_analog = true;
	conf->img_enable_analog = true;
	conf->txtkey[0] = PSP_CTRL_SQUARE;
	conf->txtkey[1] = PSP_CTRL_LTRIGGER;
	conf->txtkey[2] = PSP_CTRL_RTRIGGER;
	conf->txtkey[3] = PSP_CTRL_UP | PSP_CTRL_CIRCLE;
	conf->txtkey[4] = PSP_CTRL_DOWN | PSP_CTRL_CIRCLE;
	conf->txtkey[5] = PSP_CTRL_LEFT | PSP_CTRL_CIRCLE;
	conf->txtkey[6] = PSP_CTRL_RIGHT | PSP_CTRL_CIRCLE;
	conf->txtkey[7] = PSP_CTRL_LTRIGGER | PSP_CTRL_CIRCLE;
	conf->txtkey[8] = PSP_CTRL_RTRIGGER | PSP_CTRL_CIRCLE;
	conf->txtkey[9] = 0;
	conf->txtkey[10] = 0;
	conf->txtkey[11] = PSP_CTRL_CROSS;
	conf->txtkey[12] = PSP_CTRL_TRIANGLE;
	conf->imgkey[0] = PSP_CTRL_LTRIGGER;
	conf->imgkey[1] = PSP_CTRL_RTRIGGER;
	conf->imgkey[2] = PSP_CTRL_TRIANGLE;
	conf->imgkey[3] = PSP_CTRL_UP | PSP_CTRL_CIRCLE;
	conf->imgkey[4] = PSP_CTRL_DOWN | PSP_CTRL_CIRCLE;
	conf->imgkey[5] = PSP_CTRL_LEFT | PSP_CTRL_CIRCLE;
	conf->imgkey[6] = PSP_CTRL_RIGHT | PSP_CTRL_CIRCLE;
	conf->imgkey[7] = PSP_CTRL_SQUARE;
	conf->imgkey[8] = PSP_CTRL_CIRCLE;
	conf->imgkey[9] = PSP_CTRL_CROSS;
	conf->imgkey[10] = PSP_CTRL_LTRIGGER | PSP_CTRL_CIRCLE;
	conf->imgkey[11] = 0;
	conf->imgkey[12] = PSP_CTRL_UP;
	conf->imgkey[13] = PSP_CTRL_DOWN;
	conf->imgkey[14] = PSP_CTRL_LEFT;
	conf->imgkey[15] = PSP_CTRL_RIGHT;
	conf->flkey[0] = PSP_CTRL_CIRCLE;
	conf->flkey[1] = PSP_CTRL_LTRIGGER;
	conf->flkey[2] = PSP_CTRL_RTRIGGER;
	conf->flkey[3] = PSP_CTRL_CROSS;
	conf->flkey[4] = 0;
	conf->flkey[5] = PSP_CTRL_TRIANGLE;
	conf->flkey[6] = PSP_CTRL_SQUARE;
	conf->flkey[7] = PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER;
	conf->bicubic = false;
	conf->mp3encode = conf_encode_gbk;
	conf->lyricencode = conf_encode_gbk;
	conf->mp3cycle = conf_cycle_repeat;
	conf->isreading = false;
	conf->slideinterval = 5;
	conf->hprmctrl = false;
	conf->grayscale = 30;
	conf->showhidden = true;
	conf->showunknown = true;
	conf->showfinfo = true;
	conf->allowdelete = true;
	conf->arrange = conf_arrange_name;
	conf->enableusb = false;
	conf->viewpos = conf_viewpos_leftup;
	conf->imgmvspd = 8;
	conf->imgpaging = conf_imgpaging_direct;
	conf->imgpaging_spd = 8;
	conf->imgpaging_interval = 10;
	conf->imgpaging_duration = 10;
	conf->fontsize = 12;
	conf->bookfontsize = 12;
	conf->reordertxt = false;
	conf->pagetonext = false;
	conf->autopage = 0;
	conf->autopagetype = 2;
	conf->autolinedelay = 0;
	conf->thumb = conf_thumb_scroll;
	conf->imgpagereserve = 0;
#if defined(ENABLE_MUSIC) && defined(ENABLE_LYRIC)
	conf->lyricex = 1;
#else
	conf->lyricex = 0;
#endif
	conf->autoplay = false;
	conf->usettf = 0;
	conf->freqs[0] = 1;
	conf->freqs[1] = 6;
	conf->freqs[2] = 9;
	conf->imgbrightness = 100;
	conf->dis_scrsave = false;
	conf->autosleep = 0;
	conf->prev_autopage = 2;
	conf->launchtype = 2;

	/*
	   conf->titlecolor = RGB(0x80, 0x10, 0x10);
	   conf->menutextcolor = RGB(0xDF, 0xDF, 0xDF);
	   conf->menubcolor  = RGB(0x40, 0x10, 0x10);
	   conf->selicolor = RGB(0xFF, 0xFF, 0x40);
	   conf->selbcolor = RGB(0x20, 0x20, 0xDF);
	   conf->msgbcolor = RGB(0x18, 0x28, 0x50);
	 */

	conf->titlecolor = RGB(0x30, 0x60, 0x30);
	conf->menutextcolor = RGB(0xDF, 0xDF, 0xDF);
	conf->menubcolor = RGB(0x10, 0x30, 0x20);
	conf->selicolor = RGB(0xFF, 0xFF, 0x40);
	conf->selbcolor = RGB(0x20, 0x20, 0xDF);
	conf->msgbcolor = RGB(0x18, 0x28, 0x50);

	STRCPY_S(conf->cttfpath, scene_appdir());
	STRCAT_S(conf->cttfpath, "fonts/gbk.ttf");
	STRCPY_S(conf->ettfpath, scene_appdir());
	STRCAT_S(conf->ettfpath, "fonts/asc.ttf");

	conf->infobar_use_ttf_mode = true;
	conf->img_no_repeat = false;
	conf->hide_flash = true;
	conf->tabstop = 4;
	conf->apetagorder = true;
	STRCPY_S(conf->language, get_system_language());
	conf->filelistwidth = 160;
	if (psp_model == PSP_MODEL_STANDARD) {
		conf->ttf_load_to_memory = false;
	} else {
		conf->ttf_load_to_memory = true;
	}
	conf->save_password = true;
	conf->scrollbar_width = 5;
	conf->hide_last_row = false;
	conf->infobar_show_timer = true;
	conf->infobar_fontsize = 12;
	conf->englishtruncate = true;
	conf->image_scroll_chgn_speed = true;
	conf->ttf_haste_up = true;
	conf->linenum_style = false;
	conf->infobar_align = conf_align_left;
	conf->show_encoder_msg = false;
	conf->sfx_mode = 0;
	conf->alc_mode = 0;
	conf->use_vaudio = false;
	SPRINTF_S(conf->musicdrv_opts,
			  "mp3_brute_mode=off mp3_buffer_size=%d "
			  "wma_buffer_size=%d aac_buffer_size=%d wav_buffer_size=%d wv_buffer_size=%d "
			  "aa3_buffer_size=%d at3_buffer_size=%d m4a_buffer_size=%d "
			  "flac_buffer_size=%d",
			  BUFFERED_READER_BUFFER_SIZE, BUFFERED_READER_BUFFER_SIZE,
			  BUFFERED_READER_BUFFER_SIZE, BUFFERED_READER_BUFFER_SIZE,
			  BUFFERED_READER_BUFFER_SIZE, BUFFERED_READER_BUFFER_SIZE, BUFFERED_READER_BUFFER_SIZE, BUFFERED_READER_BUFFER_SIZE, BUFFERED_READER_BUFFER_SIZE);
	conf->magnetic_scrolling = true;
	conf->max_cache_img = 10;

	xrDisplayGetBrightness(&brightness, 0);
	conf->max_brightness = brightness;
}

static char *hexToString(char *str, int size, unsigned int hex)
{
	if (str == NULL || size == 0)
		return NULL;

	snprintf_s(str, size, "0x%08x", hex);

	return str;
}

static char *booleanToString(char *str, int size, bool b)
{
	if (str == NULL || size == 0)
		return NULL;

	snprintf_s(str, size, "%s", b ? "true" : "false");

	return str;
}

static char *intToString(char *str, int size, int i)
{
	if (str == NULL || size == 0)
		return NULL;

	snprintf_s(str, size, "%ld", i);

	return str;
}

static char *dwordToString(char *str, int size, u32 dw)
{
	if (str == NULL || size == 0)
		return NULL;

	snprintf_s(str, size, "%lu", dw);

	return str;
}

static char *encodeToString(char *str, int size, t_conf_encode encode)
{
	if (str == NULL || size == 0)
		return NULL;

	switch (encode) {
		case conf_encode_gbk:
			snprintf_s(str, size, "gbk");
			break;
		case conf_encode_big5:
			snprintf_s(str, size, "big5");
			break;
		case conf_encode_sjis:
			snprintf_s(str, size, "sjis");
			break;
		case conf_encode_ucs:
			snprintf_s(str, size, "ucs");
			break;
		case conf_encode_utf8:
			snprintf_s(str, size, "utf8");
			break;
		default:
			snprintf_s(str, size, "");
			break;
	}

	return str;
}

static t_conf_encode stringToEncode(const char *str)
{
	if (stricmp(str, "gbk") == 0) {
		return conf_encode_gbk;
	}
	if (stricmp(str, "big5") == 0) {
		return conf_encode_big5;
	}
	if (stricmp(str, "sjis") == 0) {
		return conf_encode_sjis;
	}
	if (stricmp(str, "ucs") == 0) {
		return conf_encode_ucs;
	}
	if (stricmp(str, "utf8") == 0) {
		return conf_encode_utf8;
	}

	return conf_encode_gbk;
}

static char *fitToString(char *str, int size, t_conf_fit fit)
{
	if (str == NULL || size == 0)
		return NULL;

	switch (fit) {
		case conf_fit_none:
			snprintf_s(str, size, "none");
			break;
		case conf_fit_width:
			snprintf_s(str, size, "width");
			break;
		case conf_fit_dblwidth:
			snprintf_s(str, size, "dblwidth");
			break;
		case conf_fit_height:
			snprintf_s(str, size, "height");
			break;
		case conf_fit_dblheight:
			snprintf_s(str, size, "dblheight");
			break;
		case conf_fit_custom:
			snprintf_s(str, size, "custom");
			break;
		default:
			snprintf_s(str, size, "");
			break;
	}

	return str;
}

static t_conf_fit stringToFit(char *str)
{
	if (stricmp(str, "none") == 0) {
		return conf_fit_none;
	}
	if (stricmp(str, "width") == 0) {
		return conf_fit_width;
	}
	if (stricmp(str, "dblwidth") == 0) {
		return conf_fit_dblwidth;
	}
	if (stricmp(str, "height") == 0) {
		return conf_fit_height;
	}
	if (stricmp(str, "dblheight") == 0) {
		return conf_fit_dblheight;
	}
	if (stricmp(str, "custom") == 0) {
		return conf_fit_custom;
	}

	return conf_fit_none;
}

static char *cycleToString(char *str, int size, t_conf_cycle cycle)
{
	if (str == NULL || size == 0)
		return NULL;

	switch (cycle) {
		case conf_cycle_single:
			snprintf_s(str, size, "single");
			break;
		case conf_cycle_repeat:
			snprintf_s(str, size, "repeat");
			break;
		case conf_cycle_repeat_one:
			snprintf_s(str, size, "repeat_one");
			break;
		case conf_cycle_random:
			snprintf_s(str, size, "random");
			break;
		default:
			snprintf_s(str, size, "");
			break;
	}

	return str;
}

static t_conf_cycle stringToCycle(char *str)
{
	if (stricmp(str, "single") == 0) {
		return conf_cycle_single;
	}
	if (stricmp(str, "repeat") == 0) {
		return conf_cycle_repeat;
	}
	if (stricmp(str, "repeat_one") == 0) {
		return conf_cycle_repeat_one;
	}
	if (stricmp(str, "random") == 0) {
		return conf_cycle_random;
	}

	return conf_cycle_single;
}

static char *arrangeToString(char *str, int size, t_conf_arrange arrange)
{
	if (str == NULL || size == 0)
		return NULL;

	switch (arrange) {
		case conf_arrange_ext:
			snprintf_s(str, size, "ext");
			break;
		case conf_arrange_name:
			snprintf_s(str, size, "name");
			break;
		case conf_arrange_size:
			snprintf_s(str, size, "size");
			break;
		case conf_arrange_ctime:
			snprintf_s(str, size, "ctime");
			break;
		case conf_arrange_mtime:
			snprintf_s(str, size, "mtime");
			break;
		default:
			snprintf_s(str, size, "");
			break;
	}

	return str;
}

static t_conf_arrange stringToArrange(char *str)
{
	if (stricmp(str, "ext") == 0) {
		return conf_arrange_ext;
	}
	if (stricmp(str, "name") == 0) {
		return conf_arrange_name;
	}
	if (stricmp(str, "size") == 0) {
		return conf_arrange_size;
	}
	if (stricmp(str, "ctime") == 0) {
		return conf_arrange_ctime;
	}
	if (stricmp(str, "mtime") == 0) {
		return conf_arrange_mtime;
	}

	return conf_arrange_ext;
}

static char *viewposToString(char *str, int size, t_conf_viewpos viewpos)
{
	if (str == NULL || size == 0)
		return NULL;

	switch (viewpos) {
		case conf_viewpos_leftup:
			snprintf_s(str, size, "leftup");
			break;
		case conf_viewpos_midup:
			snprintf_s(str, size, "midup");
			break;
		case conf_viewpos_rightup:
			snprintf_s(str, size, "rightup");
			break;
		case conf_viewpos_leftmid:
			snprintf_s(str, size, "leftmid");
			break;
		case conf_viewpos_midmid:
			snprintf_s(str, size, "midmid");
			break;
		case conf_viewpos_rightmid:
			snprintf_s(str, size, "rightmid");
			break;
		case conf_viewpos_leftdown:
			snprintf_s(str, size, "leftdown");
			break;
		case conf_viewpos_middown:
			snprintf_s(str, size, "middown");
			break;
		case conf_viewpos_rightdown:
			snprintf_s(str, size, "rightdown");
			break;
		default:
			snprintf_s(str, size, "");
			break;
	}

	return str;
}

static t_conf_viewpos stringToViewpos(char *str)
{
	if (stricmp(str, "leftup") == 0) {
		return conf_viewpos_leftup;
	}
	if (stricmp(str, "midup") == 0) {
		return conf_viewpos_midup;
	}
	if (stricmp(str, "rightup") == 0) {
		return conf_viewpos_rightup;
	}
	if (stricmp(str, "leftmid") == 0) {
		return conf_viewpos_leftmid;
	}
	if (stricmp(str, "midmid") == 0) {
		return conf_viewpos_midmid;
	}
	if (stricmp(str, "leftdown") == 0) {
		return conf_viewpos_leftdown;
	}
	if (stricmp(str, "middown") == 0) {
		return conf_viewpos_middown;
	}
	if (stricmp(str, "rightdown") == 0) {
		return conf_viewpos_rightmid;
	}

	return conf_viewpos_leftup;
}

static char *imgpagingToString(char *str, int size, t_conf_imgpaging imgpaging)
{
	if (str == NULL || size == 0)
		return NULL;

	switch (imgpaging) {
		case conf_imgpaging_direct:
			snprintf_s(str, size, "direct");
			break;
		case conf_imgpaging_updown:
			snprintf_s(str, size, "updown");
			break;
		case conf_imgpaging_leftright:
			snprintf_s(str, size, "leftright");
			break;
		case conf_imgpaging_updown_smooth:
			snprintf_s(str, size, "updown_smooth");
			break;
		case conf_imgpaging_leftright_smooth:
			snprintf_s(str, size, "leftright_smooth");
			break;
		default:
			snprintf_s(str, size, "");
			break;
	}

	return str;
}

static t_conf_imgpaging stringToImgpaging(char *str)
{
	if (stricmp(str, "direct") == 0) {
		return conf_imgpaging_direct;
	}
	if (stricmp(str, "updown") == 0) {
		return conf_imgpaging_updown;
	}
	if (stricmp(str, "leftright") == 0) {
		return conf_imgpaging_leftright;
	}
	if (stricmp(str, "updown_smooth") == 0) {
		return conf_imgpaging_updown_smooth;
	}
	if (stricmp(str, "leftright_smooth") == 0) {
		return conf_imgpaging_leftright_smooth;
	}

	return conf_imgpaging_direct;
}

static char *thumbToString(char *str, int size, t_conf_thumb thumb)
{
	if (str == NULL || size == 0)
		return NULL;

	switch (thumb) {
		case conf_thumb_none:
			snprintf_s(str, size, "none");
			break;
		case conf_thumb_scroll:
			snprintf_s(str, size, "scroll");
			break;
		case conf_thumb_always:
			snprintf_s(str, size, "always");
			break;
		default:
			snprintf_s(str, size, "");
			break;
	}

	return str;
}

static t_conf_thumb stringToThumb(char *str)
{
	if (stricmp(str, "none") == 0) {
		return conf_thumb_none;
	}
	if (stricmp(str, "scroll") == 0) {
		return conf_thumb_scroll;
	}
	if (stricmp(str, "always") == 0) {
		return conf_thumb_always;
	}

	return conf_thumb_none;
}

static char *infobarToString(char *str, int size, t_conf_infobar infobar)
{
	if (str == NULL || size == 0)
		return NULL;

	switch (infobar) {
		case conf_infobar_none:
			snprintf_s(str, size, "none");
			break;
		case conf_infobar_info:
			snprintf_s(str, size, "info");
			break;
		case conf_infobar_lyric:
			snprintf_s(str, size, "lyric");
			break;
		default:
			snprintf_s(str, size, "");
			break;
	}

	return str;
}

static t_conf_infobar stringToInfobar(char *str)
{
	if (stricmp(str, "none") == 0) {
		return conf_infobar_none;
	}
	if (stricmp(str, "info") == 0) {
		return conf_infobar_info;
	}
	if (stricmp(str, "lyric") == 0) {
		return conf_infobar_lyric;
	}

	return conf_infobar_none;
}

static char *vertreadToString(char *str, int size, t_conf_vertread vertread)
{
	if (str == NULL || size == 0)
		return NULL;

	switch (vertread) {
		case conf_vertread_horz:
			snprintf_s(str, size, "horz");
			break;
		case conf_vertread_rvert:
			snprintf_s(str, size, "rvert");
			break;
		case conf_vertread_lvert:
			snprintf_s(str, size, "lvert");
			break;
		case conf_vertread_reversal:
			snprintf_s(str, size, "reversal");
			break;
		default:
			snprintf_s(str, size, "");
			break;
	}

	return str;
}

static t_conf_vertread stringToVertread(char *str)
{
	if (stricmp(str, "horz") == 0) {
		return conf_vertread_horz;
	}
	if (stricmp(str, "rvert") == 0) {
		return conf_vertread_rvert;
	}
	if (stricmp(str, "lvert") == 0) {
		return conf_vertread_lvert;
	}
	if (stricmp(str, "reversal") == 0) {
		return conf_vertread_reversal;
	}

	return conf_vertread_horz;
}

static char *rotateToString(char *str, int size, t_conf_rotate rotate)
{
	if (str == NULL || size == 0)
		return NULL;

	switch (rotate) {
		case conf_rotate_0:
			snprintf_s(str, size, "0");
			break;
		case conf_rotate_90:
			snprintf_s(str, size, "90");
			break;
		case conf_rotate_180:
			snprintf_s(str, size, "180");
			break;
		case conf_rotate_270:
			snprintf_s(str, size, "270");
			break;
		default:
			snprintf_s(str, size, "");
			break;
	}

	return str;
}

static t_conf_rotate stringToRotate(char *str)
{
	if (stricmp(str, "0") == 0) {
		return conf_rotate_0;
	}
	if (stricmp(str, "90") == 0) {
		return conf_rotate_90;
	}
	if (stricmp(str, "180") == 0) {
		return conf_rotate_180;
	}
	if (stricmp(str, "270") == 0) {
		return conf_rotate_270;
	}

	return conf_rotate_0;
}

static char *alignToString(char *str, int size, t_conf_align align)
{
	if (str == NULL || size == 0)
		return NULL;

	switch (align) {
		case conf_align_left:
			snprintf_s(str, size, "left");
			break;
		case conf_align_right:
			snprintf_s(str, size, "right");
			break;
		case conf_align_center:
			snprintf_s(str, size, "center");
			break;
		default:
			snprintf_s(str, size, "");
			break;
	}

	return str;
}

static t_conf_align stringToAlign(char *str)
{
	if (stricmp(str, "left") == 0) {
		return conf_align_left;
	}
	if (stricmp(str, "right") == 0) {
		return conf_align_right;
	}
	if (stricmp(str, "center") == 0) {
		return conf_align_center;
	}
	if (stricmp(str, "middle") == 0) {
		return conf_align_center;
	}

	return conf_align_left;
}

static void check_empty_imgkey(t_conf * conf)
{
	if (conf->confver >= XREADER_VERSION_NUM)
		return;
	if (conf->imgkey[12] == 0)
		conf->imgkey[12] = PSP_CTRL_UP;
	if (conf->imgkey[13] == 0)
		conf->imgkey[13] = PSP_CTRL_DOWN;
	if (conf->imgkey[14] == 0)
		conf->imgkey[14] = PSP_CTRL_LEFT;
	if (conf->imgkey[15] == 0)
		conf->imgkey[15] = PSP_CTRL_RIGHT;
}

extern bool ini_conf_load(const char *inifilename, p_conf conf)
{
	dictionary *dict;
	char buf[80];
	int i;
	extern void get_language(void);

	if (conf == NULL || inifilename == NULL) {
		return false;
	}

	dict = iniparser_load(inifilename);

	if (dict == NULL)
		return false;

	conf_default(conf);

	STRCPY_S(conf->path, iniparser_getstring(dict, "Global:path", conf->path));
	conf->forecolor = iniparser_getunsigned(dict, "UI:forecolor", conf->forecolor);
	conf->giftranscolor = iniparser_getunsigned(dict, "Image:giftranscolor", conf->giftranscolor);
	conf->bgcolor = iniparser_getunsigned(dict, "UI:bgcolor", conf->bgcolor);
	conf->have_bg = iniparser_getboolean(dict, "UI:have_bg", conf->have_bg);
	conf->titlecolor = iniparser_getunsigned(dict, "UI:titlecolor", conf->titlecolor);
	conf->menutextcolor = iniparser_getunsigned(dict, "UI:menutextcolor", conf->menutextcolor);
	conf->menubcolor = iniparser_getunsigned(dict, "UI:menubcolor", conf->menubcolor);
	conf->selicolor = iniparser_getunsigned(dict, "UI:selicolor", conf->selicolor);
	conf->selbcolor = iniparser_getunsigned(dict, "UI:selbcolor", conf->selbcolor);
	conf->msgbcolor = iniparser_getunsigned(dict, "UI:msgbcolor", conf->msgbcolor);
	conf->rowspace = iniparser_getint(dict, "Text:rowspace", conf->rowspace);
	conf->infobar = stringToInfobar(iniparser_getstring(dict, "Text:infobar", infobarToString(buf, sizeof(buf), conf->infobar)));
	conf->infobar_style = iniparser_getint(dict, "Text:infobar_style", conf->infobar_style);
	conf->infobar_fontsize = iniparser_getunsigned(dict, "Text:infobar_fontsize", conf->infobar_fontsize);
	conf->rlastrow = iniparser_getboolean(dict, "Text:rlastrow", conf->rlastrow);
	conf->autobm = iniparser_getboolean(dict, "Text:autobm", conf->autobm);
	conf->vertread = stringToVertread(iniparser_getstring(dict, "Text:vertread", vertreadToString(buf, sizeof(buf), conf->vertread)));
	conf->encode = stringToEncode(iniparser_getstring(dict, "Text:encode", encodeToString(buf, sizeof(buf), conf->encode)));
	conf->fit = stringToFit(iniparser_getstring(dict, "Image:fit", fitToString(buf, sizeof(buf), conf->fit)));
	conf->imginfobar = iniparser_getboolean(dict, "Image:imginfobar", conf->imginfobar);
	conf->scrollbar = iniparser_getboolean(dict, "Text:scrollbar", conf->scrollbar);
	conf->scale = iniparser_getint(dict, "Image:scale", conf->scale);
	conf->rotate = stringToRotate(iniparser_getstring(dict, "Image:rotate", rotateToString(buf, sizeof(buf), conf->rotate)));

	for (i = 0; i < 20; ++i) {
		char key[20];

		SPRINTF_S(key, "Text:txtkey1_%02d", i);
		conf->txtkey[i] = iniparser_getint(dict, key, conf->txtkey[i]);
		SPRINTF_S(key, "Image:imgkey1_%02d", i);
		conf->imgkey[i] = iniparser_getint(dict, key, conf->imgkey[i]);
	}
	check_empty_imgkey(conf);
	STRCPY_S(conf->shortpath, iniparser_getstring(dict, "Global:shortpath", conf->shortpath));
	conf->confver = iniparser_getint(dict, "Global:confver", conf->confver);
	conf->bicubic = iniparser_getboolean(dict, "Image:bicubic", conf->bicubic);
	conf->wordspace = iniparser_getint(dict, "Text:wordspace", conf->wordspace);
	conf->borderspace = iniparser_getunsigned(dict, "Text:borderspace", conf->borderspace);
	STRCPY_S(conf->lastfile, iniparser_getstring(dict, "Global:lastfile", conf->lastfile));
	conf->mp3encode = stringToEncode(iniparser_getstring(dict, "Music:mp3encode", encodeToString(buf, sizeof(buf), conf->mp3encode)));
	conf->lyricencode = stringToEncode(iniparser_getstring(dict, "Music:lyricencode", encodeToString(buf, sizeof(buf), conf->lyricencode)));
	conf->mp3cycle = stringToCycle(iniparser_getstring(dict, "Music:mp3cycle", cycleToString(buf, sizeof(buf), conf->mp3cycle)));
	conf->isreading = iniparser_getboolean(dict, "Global:isreading", conf->isreading);
	STRCPY_S(conf->bgarch, iniparser_getstring(dict, "UI:bgarch", conf->bgarch));
	STRCPY_S(conf->bgfile, iniparser_getstring(dict, "UI:bgfile", conf->bgfile));
	conf->bgwhere = iniparser_getint(dict, "UI:bgwhere", conf->bgwhere);
	conf->slideinterval = iniparser_getint(dict, "Image:slideinterval", conf->slideinterval);
	conf->hprmctrl = iniparser_getboolean(dict, "Music:hprmctrl", conf->hprmctrl);
	conf->grayscale = iniparser_getint(dict, "UI:grayscale", conf->grayscale);
	conf->showhidden = iniparser_getboolean(dict, "Global:showhidden", conf->showhidden);
	conf->showunknown = iniparser_getboolean(dict, "Global:showunknown", conf->showunknown);
	conf->showfinfo = iniparser_getboolean(dict, "Global:showfinfo", conf->showfinfo);
	conf->allowdelete = iniparser_getboolean(dict, "Global:allowdelete", conf->allowdelete);
	conf->arrange = stringToArrange(iniparser_getstring(dict, "Global:arrange", arrangeToString(buf, sizeof(buf), conf->arrange)));
	conf->enableusb = iniparser_getboolean(dict, "Global:enableusb", conf->enableusb);
	conf->viewpos = stringToViewpos(iniparser_getstring(dict, "Image:viewpos", viewposToString(buf, sizeof(buf), conf->viewpos)));
	conf->imgmvspd = iniparser_getint(dict, "Image:imgmvspd", conf->imgmvspd);
	conf->imgpaging = stringToImgpaging(iniparser_getstring(dict, "Image:imgpaging", imgpagingToString(buf, sizeof(buf), conf->imgpaging)));
	conf->imgpaging_spd = iniparser_getint(dict, "Image:imgpaging_spd", conf->imgpaging_spd);
	conf->imgpaging_interval = iniparser_getint(dict, "Image:imgpaging_interval", conf->imgpaging_interval);
	conf->imgpaging_duration = iniparser_getint(dict, "Image:imgpaging_duration", conf->imgpaging_duration);
	for (i = 0; i < 20; ++i) {
		char key[20];

		SPRINTF_S(key, "Global:flkey1_%02d", i);
		conf->flkey[i] = iniparser_getint(dict, key, conf->flkey[i]);
	}
	conf->fontsize = iniparser_getint(dict, "UI:fontsize", conf->fontsize);
	conf->reordertxt = iniparser_getboolean(dict, "Text:reordertxt", conf->reordertxt);
	conf->pagetonext = iniparser_getboolean(dict, "Text:pagetonext", conf->pagetonext);
	conf->autopage = iniparser_getint(dict, "Text:autopage", conf->autopage);
	conf->prev_autopage = iniparser_getint(dict, "Text:prev_autopage", conf->prev_autopage);
	conf->autopagetype = iniparser_getint(dict, "Text:autopagetype", conf->autopagetype);
	conf->autolinedelay = iniparser_getint(dict, "Text:autolinedelay", conf->autolinedelay);
	conf->thumb = stringToThumb(iniparser_getstring(dict, "Image:thumb", thumbToString(buf, sizeof(buf), conf->thumb)));
	conf->bookfontsize = iniparser_getint(dict, "Text:bookfontsize", conf->bookfontsize);
	conf->enable_analog = iniparser_getboolean(dict, "Text:enable_analog", conf->enable_analog);
	conf->img_enable_analog = iniparser_getboolean(dict, "Image:img_enable_analog", conf->img_enable_analog);
	for (i = 0; i < 20; ++i) {
		char key[20];

		SPRINTF_S(key, "Text:txtkey2_%02d", i);
		conf->txtkey2[i] = iniparser_getint(dict, key, conf->txtkey2[i]);
		SPRINTF_S(key, "Image:imgkey2_%02d", i);
		conf->imgkey2[i] = iniparser_getint(dict, key, conf->imgkey2[i]);
		SPRINTF_S(key, "Global:flkey2_%02d", i);
		conf->flkey2[i] = iniparser_getint(dict, key, conf->flkey2[i]);
	}
	conf->imgpagereserve = iniparser_getint(dict, "Image:imgpagereserve", conf->imgpagereserve);
	conf->lyricex = iniparser_getint(dict, "Music:lyricex", conf->lyricex);
	conf->autoplay = iniparser_getboolean(dict, "Music:autoplay", conf->autoplay);
	conf->usettf = iniparser_getboolean(dict, "Text:usettf", conf->usettf);
	STRCPY_S(conf->cttfarch, iniparser_getstring(dict, "Text:cttfarch", conf->cttfarch));
	STRCPY_S(conf->cttfpath, iniparser_getstring(dict, "Text:cttfpath", conf->cttfpath));
	STRCPY_S(conf->ettfarch, iniparser_getstring(dict, "Text:ettfarch", conf->ettfarch));
	STRCPY_S(conf->ettfpath, iniparser_getstring(dict, "Text:ettfpath", conf->ettfpath));
	for (i = 0; i < 3; ++i) {
		char key[20];

		SPRINTF_S(key, "Global:freqs_%d", i);
		conf->freqs[i] = iniparser_getint(dict, key, conf->freqs[i]);
	}
	conf->imgbrightness = iniparser_getint(dict, "Image:imgbrightness", conf->imgbrightness);
	conf->dis_scrsave = iniparser_getboolean(dict, "Global:dis_scrsave", conf->dis_scrsave);
	conf->autosleep = iniparser_getint(dict, "Global:autosleep", conf->autosleep);
	conf->launchtype = iniparser_getint(dict, "Global:launchtype", conf->launchtype);
	conf->infobar_use_ttf_mode = iniparser_getboolean(dict, "Text:infobar_use_ttf_mode", conf->infobar_use_ttf_mode);
	conf->img_no_repeat = iniparser_getboolean(dict, "Image:no_repeat", conf->img_no_repeat);

	conf->hide_flash = iniparser_getboolean(dict, "Global:hide_flash", conf->hide_flash);
	conf->tabstop = iniparser_getunsigned(dict, "Text:tabstop", conf->tabstop);

	conf->apetagorder = iniparser_getboolean(dict, "Music:apetagorder", conf->apetagorder);

	STRCPY_S(conf->language, iniparser_getstring(dict, "UI:language", conf->language));

	get_language();

	conf->filelistwidth = iniparser_getint(dict, "UI:filelistwidth", conf->filelistwidth);

	if (conf->filelistwidth < 0 || conf->filelistwidth > 240)
		conf->filelistwidth = 160;

	conf->ttf_load_to_memory = iniparser_getboolean(dict, "Text:ttf_load_to_memory", conf->ttf_load_to_memory);

	conf->save_password = iniparser_getboolean(dict, "Global:save_password", conf->save_password);

	conf->scrollbar_width = iniparser_getint(dict, "Text:scrollbar_width", conf->scrollbar_width);

	conf->hide_last_row = iniparser_getboolean(dict, "Text:hide_last_row", conf->hide_last_row);

	conf->infobar_show_timer = iniparser_getboolean(dict, "Text:infobar_show_timer", conf->infobar_show_timer);

	conf->englishtruncate = iniparser_getboolean(dict, "Text:englishtruncate", conf->englishtruncate);

	conf->image_scroll_chgn_speed = iniparser_getboolean(dict, "Image:image_scroll_chgn_speed", conf->image_scroll_chgn_speed);

	conf->ttf_haste_up = iniparser_getboolean(dict, "Text:ttf_haste_up", conf->ttf_haste_up);

	conf->linenum_style = iniparser_getboolean(dict, "Text:linenum_style", conf->linenum_style);

	conf->infobar_align = stringToAlign(iniparser_getstring(dict, "Text:infobar_align", ""));

	STRCPY_S(conf->musicdrv_opts, iniparser_getstring(dict, "Music:musicdrv_opts", conf->musicdrv_opts));

	conf->magnetic_scrolling = iniparser_getboolean(dict, "Image:magnetic_scrolling", conf->magnetic_scrolling);

	conf->max_cache_img = iniparser_getunsigned(dict, "Image:max_cache_img", conf->max_cache_img);

	conf->show_encoder_msg = iniparser_getboolean(dict, "Music:show_encoder_msg", conf->show_encoder_msg);

	conf->sfx_mode = iniparser_getint(dict, "Music:sfx_mode", conf->sfx_mode);
	conf->alc_mode = iniparser_getint(dict, "Music:alc_mode", conf->alc_mode);

	conf->use_vaudio = iniparser_getboolean(dict, "Music:use_vaudio", conf->use_vaudio);

	if (conf->max_cache_img == 0) {
		conf->max_cache_img = 10;
	}

	conf->max_brightness = iniparser_getint(dict, "Global:max_brightness", conf->max_brightness);

	dictionary_del(dict);

	return true;
}

extern bool ini_conf_save(p_conf conf)
{
	dictionary *dict;
	FILE *fp;
	char buf[PATH_MAX];
	int i;

	if (conf == NULL) {
		return false;
	}

	conf->confver = XREADER_VERSION_NUM;
	dict = dictionary_new(0);

	if (dict == NULL)
		return false;

	fp = fopen(conf_filename, "w");

	if (fp == NULL) {
		return false;
	}

	if (iniparser_setstring(dict, "Global", NULL) != 0)
		goto error;
	if (iniparser_setstring(dict, "UI", NULL) != 0)
		goto error;
	if (iniparser_setstring(dict, "Text", NULL) != 0)
		goto error;
	if (iniparser_setstring(dict, "Image", NULL) != 0)
		goto error;
	if (iniparser_setstring(dict, "Music", NULL) != 0)
		goto error;

	iniparser_setstring(dict, "Global:path", conf->path);
	iniparser_setstring(dict, "UI:forecolor", hexToString(buf, sizeof(buf), conf->forecolor));
	iniparser_setstring(dict, "Image:giftranscolor", hexToString(buf, sizeof(buf), conf->giftranscolor));
	iniparser_setstring(dict, "UI:bgcolor", hexToString(buf, sizeof(buf), conf->bgcolor));
	iniparser_setstring(dict, "UI:have_bg", booleanToString(buf, sizeof(buf), conf->have_bg));
	iniparser_setstring(dict, "UI:titlecolor", hexToString(buf, sizeof(buf), conf->titlecolor));
	iniparser_setstring(dict, "UI:menutextcolor", hexToString(buf, sizeof(buf), conf->menutextcolor));
	iniparser_setstring(dict, "UI:menubcolor", hexToString(buf, sizeof(buf), conf->menubcolor));
	iniparser_setstring(dict, "UI:selicolor", hexToString(buf, sizeof(buf), conf->selicolor));
	iniparser_setstring(dict, "UI:selbcolor", hexToString(buf, sizeof(buf), conf->selbcolor));
	iniparser_setstring(dict, "UI:msgbcolor", hexToString(buf, sizeof(buf), conf->msgbcolor));
	iniparser_setstring(dict, "Text:rowspace", dwordToString(buf, sizeof(buf), conf->rowspace));
	iniparser_setstring(dict, "Text:infobar", infobarToString(buf, sizeof(buf), conf->infobar));
	iniparser_setstring(dict, "Text:infobar_style", intToString(buf, sizeof(buf), conf->infobar_style));
	iniparser_setstring(dict, "Text:rlastrow", booleanToString(buf, sizeof(buf), conf->rlastrow));
	iniparser_setstring(dict, "Text:autobm", booleanToString(buf, sizeof(buf), conf->autobm));
	iniparser_setstring(dict, "Text:vertread", vertreadToString(buf, sizeof(buf), conf->vertread));
	iniparser_setstring(dict, "Text:encode", encodeToString(buf, sizeof(buf), conf->encode));
	iniparser_setstring(dict, "Image:fit", fitToString(buf, sizeof(buf), conf->fit));
	iniparser_setstring(dict, "Image:imginfobar", booleanToString(buf, sizeof(buf), conf->imginfobar));
	iniparser_setstring(dict, "Text:scrollbar", booleanToString(buf, sizeof(buf), conf->scrollbar));
	iniparser_setstring(dict, "Image:scale", dwordToString(buf, sizeof(buf), conf->scale));
	iniparser_setstring(dict, "Image:rotate", rotateToString(buf, sizeof(buf), conf->rotate));

	for (i = 0; i < 20; ++i) {
		char key[20];

		SPRINTF_S(key, "Text:txtkey1_%02d", i);
		iniparser_setstring(dict, key, hexToString(buf, sizeof(buf), conf->txtkey[i]));
		SPRINTF_S(key, "Image:imgkey1_%02d", i);
		iniparser_setstring(dict, key, hexToString(buf, sizeof(buf), conf->imgkey[i]));
	}
	iniparser_setstring(dict, "Global:shortpath", conf->shortpath);
	iniparser_setstring(dict, "Global:confver", hexToString(buf, sizeof(buf), conf->confver));
	iniparser_setstring(dict, "Image:bicubic", booleanToString(buf, sizeof(buf), conf->bicubic));
	iniparser_setstring(dict, "Text:wordspace", dwordToString(buf, sizeof(buf), conf->wordspace));
	iniparser_setstring(dict, "Text:borderspace", dwordToString(buf, sizeof(buf), conf->borderspace));
	iniparser_setstring(dict, "Global:lastfile", conf->lastfile);
	iniparser_setstring(dict, "Music:mp3encode", encodeToString(buf, sizeof(buf), conf->mp3encode));
	iniparser_setstring(dict, "Music:lyricencode", encodeToString(buf, sizeof(buf), conf->lyricencode));
	iniparser_setstring(dict, "Music:mp3cycle", cycleToString(buf, sizeof(buf), conf->mp3cycle));
	iniparser_setstring(dict, "Global:isreading", booleanToString(buf, sizeof(buf), conf->isreading));
	iniparser_setstring(dict, "UI:bgarch", conf->bgarch);
	iniparser_setstring(dict, "UI:bgfile", conf->bgfile);
	iniparser_setstring(dict, "UI:bgwhere", intToString(buf, sizeof(buf), conf->bgwhere));
	iniparser_setstring(dict, "Image:slideinterval", dwordToString(buf, sizeof(buf), conf->slideinterval));
	iniparser_setstring(dict, "Music:hprmctrl", booleanToString(buf, sizeof(buf), conf->hprmctrl));
	iniparser_setstring(dict, "UI:grayscale", intToString(buf, sizeof(buf), conf->grayscale));
	iniparser_setstring(dict, "Global:showhidden", booleanToString(buf, sizeof(buf), conf->showhidden));
	iniparser_setstring(dict, "Global:showunknown", booleanToString(buf, sizeof(buf), conf->showunknown));
	iniparser_setstring(dict, "Global:showfinfo", booleanToString(buf, sizeof(buf), conf->showfinfo));
	iniparser_setstring(dict, "Global:allowdelete", booleanToString(buf, sizeof(buf), conf->allowdelete));
	iniparser_setstring(dict, "Global:arrange", arrangeToString(buf, sizeof(buf), conf->arrange));
	iniparser_setstring(dict, "Global:enableusb", booleanToString(buf, sizeof(buf), conf->enableusb));
	iniparser_setstring(dict, "Image:viewpos", viewposToString(buf, sizeof(buf), conf->viewpos));
	iniparser_setstring(dict, "Image:imgmvspd", dwordToString(buf, sizeof(buf), conf->imgmvspd));
	iniparser_setstring(dict, "Image:imgpaging", imgpagingToString(buf, sizeof(buf), conf->imgpaging));
	iniparser_setstring(dict, "Image:imgpaging_spd", dwordToString(buf, sizeof(buf), conf->imgpaging_spd));
	iniparser_setstring(dict, "Image:imgpaging_interval", dwordToString(buf, sizeof(buf), conf->imgpaging_interval));
	iniparser_setstring(dict, "Image:imgpaging_duration", dwordToString(buf, sizeof(buf), conf->imgpaging_duration));
	for (i = 0; i < 20; ++i) {
		char key[20];

		SPRINTF_S(key, "Global:flkey1_%02d", i);
		iniparser_setstring(dict, key, hexToString(buf, sizeof(buf), conf->flkey[i]));
	}
	iniparser_setstring(dict, "UI:fontsize", intToString(buf, sizeof(buf), conf->fontsize));
	iniparser_setstring(dict, "Text:reordertxt", booleanToString(buf, sizeof(buf), conf->reordertxt));
	iniparser_setstring(dict, "Text:pagetonext", booleanToString(buf, sizeof(buf), conf->pagetonext));
	iniparser_setstring(dict, "Text:autopage", intToString(buf, sizeof(buf), conf->autopage));
	iniparser_setstring(dict, "Text:prev_autopage", intToString(buf, sizeof(buf), conf->prev_autopage));
	iniparser_setstring(dict, "Text:autopagetype", intToString(buf, sizeof(buf), conf->autopagetype));
	iniparser_setstring(dict, "Text:autolinedelay", intToString(buf, sizeof(buf), conf->autolinedelay));
	iniparser_setstring(dict, "Image:thumb", thumbToString(buf, sizeof(buf), conf->thumb));
	iniparser_setstring(dict, "Text:bookfontsize", intToString(buf, sizeof(buf), conf->bookfontsize));
	iniparser_setstring(dict, "Text:enable_analog", booleanToString(buf, sizeof(buf), conf->enable_analog));
	iniparser_setstring(dict, "Image:img_enable_analog", booleanToString(buf, sizeof(buf), conf->img_enable_analog));
	for (i = 0; i < 20; ++i) {
		char key[20];

		SPRINTF_S(key, "Text:txtkey2_%02d", i);
		iniparser_setstring(dict, key, hexToString(buf, sizeof(buf), conf->txtkey2[i]));
		SPRINTF_S(key, "Image:imgkey2_%02d", i);
		iniparser_setstring(dict, key, hexToString(buf, sizeof(buf), conf->imgkey2[i]));
		SPRINTF_S(key, "Global:flkey2_%02d", i);
		iniparser_setstring(dict, key, hexToString(buf, sizeof(buf), conf->flkey2[i]));
	}
	iniparser_setstring(dict, "Image:imgpagereserve", dwordToString(buf, sizeof(buf), conf->imgpagereserve));
	iniparser_setstring(dict, "Music:lyricex", dwordToString(buf, sizeof(buf), conf->lyricex));
	iniparser_setstring(dict, "Music:autoplay", booleanToString(buf, sizeof(buf), conf->autoplay));
	iniparser_setstring(dict, "Text:usettf", booleanToString(buf, sizeof(buf), conf->usettf));
	iniparser_setstring(dict, "Text:cttfarch", conf->cttfarch);
	iniparser_setstring(dict, "Text:cttfpath", conf->cttfpath);
	iniparser_setstring(dict, "Text:ettfarch", conf->ettfarch);
	iniparser_setstring(dict, "Text:ettfpath", conf->ettfpath);
	for (i = 0; i < 3; ++i) {
		char key[20];

		SPRINTF_S(key, "Global:freqs_%d", i);
		iniparser_setstring(dict, key, intToString(buf, sizeof(buf), conf->freqs[i]));
	}
	iniparser_setstring(dict, "Image:imgbrightness", intToString(buf, sizeof(buf), conf->imgbrightness));
	iniparser_setstring(dict, "Global:dis_scrsave", booleanToString(buf, sizeof(buf), conf->dis_scrsave));
	iniparser_setstring(dict, "Global:autosleep", intToString(buf, sizeof(buf), conf->autosleep));
	iniparser_setstring(dict, "Global:launchtype", intToString(buf, sizeof(buf), conf->launchtype));
	iniparser_setstring(dict, "Text:infobar_use_ttf_mode", booleanToString(buf, sizeof(buf), conf->infobar_use_ttf_mode));
	iniparser_setstring(dict, "Text:infobar_fontsize", dwordToString(buf, sizeof(buf), conf->infobar_fontsize));
	iniparser_setstring(dict, "Image:no_repeat", booleanToString(buf, sizeof(buf), conf->img_no_repeat));

	iniparser_setstring(dict, "Global:hide_flash", booleanToString(buf, sizeof(buf), conf->hide_flash));

	iniparser_setstring(dict, "Text:tabstop", dwordToString(buf, sizeof(buf), conf->tabstop));

	iniparser_setstring(dict, "Music:apetagorder", booleanToString(buf, sizeof(buf), conf->apetagorder));

	iniparser_setstring(dict, "UI:language", conf->language);

	iniparser_setstring(dict, "UI:filelistwidth", intToString(buf, sizeof(buf), conf->filelistwidth));

	iniparser_setstring(dict, "Text:ttf_load_to_memory", booleanToString(buf, sizeof(buf), conf->ttf_load_to_memory));

	iniparser_setstring(dict, "Global:save_password", booleanToString(buf, sizeof(buf), conf->save_password));

	iniparser_setstring(dict, "Text:scrollbar_width", intToString(buf, sizeof(buf), conf->scrollbar_width));

	iniparser_setstring(dict, "Text:hide_last_row", booleanToString(buf, sizeof(buf), conf->hide_last_row));

	iniparser_setstring(dict, "Text:infobar_show_timer", booleanToString(buf, sizeof(buf), conf->infobar_show_timer));

	iniparser_setstring(dict, "Text:englishtruncate", booleanToString(buf, sizeof(buf), conf->englishtruncate));

	iniparser_setstring(dict, "Image:image_scroll_chgn_speed", booleanToString(buf, sizeof(buf), conf->image_scroll_chgn_speed));

	iniparser_setstring(dict, "Text:ttf_haste_up", booleanToString(buf, sizeof(buf), conf->ttf_haste_up));

	iniparser_setstring(dict, "Text:linenum_style", booleanToString(buf, sizeof(buf), conf->linenum_style));

	iniparser_setstring(dict, "Text:infobar_align", alignToString(buf, sizeof(buf), conf->infobar_align));

	iniparser_setstring(dict, "Music:musicdrv_opts", conf->musicdrv_opts);

	iniparser_setstring(dict, "Image:magnetic_scrolling", booleanToString(buf, sizeof(buf), conf->magnetic_scrolling));

	iniparser_setstring(dict, "Image:max_cache_img", dwordToString(buf, sizeof(buf), conf->max_cache_img));

	iniparser_setstring(dict, "Music:show_encoder_msg", booleanToString(buf, sizeof(buf), conf->show_encoder_msg));

	iniparser_setstring(dict, "Music:sfx_mode", intToString(buf, sizeof(buf), conf->sfx_mode));

	iniparser_setstring(dict, "Music:alc_mode", intToString(buf, sizeof(buf), conf->alc_mode));

	iniparser_setstring(dict, "Music:use_vaudio", booleanToString(buf, sizeof(buf), conf->use_vaudio));

	iniparser_setstring(dict, "Global:max_brightness", intToString(buf, sizeof(buf), conf->max_brightness));

	iniparser_dump_ini(dict, fp);

	fclose(fp);

	dictionary_del(dict);

	return true;
  error:
	if (fp != NULL)
		fclose(fp);
	return false;
}

extern bool conf_load(p_conf conf)
{
	conf_default(conf);
	if (ini_conf_load(conf_filename, conf) == false)
		return false;

	/*
	   if (conf->confver != XREADER_VERSION_NUM)
	   conf_default(conf);
	 */
#ifndef ENABLE_USB
	conf->enableusb = false;
#endif
#ifndef ENABLE_BG
	STRCPY_S(conf->bgfile, "");
	STRCPY_S(conf->bgarch, "");
	conf->bgwhere = scene_in_dir;
#endif
#ifndef ENABLE_MUSIC
	conf->autoplay = false;
	conf->hprmctrl = true;
#else
#ifndef ENABLE_LYRIC
	conf->lyricex = 0;
#endif
#endif

	ini_conf_save(conf);

	return true;
}

extern bool conf_save(p_conf conf)
{
	ini_conf_save(conf);
	return true;
}

extern const char *conf_get_encodename(t_conf_encode encode)
{
	switch (encode) {
		case conf_encode_gbk:
			return "GBK";
		case conf_encode_big5:
			return "BIG5";
		case conf_encode_sjis:
			return "SJIS";
		case conf_encode_ucs:
			return "UCS";
		case conf_encode_utf8:
			return "UTF-8";
		default:
			return "";
	}
}

extern const char *conf_get_fitname(t_conf_fit fit)
{
	switch (fit) {
		case conf_fit_none:
			return _("原始大小");
			break;
		case conf_fit_width:
			return _("适应宽度");
			break;
		case conf_fit_dblwidth:
			return _("适应两倍宽度");
			break;
		case conf_fit_height:
			return _("适应高度");
			break;
		case conf_fit_dblheight:
			return _("适应两倍高度");
			break;
		case conf_fit_custom:
			return "";
			break;
		default:
			return "";
			break;
	}
}

extern const char *conf_get_cyclename(t_conf_cycle cycle)
{
	switch (cycle) {
		case conf_cycle_single:
			return _("顺序播放");
		case conf_cycle_repeat:
			return _("循环播放");
		case conf_cycle_repeat_one:
			return _("循环单曲");
		case conf_cycle_random:
			return _("随机播放");
		default:
			return "";
	}
}

extern const char *conf_get_arrangename(t_conf_arrange arrange)
{
	switch (arrange) {
		case conf_arrange_ext:
			return _("按扩展名");
			break;
		case conf_arrange_name:
			return _("按文件名");
			break;
		case conf_arrange_size:
			return _("按文件大小");
			break;
		case conf_arrange_ctime:
			return _("按创建时间");
			break;
		case conf_arrange_mtime:
			return _("按修改时间");
			break;
		default:
			return "";
	}
}

extern const char *conf_get_viewposname(t_conf_viewpos viewpos)
{
	switch (viewpos) {
		case conf_viewpos_leftup:
			return _("左上");
		case conf_viewpos_midup:
			return _("中上");
		case conf_viewpos_rightup:
			return _("右上");
		case conf_viewpos_leftmid:
			return _("左中");
		case conf_viewpos_midmid:
			return _("正中");
		case conf_viewpos_rightmid:
			return _("右中");
		case conf_viewpos_leftdown:
			return _("左下");
		case conf_viewpos_middown:
			return _("中下");
		case conf_viewpos_rightdown:
			return _("右下");
		default:
			return "";
	}
}

extern const char *conf_get_imgpagingname(t_conf_imgpaging imgpaging)
{
	switch (imgpaging) {
		case conf_imgpaging_direct:
			return _("直接翻页");
		case conf_imgpaging_updown:
			return _("先上下卷动");
		case conf_imgpaging_leftright:
			return _("先左右卷动");
		case conf_imgpaging_updown_smooth:
			return _("先上下滚动");
		case conf_imgpaging_leftright_smooth:
			return _("先左右滚动");
		default:
			return "";
	}
}

extern const char *conf_get_thumbname(t_conf_thumb thumb)
{
	switch (thumb) {
		case conf_thumb_none:
			return _("不显示");
		case conf_thumb_scroll:
			return _("卷动时显示");
		case conf_thumb_always:
			return _("总是显示");
		default:
			return "";
	}
}

extern const char *conf_get_infobarname(t_conf_infobar infobar)
{
	switch (infobar) {
		case conf_infobar_none:
			return _("不显示");
		case conf_infobar_info:
			return _("阅读信息");
#ifdef ENABLE_LYRIC
		case conf_infobar_lyric:
			return _("歌词");
#endif
		default:
			return "";
	}
}
