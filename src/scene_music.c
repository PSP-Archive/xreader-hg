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
#include <pspkernel.h>
#include <psppower.h>
#include <psprtc.h>
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
#ifdef ENABLE_MUSIC
#include "audiocore/musicmgr.h"
#include "audiocore/musicdrv.h"
#ifdef ENABLE_LYRIC
#include "audiocore/lyric.h"
#endif
#endif
#include "text.h"
#include "bg.h"
#include "copy.h"
#include "common/qsort.h"
#include "common/utils.h"
#include "scene_impl.h"
#include "simple_gettext.h"
#include "charsets.h"
#include "dbg.h"
#include "image_queue.h"
#include "audiocore/xaudiolib.h"
#include "audiocore/musiclist.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

extern win_menu_predraw_data g_predraw;

#ifdef ENABLE_MUSIC
static int g_ins_kbps;
#endif

static volatile int secticks = 0;
static u32 enable_exit_menu = 0;

static u64 start, end;

t_win_menu_op scene_mp3_list_menucb(u32 key, p_win_menuitem item, u32 * count, u32 max_height, u32 * topindex, u32 * index)
{
	u32 orgidx;
	t_win_menu_op op;

	switch (key) {
		case (PSP_CTRL_SELECT | PSP_CTRL_START):
			if (win_msgbox("是否退出软件?", "是", "否", COLOR_WHITE, COLOR_WHITE, config.msgbcolor)) {
				scene_exit();
				return win_menu_op_continue;
			}
			return win_menu_op_force_redraw;
		case PSP_CTRL_SQUARE:
#ifdef ENABLE_MUSIC
			if (*index < music_maxindex() - 1) {
				t_win_menuitem sitem;
				u32 mp3i = *index;

				music_movedown(mp3i);
				memcpy(&sitem, &item[mp3i], sizeof(t_win_menuitem));
				memcpy(&item[mp3i], &item[mp3i + 1], sizeof(t_win_menuitem));
				memcpy(&item[mp3i + 1], &sitem, sizeof(t_win_menuitem));
				(*index)++;
				return win_menu_op_force_redraw;
			}
#endif
			return win_menu_op_continue;
		case PSP_CTRL_TRIANGLE:
#ifdef ENABLE_MUSIC
			if (*index > 0) {
				t_win_menuitem sitem;
				u32 mp3i = *index;

				music_moveup(mp3i);
				memcpy(&sitem, &item[mp3i], sizeof(t_win_menuitem));
				memcpy(&item[mp3i], &item[mp3i - 1], sizeof(t_win_menuitem));
				memcpy(&item[mp3i - 1], &sitem, sizeof(t_win_menuitem));
				(*index)--;
				return win_menu_op_force_redraw;
			}
#endif
			return win_menu_op_continue;
		case PSP_CTRL_CIRCLE:
#ifdef ENABLE_MUSIC
			if (win_msgbox(_("从歌曲列表移除文件?"), _("是"), _("否"), COLOR_WHITE, COLOR_WHITE, config.msgbcolor)) {
				music_del(*index);
				if (*index < *count - 1)
					memmove(&item[*index], &item[*index + 1], (*count - *index - 1) * sizeof(t_win_menuitem));
				if (*index == *count - 1)
					(*index)--;
				(*count)--;
			}
#endif
			return win_menu_op_ok;
		case PSP_CTRL_START:
#ifdef ENABLE_MUSIC
			{
				MusicListEntry *p;

				if ((p = musiclist_get(&g_music_list, *index)) != NULL) {
					music_directplay(p->spath, p->lpath);
				}

				return win_menu_op_continue;
			}
#endif
			return win_menu_op_ok;
		case PSP_CTRL_SELECT:
			ctrl_waitrelease();
			return win_menu_op_cancel;
	}

	orgidx = *index;
	op = win_menu_defcb(key, item, count, max_height, topindex, index);
	if (*index != orgidx && ((orgidx - *topindex < 6 && *index - *topindex >= 6)
							 || (orgidx - *topindex >= 6 && *index - *topindex < 6)))
		return win_menu_op_force_redraw;
	return op;
}

