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
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "config.h"
#include "scene.h"
#include "xaudiolib.h"
#include "musicmgr.h"
#include "musicdrv.h"
#include "strsafe.h"
#include "common/utils.h"
#include "buffered_reader.h"
#include "tta/ttalib.h"
#include "ssv.h"
#include "simple_gettext.h"
#include "dbg.h"
#include "mp3info.h"
#include "musicinfo.h"
#include "genericplayer.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

#ifdef ENABLE_TTA

static int __end(void);

#define TTA_BUFFER_SIZE (PCM_BUFFER_LENGTH * MAX_NCH)

/**
 * TTA音乐播放缓冲
 */
static uint16_t *g_buff = NULL;

/**
 * TTA音乐播放缓冲大小，以帧数计
 */
static unsigned g_buff_frame_size;

/**
 * TTA音乐播放缓冲当前位置，以帧数计
 */
static int g_buff_frame_start;

/**
 * TTA音乐信息
 */
static tta_info ttainfo;

/**
 * TTA音乐已播放帧数
 */
static int g_samples_decoded = 0;

/**
 * TTA音乐数据开始位置
 */
static uint32_t g_tta_data_offset = 0;

/**
 * 复制数据到声音缓冲区
 *
 * @note 声音缓存区的格式为双声道，16位低字序
 *
 * @param buf 声音缓冲区指针
 * @param srcbuf 解码数据缓冲区指针
 * @param frames 复制帧数
 * @param channels 声道数
 */
static void send_to_sndbuf(void *buf, uint16_t * srcbuf, int frames, int channels)
{
	int n;
	signed short *p = (signed short *) buf;

	if (frames <= 0)
		return;

	if (channels == 2) {
		memcpy(buf, srcbuf, frames * channels * sizeof(*srcbuf));
	} else {
		for (n = 0; n < frames * channels; n++) {
			*p++ = srcbuf[n];
			*p++ = srcbuf[n];
		}
	}

}

static int tta_seek_seconds(double seconds)
{
	if (set_position(seconds * 1000 / SEEK_STEP) == 0) {
		g_buff_frame_size = g_buff_frame_start = 0;
		g_play_time = seconds;
		g_samples_decoded = (uint32_t) (seconds * g_info.sample_freq);
		return 0;
	}

	__end();
	return -1;
}

/**
 * TTA音乐播放回调函数，
 * 负责将解码数据填充声音缓存区
 *
 * @note 声音缓存区的格式为双声道，16位低字序
 *
 * @param buf 声音缓冲区指针
 * @param reqn 缓冲区帧大小
 * @param pdata 用户数据，无用
 */
static int tta_audiocallback(void *buf, unsigned int reqn, void *pdata)
{
	int avail_frame;
	int snd_buf_frame_size = (int) reqn;
	int ret;
	double incr;
	signed short *audio_buf = buf;

	UNUSED(pdata);

	if (g_status != ST_PLAYING) {
		if (g_status == ST_FFORWARD) {
			g_play_time += g_seek_seconds;
			if (g_play_time >= g_info.duration) {
				__end();
				return -1;
			}
			generic_lock();
			generic_set_status(ST_PLAYING);
			generic_set_playback(true);
			generic_unlock();
			tta_seek_seconds(g_play_time);
		} else if (g_status == ST_FBACKWARD) {
			g_play_time -= g_seek_seconds;
			if (g_play_time < 0.) {
				g_play_time = 0.;
			}
			generic_lock();
			generic_set_status(ST_PLAYING);
			generic_set_playback(true);
			generic_unlock();
			tta_seek_seconds(g_play_time);
		}
		xAudioClearSndBuf(buf, snd_buf_frame_size);
		return 0;
	}

	while (snd_buf_frame_size > 0) {
		avail_frame = g_buff_frame_size - g_buff_frame_start;

		if (avail_frame >= snd_buf_frame_size) {
			send_to_sndbuf(audio_buf, &g_buff[g_buff_frame_start * g_info.channels], snd_buf_frame_size, g_info.channels);
			g_buff_frame_start += snd_buf_frame_size;
			audio_buf += snd_buf_frame_size * 2;
			snd_buf_frame_size = 0;
		} else {
			send_to_sndbuf(audio_buf, &g_buff[g_buff_frame_start * g_info.channels], avail_frame, g_info.channels);
			snd_buf_frame_size -= avail_frame;
			audio_buf += avail_frame * 2;

			if (g_samples_decoded >= g_info.samples) {
				__end();
				return -1;
			}
			ret = get_samples((u8 *) g_buff);
			if (ret <= 0) {
				__end();
				return -1;
			}

			g_buff_frame_size = ret;
			g_buff_frame_start = 0;

			incr = (double) (g_buff_frame_size) / g_info.sample_freq;
			g_play_time += incr;

			g_samples_decoded += g_buff_frame_size;
		}
	}

	return 0;
}

