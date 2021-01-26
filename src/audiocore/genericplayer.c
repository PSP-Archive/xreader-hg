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

#include <pspkernel.h>
#include <psprtc.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "config.h"
#include "ssv.h"
#include "scene.h"
#include "xaudiolib.h"
#include "musicmgr.h"
#include "musicdrv.h"
#include "strsafe.h"
#include "common/utils.h"
#include "apetaglib/APETag.h"
#include "freq_lock.h"
#include "freq_lock.h"
#include "musicinfo.h"
#include "dbg.h"
#include "buffered_reader.h"
#include "genericplayer.h"
#include "thread_lock.h"
#include "mediaengine.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

/**
 * 上次按快进退键类型
 */
bool g_last_seek_is_forward = false;

/**
 * 上次按快进退键起始时间
 */
volatile u64 g_last_seek_tick = 0;

/**
 *  按快进退键计数
 */
volatile u32 g_seek_count = 0;

/**
 * 休眠前播放状态
 */
int g_suspend_status = ST_UNKNOWN;

/**
 * 当前播放时间，以秒数计
 */
double g_play_time = 0;

/**
 * Wave音乐休眠时播放时间
 */
double g_suspend_playing_time = 0;

/**
 * 音乐快进、退秒数
 */
int g_seek_seconds = 0;

/**
 * 当前驱动播放状态
 */
int g_status = ST_UNKNOWN;

/**
 * 当前驱动播放状态写锁
 */
static struct psp_mutex_t generic_l;

/**
 * freq_locker ID
 */
int g_fid = -1;

/**
 * 当前播放音乐文件信息
 */
MusicInfo g_info = { 0 };

/**
 * 默认缓冲IO缓冲字节大小，最低不小于8192
 */
int g_io_buffer_size = BUFFERED_READER_BUFFER_SIZE;

reader_data data;

/**
 * 加锁
 */
int generic_lock(void)
{
	return xr_lock(&generic_l);
}

/**
 * 解锁
 */
int generic_unlock(void)
{
	return xr_unlock(&generic_l);
}

int generic_set_opt(const char *unused, const char *values)
{
	int argc;
	char **argv;

	build_args(values, &argc, &argv);
	clean_args(argc, argv);

	return 0;
}

void generic_set_playback(bool playing)
{
	if (playing) {
		if (g_fid < 0) {
			struct music_info info = { 0 };

			info.type = MD_GET_CPUFREQ;

			if (musicdrv_get_info(&info) == 0) {
				g_fid = freq_enter(info.psp_freq[0], info.psp_freq[1]);
			}
		}
	} else {
		if (g_fid >= 0) {
			freq_leave(g_fid);
			g_fid = -1;
		}
	}
}

int generic_set_status(int status)
{
	int prev;

	generic_lock();
	prev = g_status;
	g_status = status;
	generic_unlock();

	return prev;
}

int generic_play(void)
{
	generic_lock();
	generic_set_playback(true);
	generic_set_status(ST_PLAYING);
	generic_unlock();

	return 0;
}

int generic_pause(void)
{
	generic_lock();
	generic_set_playback(false);
	generic_set_status(ST_PAUSED);
	generic_unlock();

	return 0;
}

/**
 * 得到当前驱动的播放状态
 *
 * @return 状态
 */
int generic_get_status(void)
{
	return g_status;
}

/**
 * 快进音乐文件
 *
 * @param sec 秒数
 *
 * @return 成功时返回0
 */
int generic_fforward(int sec)
{
	generic_lock();

	if (g_status == ST_PLAYING || g_status == ST_PAUSED || g_status == ST_FBACKWARD) {
		generic_set_status(ST_FFORWARD);
	}

	g_seek_seconds = sec;

	sceRtcGetCurrentTick((u64 *) & g_last_seek_tick);
	g_last_seek_is_forward = true;
	g_seek_count++;
	generic_unlock();

	return 0;
}

/**
 * 快退音乐文件
 *
 * @param sec 秒数
 *
 * @return 成功时返回0
 */
int generic_fbackward(int sec)
{
	generic_lock();

	if (g_status == ST_PLAYING || g_status == ST_PAUSED || g_status == ST_FFORWARD) {
		generic_set_status(ST_FBACKWARD);
	}

	g_seek_seconds = sec;

	sceRtcGetCurrentTick((u64 *) & g_last_seek_tick);
	g_last_seek_is_forward = false;
	g_seek_count++;
	generic_unlock();

	return 0;
}

int generic_end(void)
{
	xr_lock_destroy(&generic_l);
	generic_set_playback(false);
	unload_prx();

	return 0;
}

int generic_init(void)
{
	xr_lock_init(&generic_l);
	g_seek_count = 0;

	if (config.use_vaudio)
		load_me_prx(VAUDIO);

	return 0;
}

int generic_resume(const char *spath, const char *lpath)
{
	generic_lock();
	generic_set_status(g_suspend_status);

	if (g_status == ST_PLAYING)
		generic_set_playback(true);

	generic_unlock();
	generic_set_status(ST_LOADED);

	return 0;
}

int generic_suspend(void)
{
	g_suspend_status = g_status;
	g_suspend_playing_time = g_play_time;

	return 0;
}

int generic_get_info(struct music_info *info)
{
	if (info->type & MD_GET_TITLE) {
		info->encode = g_info.tag.encode;

		if (info->encode != conf_encode_ucs)
			STRCPY_S(info->title, g_info.tag.title);
		else
			memcpy(info->title, g_info.tag.title, sizeof(info->title));
	}
	if (info->type & MD_GET_ALBUM) {
		info->encode = g_info.tag.encode;

		if (info->encode != conf_encode_ucs)
			STRCPY_S(info->album, g_info.tag.album);
		else
			memcpy(info->album, g_info.tag.album, sizeof(info->album));
	}
	if (info->type & MD_GET_ARTIST) {
		info->encode = g_info.tag.encode;

		if (info->encode != conf_encode_ucs)
			STRCPY_S(info->artist, g_info.tag.artist);
		else
			memcpy(info->artist, g_info.tag.artist, sizeof(info->artist));
	}
	if (info->type & MD_GET_COMMENT) {
		info->encode = g_info.tag.encode;

		if (info->encode != conf_encode_ucs)
			STRCPY_S(info->comment, g_info.tag.comment);
		else
			memcpy(info->comment, g_info.tag.comment, sizeof(info->comment));
	}
	if (info->type & MD_GET_DURATION) {
		info->duration = g_info.duration;
	}
	if (info->type & MD_GET_FREQ) {
		info->freq = g_info.sample_freq;
	}
	if (info->type & MD_GET_CHANNELS) {
		info->channels = g_info.channels;
	}
	if (info->type & MD_GET_AVGKBPS) {
		info->avg_kbps = g_info.avg_bps / 1000;
	}

	return 0;
}