void scene_mp3_list_predraw(p_win_menuitem item, u32 index, u32 topindex, u32 max_height)
{
	int left, right, upper, bottom;
	int pos, posend;

	default_predraw(&g_predraw, _(" ○删除 □下移 △上移 ×退出 START播放"), max_height, &left, &right, &upper, &bottom, 4);
	pos = get_center_pos(0, 480, _("要添加乐曲请到文件列表选取音乐文件按○"));
	posend = pos + 1 + strlen(_("要添加乐曲请到文件列表选取音乐文件按○")) * DISP_FONTSIZE / 2;
	upper = bottom + 1;
	bottom = bottom + 1 + 1 * DISP_FONTSIZE;

	disp_rectangle(pos - 2, upper - 1, posend + 1, bottom + 1, COLOR_WHITE);
	disp_fillrect(pos - 1, upper, posend, bottom, RGB(0x20, 0x40, 0x30));
	disp_putstring(pos, upper, COLOR_WHITE, (const u8 *)
				   _("要添加乐曲请到文件列表选取音乐文件按○"));
}

void scene_mp3_list_postdraw(p_win_menuitem item, u32 index, u32 topindex, u32 max_height)
{
	char outstr[256];
	const char *fname;

#ifdef ENABLE_MUSIC
	MusicListEntry *fl = musiclist_get(&g_music_list, index);

	if (fl == NULL)
		return;
	fname = fl->lpath;
#else
	fname = _("音乐已关闭");
#endif

	if (strlen(fname) <= 36)
		STRCPY_S(outstr, fname);
	else {
		mbcsncpy_s((unsigned char *) outstr, 34, (const unsigned char *) fname, -1);
		if (strlen(outstr) < 33) {
			mbcsncpy_s(((unsigned char *) outstr), 36, ((const unsigned char *) fname), -1);
			STRCAT_S(outstr, "..");
		} else
			STRCAT_S(outstr, "...");
	}
	if (index - topindex < 6) {
		disp_rectangle(239 - 9 * DISP_FONTSIZE, 140 + 5 * DISP_FONTSIZE, 243 + 9 * DISP_FONTSIZE, 141 + 6 * DISP_FONTSIZE, COLOR_WHITE);
		disp_fillrect(240 - 9 * DISP_FONTSIZE, 141 + 5 * DISP_FONTSIZE, 242 + 9 * DISP_FONTSIZE, 140 + 6 * DISP_FONTSIZE, RGB(0x40, 0x40, 0x40));
		disp_putstring(240 - 9 * DISP_FONTSIZE, 141 + 5 * DISP_FONTSIZE, COLOR_WHITE, (const u8 *) outstr);
	} else {
		disp_rectangle(239 - 9 * DISP_FONTSIZE, 133 - 6 * DISP_FONTSIZE, 243 + 9 * DISP_FONTSIZE, 134 - 5 * DISP_FONTSIZE, COLOR_WHITE);
		disp_fillrect(240 - 9 * DISP_FONTSIZE, 134 - 6 * DISP_FONTSIZE, 242 + 9 * DISP_FONTSIZE, 133 - 5 * DISP_FONTSIZE, RGB(0x40, 0x40, 0x40));
		disp_putstring(240 - 9 * DISP_FONTSIZE, 134 - 6 * DISP_FONTSIZE, COLOR_WHITE, (const u8 *) outstr);
	}
}