/**
 * 初始化驱动变量资源等
 *
 * @return 成功时返回0
 */
static int __init(void)
{
	generic_init();

	generic_set_status(ST_UNKNOWN);

	g_buff_frame_size = g_buff_frame_start = 0;
	g_seek_seconds = 0;

	g_play_time = 0.;

	g_samples_decoded = g_tta_data_offset = 0;
	memset(&g_info, 0, sizeof(g_info));

	return 0;
}

static int tta_read_tag(const char *spath)
{
	generic_readtag(&g_info, spath);

	return 0;
}

/**
 * 装载TTA音乐文件 
 *
 * @param spath 短路径名
 * @param lpath 长路径名
 *
 * @return 成功时返回0
 */
static int tta_load(const char *spath, const char *lpath)
{
	__init();

	if (tta_read_tag(spath) != 0) {
		__end();
		return -1;
	}

	if (g_buff != NULL) {
		free(g_buff);
		g_buff = NULL;
	}
	g_buff = calloc(TTA_BUFFER_SIZE, sizeof(*g_buff));
	if (g_buff == NULL) {
		__end();
		return -1;
	}

	if (open_tta_file(spath, &ttainfo, 0, g_io_buffer_size) < 0) {
		dbg_printf(d, "TTA Decoder Error - %s", get_error_str(ttainfo.STATE));
		close_tta_file(&ttainfo);
		return -1;
	}

	if (player_init(&ttainfo) != 0) {
		__end();
		return -1;
	}

	if (ttainfo.BPS == 0) {
		__end();
		return -1;
	}

	g_info.samples = ttainfo.DATALENGTH;
	g_info.duration = (double) ttainfo.LENGTH;
	g_info.sample_freq = ttainfo.SAMPLERATE;
	g_info.channels = ttainfo.NCH;
	g_info.filesize = ttainfo.FILESIZE;

	if (config.use_vaudio)
		xAudioSetFrameSize(2048);
	else
		xAudioSetFrameSize(4096);

	if (xAudioInit() < 0) {
		__end();
		return -1;
	}

	if (xAudioSetFrequency(ttainfo.SAMPLERATE) < 0) {
		__end();
		return -1;
	}

	xAudioSetChannelCallback(0, tta_audiocallback, NULL);

	generic_set_status(ST_LOADED);

	return 0;
}

/**
 * 停止TTA音乐文件的播放，销毁资源等
 *
 * @note 可以在播放线程中调用
 *
 * @return 成功时返回0
 */
static int __end(void)
{
	xAudioEndPre();

	generic_set_status(ST_STOPPED);
	g_play_time = 0.;

	return 0;
}

/**
 * 停止TTA音乐文件的播放，销毁所占有的线程、资源等
 *
 * @note 不可以在播放线程中调用，必须能够多次重复调用而不死机
 *
 * @return 成功时返回0
 */
static int tta_end(void)
{
	__end();

	xAudioEnd();

	if (g_buff != NULL) {
		free(g_buff);
		g_buff = NULL;
	}

	player_stop();
	close_tta_file(&ttainfo);
	generic_set_status(ST_STOPPED);
	generic_end();

	return 0;
}

/**
 * PSP准备休眠时TTA的操作
 *
 * @return 成功时返回0
 */
static int tta_suspend(void)
{
	generic_suspend();
	tta_end();

	return 0;
}

/**
 * PSP准备从休眠时恢复的TTA的操作
 *
 * @param spath 当前播放音乐名，8.3路径形式
 * @param lpath 当前播放音乐名，长文件名形式
 *
 * @return 成功时返回0
 */
static int tta_resume(const char *spath, const char *lpath)
{
	int ret;

	ret = tta_load(spath, lpath);
	if (ret != 0) {
		dbg_printf(d, "%s: tta_load failed %d", __func__, ret);
		return -1;
	}

	g_play_time = g_suspend_playing_time;
	tta_seek_seconds(g_play_time);
	g_suspend_playing_time = 0;

	generic_resume(spath, lpath);

	return 0;
}