void scene_mp3_list(void)
{
#ifdef ENABLE_MUSIC
	p_win_menuitem item;

	u32 i;
	u32 index = 0;

	win_menu_predraw_data prev;

	item = malloc(music_maxindex() * sizeof(*item));

	if (item == NULL)
		return;

	memcpy(&prev, &g_predraw, sizeof(prev));

#ifdef ENABLE_NLS
	if (strcmp(simple_textdomain(NULL), "zh_CN") == 0)
		g_predraw.max_item_len = WRR * 4;
	else
		g_predraw.max_item_len = WRR * 5;
#else
	g_predraw.max_item_len = WRR * 4;
#endif

	for (i = 0; i < music_maxindex(); i++) {
		MusicListEntry *fl = musiclist_get(&g_music_list, i);
		char *rname;

		if (fl == NULL)
			continue;

		rname = strrchr(fl->lpath, '/');

		if (rname == NULL)
			rname = fl->lpath;
		else
			rname++;

		if (strlen(rname) <= g_predraw.max_item_len - 2)
			STRCPY_S(item[i].name, rname);
		else {
			mbcsncpy_s((unsigned char *) item[i].name, g_predraw.max_item_len - 3, (const unsigned char *) rname, -1);
			STRCAT_S(item[i].name, "...");
		}
		item[i].width = strlen(item[i].name);
		item[i].selected = false;
		item[i].icolor = config.menutextcolor;
		item[i].selicolor = config.selicolor;
		item[i].selrcolor = RGB(0x40, 0x40, 0x28);
		item[i].selbcolor = config.selbcolor;
	}

	g_predraw.item_count = 12;
	g_predraw.x = 240;
	g_predraw.y = 130;
	g_predraw.left = g_predraw.x - DISP_FONTSIZE * g_predraw.max_item_len / 2 / 2;
	g_predraw.upper = g_predraw.y - DISP_FONTSIZE * g_predraw.item_count / 2;
	g_predraw.linespace = 0;

	while ((index =
			win_menu(g_predraw.left, g_predraw.upper,
					 g_predraw.max_item_len, g_predraw.item_count, item,
					 music_maxindex(), index, 0, RGB(0x40, 0x40, 0x28),
					 true, scene_mp3_list_predraw, scene_mp3_list_postdraw, scene_mp3_list_menucb)) != INVALID)
		if (music_maxindex() == 0)
			break;
	memcpy(&g_predraw, &prev, sizeof(g_predraw));
	ctrl_waitrelease();
	free(item);
#endif
}

const char *get_week_str(int day)
{
	static char *week_str[] = {
		"星期天",
		"星期一",
		"星期二",
		"星期三",
		"星期四",
		"星期五",
		"星期六",
		"日期错误"
	};

	if (day < 0 || day > 7) {
		day = 7;
	}

	return _(week_str[day]);
}

static void scene_mp3bar_delay_action(void)
{
	sceKernelDelayThread(50000);

	if (config.dis_scrsave)
		scePowerTick(0);
}

#if defined(ENABLE_MUSIC) && defined(ENABLE_LYRIC)
static void draw_lyric_string(const char **ly, u32 lidx, u32 * ss, u32 cpl)
{
	char t[BUFSIZ];

	if (ly[lidx] == NULL)
		return;
	if (ss[lidx] > cpl)
		ss[lidx] = cpl;

	lyric_decode(ly[lidx], t, &ss[lidx]);
	disp_putnstring(6 + (cpl - ss[lidx]) * DISP_FONTSIZE / 4,
					136 - (DISP_FONTSIZE + 1) * (1 + config.lyricex) +
					(DISP_FONTSIZE + 1) * lidx + 1,
					(lidx == config.lyricex) ? COLOR_WHITE : RGB(0x7F, 0x7F, 0x7F), (const u8 *) t, ss[lidx], 0, 0, DISP_FONTSIZE, 0);
}
#endif

static void scene_draw_lyric(void)
{
#if defined(ENABLE_MUSIC) && defined(ENABLE_LYRIC)
	const char *ly[config.lyricex * 2 + 1];
	u32 ss[config.lyricex * 2 + 1];

	disp_rectangle(5, 136 - (DISP_FONTSIZE + 1) * (1 + config.lyricex), 474, 136 + (DISP_FONTSIZE + 1) * config.lyricex, COLOR_WHITE);
	disp_fillrect(6, 136 - (DISP_FONTSIZE + 1) * (1 + config.lyricex) + 1, 473, 136 + (DISP_FONTSIZE + 1) * config.lyricex - 1, config.msgbcolor);
	if (lyric_get_cur_lines(music_get_lyric(), config.lyricex, ly, ss)) {
		int lidx;
		u32 cpl = 936 / DISP_FONTSIZE;

		for (lidx = 0; lidx < config.lyricex * 2 + 1; lidx++) {
			draw_lyric_string(ly, lidx, ss, cpl);
		}
	}
#endif
}

#ifdef ENABLE_MUSIC

static void scene_draw_mp3bar_music_staff(void)
{
	int bitrate, sample, len, tlen;
	char infostr[80];
	struct music_info info = { 0 };

	disp_rectangle(5, 262 - DISP_FONTSIZE * 4, 474, 267, COLOR_WHITE);
	disp_fillrect(6, 263 - DISP_FONTSIZE * 4, 473, 266, config.msgbcolor);
	info.type = MD_GET_AVGKBPS | MD_GET_FREQ | MD_GET_CURTIME | MD_GET_DURATION | MD_GET_DECODERNAME;
	if (musicdrv_get_info(&info) == 0) {
		bitrate = info.avg_kbps;
		sample = info.freq;
		len = (int) info.cur_time;
		tlen = (int) info.duration;

		if (g_ins_kbps == 0)
			SPRINTF_S(infostr,
					  "%s   %4d kbps   %d Hz   %02d:%02d / %02d:%02d",
					  conf_get_cyclename(config.mp3cycle), bitrate, sample, len / 60, len % 60, tlen / 60, tlen % 60);
		else
			SPRINTF_S(infostr,
					  "%s   %4d kbps   %d Hz   %02d:%02d / %02d:%02d",
					  conf_get_cyclename(config.mp3cycle), g_ins_kbps, sample, len / 60, len % 60, tlen / 60, tlen % 60);

		if (config.fontsize <= 12) {
			char t[128];

			SPRINTF_S(t, "   [%s@%dkbps]", info.decoder_name, bitrate);
			STRCAT_S(infostr, t);
		}
	} else
		SPRINTF_S(infostr, "%s", conf_get_cyclename(config.mp3cycle));
	disp_putstring(6 + DISP_FONTSIZE, 265 - DISP_FONTSIZE * 2, COLOR_WHITE, (const u8 *) infostr);
	disp_putstring(6 + DISP_FONTSIZE, 263 - DISP_FONTSIZE * 4, COLOR_WHITE, (const u8 *)
				   _("  SELECT 编辑列表   ←快速后退   →快速前进"));
	SPRINTF_S(infostr, "%s  LRC  %s  ID3v1 %s",
			  _("  SELECT 编辑列表   ←快速后退   →快速前进"), conf_get_encodename(config.lyricencode), conf_get_encodename(config.mp3encode));
	disp_putnstring(6 + DISP_FONTSIZE, 263 - DISP_FONTSIZE * 4, COLOR_WHITE,
					(const u8 *) infostr, (468 - DISP_FONTSIZE * 2) * 2 / DISP_FONTSIZE, 0, 0, DISP_FONTSIZE, 0);

	if (config.use_vaudio) {
		SPRINTF_S(infostr, "%s [%s] [%s]",
				  _("○播放/暂停 ×循环 □停止 △曲名编码  L上一首  R下一首"), get_sfx_mode_str(config.sfx_mode), (config.alc_mode ? "ALC" : "OFF"));
	} else {
		STRCPY_S(infostr, _("○播放/暂停 ×循环 □停止 △曲名编码  L上一首  R下一首"));
	}

	disp_putstring(6 + DISP_FONTSIZE, 264 - DISP_FONTSIZE * 3, COLOR_WHITE, (const u8 *) infostr);
	info.type = MD_GET_TITLE | MD_GET_ARTIST | MD_GET_ALBUM;
	if (musicdrv_get_info(&info) == 0) {
		char tag[512];

		switch (info.encode) {
			case conf_encode_utf8:
				charsets_utf8_conv((const u8 *) info.artist, sizeof(info.artist), (u8 *) info.artist, sizeof(info.artist));
				charsets_utf8_conv((const u8 *) info.title, sizeof(info.title), (u8 *) info.title, sizeof(info.title));
				charsets_utf8_conv((const u8 *) info.album, sizeof(info.album), (u8 *) info.album, sizeof(info.album));
				charsets_utf8_conv((const u8 *) info.comment, sizeof(info.comment), (u8 *) info.comment, sizeof(info.comment));
				break;
			case conf_encode_big5:
				charsets_big5_conv((const u8 *) info.artist, sizeof(info.artist), (u8 *) info.artist, sizeof(info.artist));
				charsets_big5_conv((const u8 *) info.title, sizeof(info.title), (u8 *) info.title, sizeof(info.title));
				charsets_big5_conv((const u8 *) info.album, sizeof(info.album), (u8 *) info.album, sizeof(info.album));
				charsets_big5_conv((const u8 *) info.comment, sizeof(info.comment), (u8 *) info.comment, sizeof(info.comment));
				break;
			case conf_encode_sjis:
				{
					u8 *temp;
					u32 size;

					temp = NULL, size = strlen(info.artist);
					charsets_sjis_conv((const u8 *) info.artist, (u8 **) & temp, (u32 *) & size);
					strncpy_s(info.artist, sizeof(info.artist), (const char *) temp, size);
					free(temp);
					temp = NULL, size = strlen(info.title);
					charsets_sjis_conv((const u8 *) info.title, (u8 **) & temp, (u32 *) & size);
					strncpy_s(info.title, sizeof(info.title), (const char *) temp, size);
					free(temp);
					temp = NULL, size = strlen(info.album);
					charsets_sjis_conv((const u8 *) info.album, (u8 **) & temp, (u32 *) & size);
					strncpy_s(info.album, sizeof(info.album), (const char *) temp, size);
					free(temp);
					temp = NULL, size = strlen(info.comment);
					charsets_sjis_conv((const u8 *) info.comment, (u8 **) & temp, (u32 *) & size);
					strncpy_s(info.comment, sizeof(info.comment), (const char *) temp, size);
					free(temp);
				}
				break;
			case conf_encode_gbk:
				break;
			case conf_encode_ucs:
				{
					charsets_ucs_conv((const u8 *) info.artist, sizeof(info.artist), (u8 *) info.artist, sizeof(info.artist));
					charsets_ucs_conv((const u8 *) info.title, sizeof(info.title), (u8 *) info.title, sizeof(info.title));
					charsets_ucs_conv((const u8 *) info.album, sizeof(info.album), (u8 *) info.album, sizeof(info.album));
					charsets_ucs_conv((const u8 *) info.comment, sizeof(info.comment), (u8 *) info.comment, sizeof(info.comment));
				}
				break;
			default:
				break;
		}

		if (info.artist[0] != '\0' && info.album[0] != '\0' && info.title[0] != '\0')
			SPRINTF_S(tag, "%s - %s - %s", info.artist, info.album, info.title);
		else if (info.album[0] != '\0' && info.title[0] != '\0')
			SPRINTF_S(tag, "%s - %s", info.album, info.title);
		else if (info.artist[0] != '\0' && info.title[0] != '\0')
			SPRINTF_S(tag, "%s - %s", info.artist, info.title);
		else if (info.title[0] != '\0')
			SPRINTF_S(tag, "%s", info.title);
		else {
			int i = music_get_current_pos();
			MusicListEntry *fl = musiclist_get(&g_music_list, i);

			if (fl) {
				const char *p = strrchr(fl->lpath, '/');

				if (p == NULL || *(p + 1) == '\0') {
					SPRINTF_S(tag, "%s", fl->lpath);
				} else {
					SPRINTF_S(tag, "%s", p + 1);
				}
			} else
				STRCPY_S(tag, "");
		}

		{
			struct music_info info;

			memset(&info, 0, sizeof(info));
			info.type = MD_GET_DECODERNAME | MD_GET_ENCODEMSG;
			if (musicdrv_get_info(&info) == 0) {
				if (info.encode_msg[0] != '\0') {
					STRCAT_S(tag, " ");
					STRCAT_S(tag, _("编码"));
					STRCAT_S(tag, _(": "));
					STRCAT_S(tag, info.encode_msg);
				}
			}
		}

		disp_putnstring(6 + DISP_FONTSIZE, 266 - DISP_FONTSIZE, COLOR_WHITE,
						(const u8 *) tag, (468 - DISP_FONTSIZE * 2) * 2 / DISP_FONTSIZE, 0, 0, DISP_FONTSIZE, 0);
	}
}
#endif