/**
 * 得到TTA音乐文件相关信息
 *
 * @param pinfo 信息结构体指针
 *
 * @return
 */
static int tta_get_info(struct music_info *pinfo)
{
	generic_get_info(pinfo);

	if (pinfo->type & MD_GET_TITLE) {
		if (g_info.tag.title[0] == '\0') {
			pinfo->encode = conf_encode_gbk;
			STRCPY_S(pinfo->title, (const char *) ttainfo.ID3.title);
		}
	}
	if (pinfo->type & MD_GET_ARTIST) {
		if (g_info.tag.artist[0] == '\0') {
			pinfo->encode = conf_encode_gbk;
			STRCPY_S(pinfo->artist, (const char *) ttainfo.ID3.artist);
		}
	}
	if (pinfo->type & MD_GET_ALBUM) {
		if (g_info.tag.album[0] == '\0') {
			pinfo->encode = conf_encode_gbk;
			STRCPY_S(pinfo->album, (const char *) ttainfo.ID3.album);
		}
	}
	if (pinfo->type & MD_GET_CURTIME) {
		pinfo->cur_time = g_play_time;
	}
	if (pinfo->type & MD_GET_CPUFREQ) {
#ifdef _DEBUG
		pinfo->psp_freq[0] = 222;
#else
		if (config.use_vaudio)
			pinfo->psp_freq[0] = 166;
		else
			pinfo->psp_freq[0] = 133;
#endif
		pinfo->psp_freq[1] = pinfo->psp_freq[0] / 2;
	}
	if (pinfo->type & MD_GET_FREQ) {
		pinfo->freq = ttainfo.SAMPLERATE;
	}
	if (pinfo->type & MD_GET_CHANNELS) {
		pinfo->channels = ttainfo.NCH;
	}
	if (pinfo->type & MD_GET_AVGKBPS) {
		pinfo->avg_kbps = ttainfo.BITRATE;
	}
	if (pinfo->type & MD_GET_DECODERNAME) {
		STRCPY_S(pinfo->decoder_name, "tta");
	}
	if (pinfo->type & MD_GET_ENCODEMSG) {
		if (config.show_encoder_msg && g_status != ST_UNKNOWN) {
			SPRINTF_S(pinfo->encode_msg, "%s: %.2f", _("压缩率"), ttainfo.COMPRESS);
		} else {
			pinfo->encode_msg[0] = '\0';
		}
	}

	return 0;
}

/**
 * 检测是否为TTA文件，目前只检查文件后缀名
 *
 * @param spath 当前播放音乐名，8.3路径形式
 *
 * @return 是TTA文件返回1，否则返回0
 */
static int tta_probe(const char *spath)
{
	const char *p;

	p = utils_fileext(spath);

	if (p) {
		if (stricmp(p, "tta") == 0) {
			return 1;
		}
	}

	return 0;
}

static int tta_set_opt(const char *unused, const char *values)
{
	int argc, i;
	char **argv;

	if (config.use_vaudio)
		g_io_buffer_size = BUFFERED_READER_BUFFER_SIZE / 2;
	else
		g_io_buffer_size = BUFFERED_READER_BUFFER_SIZE;

	dbg_printf(d, "%s: options are %s", __func__, values);

	build_args(values, &argc, &argv);

	for (i = 0; i < argc; ++i) {
		if (!strncasecmp(argv[i], "tta_buffer_size", sizeof("tta_buffer_size") - 1)) {
			const char *p = argv[i];

			if ((p = strrchr(p, '=')) != NULL) {
				p++;
				g_io_buffer_size = atoi(p);

				if (config.use_vaudio)
					g_io_buffer_size = g_io_buffer_size / 2;

				if (g_io_buffer_size < 8192) {
					g_io_buffer_size = 8192;
				}
			}
		}
	}

	clean_args(argc, argv);

	return 0;
}

static struct music_ops tta_ops = {
	.name = "tta",
	.set_opt = tta_set_opt,
	.load = tta_load,
	.play = NULL,
	.pause = NULL,
	.end = tta_end,
	.get_status = NULL,
	.fforward = NULL,
	.fbackward = NULL,
	.suspend = tta_suspend,
	.resume = tta_resume,
	.get_info = tta_get_info,
	.probe = tta_probe,
	.next = NULL
};

int tta_init(void)
{
	return register_musicdrv(&tta_ops);
}

#endif