static void scene_draw_mp3bar(bool * firstdup)
{
	static u32 memory_free = 0;
	static u64 mem_prev, mem_now;

	char infostr[80];
	pspTime tm;
	u32 cpu, bus;
	u32 pos;
	int percent, lifetime, tempe, volt;

	if (*firstdup) {
		disp_duptocachealpha(50);
		*firstdup = false;
	} else
		disp_duptocache();

	disp_rectangle(5, 5, 474, 8 + DISP_FONTSIZE * 2, COLOR_WHITE);
	disp_fillrect(6, 6, 473, 7 + DISP_FONTSIZE * 2, config.msgbcolor);

	sceRtcGetCurrentClockLocalTime(&tm);
	pos = sceRtcGetDayOfWeek(tm.year, tm.month, tm.day);
	pos = pos >= 7 ? 7 : pos;
	power_get_clock(&cpu, &bus);
	SPRINTF_S(infostr,
			  _("%u年%u月%u日  %s  %02u:%02u:%02u   CPU/BUS: %d/%d"),
			  tm.year, tm.month, tm.day, get_week_str(pos), tm.hour, tm.minutes, tm.seconds, (int) cpu, (int) bus);

	{
		char temp[80];

		if (cache_get_loaded_size() != 0 || ccacher.memory_usage != 0) {
			SPRINTF_S(temp, "   %s: %u/%dKB", _("缓存"), cache_get_loaded_size(), ccacher.memory_usage / 1024);
			STRCAT_S(infostr, temp);
		}
	}

	sceRtcGetCurrentTick(&mem_now);

	if (pspDiffTime(&mem_now, &mem_prev) > 3.0) {
		memory_free = get_free_mem();
		sceRtcGetCurrentTick(&mem_prev);
	}

	disp_putstring(6 + DISP_FONTSIZE * 2, 6, COLOR_WHITE, (const u8 *) infostr);
	power_get_battery(&percent, &lifetime, &tempe, &volt);
	if (percent >= 0) {
		if (config.fontsize <= 12) {
			SPRINTF_S(infostr,
					  _
					  ("%s  剩余电量: %d%%(%d小时%d分钟)  电池温度: %d℃   剩余内存: %dKB"),
					  power_get_battery_charging(), percent, lifetime / 60, lifetime % 60, tempe, memory_free / 1024);
		} else {
			SPRINTF_S(infostr,
					  _("%s  剩余电量: %d%%(%d小时%d分钟)  电池温度: %d℃"), power_get_battery_charging(), percent, lifetime / 60, lifetime % 60, tempe);
		}
	} else
		SPRINTF_S(infostr, _("[电源供电]   剩余内存: %dKB"), memory_free / 1024);

#ifdef ENABLE_NLS
	if (strcmp(simple_textdomain(NULL), "en_US") == 0)
		disp_putstring(6, 7 + DISP_FONTSIZE, COLOR_WHITE, (const u8 *) infostr);
	else
		disp_putstring(6 + DISP_FONTSIZE, 7 + DISP_FONTSIZE, COLOR_WHITE, (const u8 *) infostr);
#else
	disp_putstring(6 + DISP_FONTSIZE, 7 + DISP_FONTSIZE, COLOR_WHITE, (const u8 *) infostr);
#endif

	scene_draw_lyric();

#ifdef ENABLE_MUSIC
	scene_draw_mp3bar_music_staff();
#endif
}

static int scene_mp3bar_handle_input(u32 key, pixel ** saveimage)
{
	double interval;

#ifdef ENABLE_MUSIC
	static u32 oldkey;
#endif

	if (!(key & PSP_CTRL_START)) {
		enable_exit_menu = 1;
	}

	sceRtcGetCurrentTick(&end);
	interval = pspDiffTime(&end, &start);

	switch (key) {
#ifdef ENABLE_MUSIC
		case PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER:
			if (win_msgbox(_("重启音频系统"), _("是"), _("否"), COLOR_WHITE, COLOR_WHITE, config.msgbcolor)) {
				power_down();
				power_down();
				power_down();
				power_up();
				power_up();
				power_up();
				power_up();
				ctrl_waitrelease();
				oldkey = 0;
			}
			break;
#endif
		case (PSP_CTRL_SELECT | PSP_CTRL_START):
			if (win_msgbox(_("是否退出软件?"), _("是"), _("否"), COLOR_WHITE, COLOR_WHITE, config.msgbcolor)) {
				scene_exit();
				return 1;
			}
			break;
		case PSP_CTRL_CIRCLE:
#ifdef ENABLE_MUSIC
			music_list_playorpause();
			ctrl_waitrelease();
#endif
			break;
		case PSP_CTRL_CROSS:
#ifdef ENABLE_MUSIC
			if (config.mp3cycle == conf_cycle_random)
				config.mp3cycle = conf_cycle_single;
			else
				config.mp3cycle++;
			music_set_cycle_mode(config.mp3cycle);
			ctrl_waitrelease();
#endif
			break;
		case PSP_CTRL_SQUARE:
#ifdef ENABLE_MUSIC
			if (music_list_stop() == 0) {
			}

			ctrl_waitrelease();
#endif
			break;
		case (PSP_CTRL_LEFT | PSP_CTRL_TRIANGLE):
#ifdef ENABLE_MUSIC
			config.lyricencode++;
			if ((u32) config.lyricencode > 4)
				config.lyricencode = 0;
			sceKernelDelayThread(200000);
#endif
			break;
		case (PSP_CTRL_RIGHT | PSP_CTRL_TRIANGLE):
#ifdef ENABLE_MUSIC
			config.mp3encode++;
			if ((u32) config.mp3encode > 4)
				config.mp3encode = 0;
//          music_set_encode(config.mp3encode);
			sceKernelDelayThread(200000);
#endif
			break;
		case PSP_CTRL_LTRIGGER:
#ifdef ENABLE_MUSIC
			if (key != oldkey) {
				sceRtcGetCurrentTick(&start);
				sceRtcGetCurrentTick(&end);
				interval = pspDiffTime(&end, &start);
			}

			if (interval >= 0.5) {
				musicdrv_fbackward(5);
				sceKernelDelayThread(200000);
			}
#endif
			break;
		case PSP_CTRL_RTRIGGER:
#ifdef ENABLE_MUSIC
			if (key != oldkey) {
				sceRtcGetCurrentTick(&start);
				sceRtcGetCurrentTick(&end);
				interval = pspDiffTime(&end, &start);
			}

			if (interval >= 0.5) {
				musicdrv_fforward(5);
				sceKernelDelayThread(200000);
			}
#endif
			break;
		case PSP_CTRL_UP:
#ifdef ENABLE_MUSIC
			musicdrv_fforward(30);
			sceKernelDelayThread(200000);
#endif
			break;
		case PSP_CTRL_DOWN:
#ifdef ENABLE_MUSIC
			musicdrv_fbackward(30);
			sceKernelDelayThread(200000);
#endif
			break;
		case PSP_CTRL_LEFT:
#ifdef ENABLE_MUSIC
			musicdrv_fbackward(5);
			sceKernelDelayThread(200000);
#endif
			break;
		case PSP_CTRL_RIGHT:
#ifdef ENABLE_MUSIC
			musicdrv_fforward(5);
			sceKernelDelayThread(200000);
#endif
			break;
		case PSP_CTRL_SELECT:
#ifdef ENABLE_MUSIC
			if (music_maxindex() == 0)
				break;
			scene_mp3_list();
#endif
			break;
		case PSP_CTRL_START:
			if (enable_exit_menu) {
				ctrl_waitrelease();
				if (*saveimage != NULL) {
					disp_putimage(0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, 0, 0, *saveimage);
					disp_flip();
					disp_putimage(0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, 0, 0, *saveimage);
					free(*saveimage);
					*saveimage = NULL;
				} else {
					disp_waitv();
#ifdef ENABLE_BG
					if (!bg_display())
#endif
						disp_fillvram(0);
					disp_flip();
					disp_duptocache();
				}
				return 1;
			}
	}

#ifdef ENABLE_MUSIC
	if ((!(key == PSP_CTRL_LTRIGGER || key == PSP_CTRL_RTRIGGER))
		&& (oldkey == PSP_CTRL_LTRIGGER || oldkey == PSP_CTRL_RTRIGGER)) {
		if (interval < 0.5) {
			if (oldkey == PSP_CTRL_LTRIGGER)
				music_prev();
			else if (oldkey == PSP_CTRL_RTRIGGER)
				music_next();
		}

		sceRtcGetCurrentTick(&start);
	}
	oldkey = key;
#endif

	return 0;
}

void scene_mp3bar(void)
{
	bool firstdup = true;
	u64 timer_start, timer_end;
	pixel *saveimage;

	debug_malloc();

	saveimage = (pixel *) memalign(16, PSP_SCREEN_WIDTH * PSP_SCREEN_HEIGHT * sizeof(pixel));
	if (saveimage != NULL)
		disp_getimage(0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, saveimage);

	sceRtcGetCurrentTick(&start);
	sceRtcGetCurrentTick(&end);
	sceRtcGetCurrentTick(&timer_start);

	enable_exit_menu = 0;

	while (1) {
		u32 key;

		sceRtcGetCurrentTick(&timer_end);
#ifdef ENABLE_MUSIC
		if (pspDiffTime(&timer_end, &timer_start) >= 1.0) {
			struct music_info info;

			memset(&info, 0, sizeof(info));
			info.type = MD_GET_INSKBPS;
			if (musicdrv_get_info(&info) == 0) {
				g_ins_kbps = info.ins_kbps;
			}

			sceRtcGetCurrentTick(&timer_start);
			secticks++;
		}
#endif

		scene_draw_mp3bar(&firstdup);
		disp_flip();
		key = ctrl_read_cont();

		if (key != 0) {
			secticks = 0;
		}

		if (scene_mp3bar_handle_input(key, &saveimage) == 1)
			return;

		if (config.autosleep != 0 && secticks > 60 * config.autosleep) {
			power_down();
			scePowerRequestSuspend();
			secticks = 0;
		}

		scene_mp3bar_delay_action();
	}
}
