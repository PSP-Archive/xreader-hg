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
#include "dbg.h"
#include "ssv.h"
#include "genericplayer.h"
#include "musicinfo.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

#ifdef ENABLE_WAV

static int __end(void);

#define WAVE_BUFFER_SIZE (1024 * 95)

/**
 * Wave音乐播放缓冲
 */
static uint16_t *g_buff = NULL;

/**
 * Wave音乐播放缓冲大小，以帧数计
 */
static unsigned g_buff_frame_size;

/**
 * Wave音乐播放缓冲当前位置，以帧数计
 */
static int g_buff_frame_start;

/**
 * Wave音乐每帧字节数
 */
static int g_wav_byte_per_frame = 0;

/**
 * Wave音乐已播放帧数
 */
static int g_samples_decoded = 0;

/**
 * Wave音乐数据开始位置
 */
static uint32_t g_wav_data_offset = 0;

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

static int wav_seek_seconds(double seconds)
{
	int ret;

	if (data.use_buffer) {
		ret = buffered_reader_seek(data.r, g_wav_data_offset + (uint32_t) (seconds * g_info.sample_freq) * g_wav_byte_per_frame);
	} else {
		ret = sceIoLseek(data.fd, g_wav_data_offset + (uint32_t) (seconds * g_info.sample_freq) * g_wav_byte_per_frame, SEEK_SET);
	}

	if (ret >= 0) {
		g_buff_frame_size = g_buff_frame_start = 0;
		g_play_time = seconds;
		g_samples_decoded = (uint32_t) (seconds * g_info.sample_freq);
		return 0;
	}

	__end();
	return -1;
}

/**
 * Wave音乐播放回调函数，
 * 负责将解码数据填充声音缓存区
 *
 * @note 声音缓存区的格式为双声道，16位低字序
 *
 * @param buf 声音缓冲区指针
 * @param reqn 缓冲区帧大小
 * @param pdata 用户数据，无用
 */
static int wav_audiocallback(void *buf, unsigned int reqn, void *pdata)
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
			wav_seek_seconds(g_play_time);
		} else if (g_status == ST_FBACKWARD) {
			g_play_time -= g_seek_seconds;
			if (g_play_time < 0.) {
				g_play_time = 0.;
			}
			generic_lock();
			generic_set_status(ST_PLAYING);
			generic_set_playback(true);
			generic_unlock();
			wav_seek_seconds(g_play_time);
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
			incr = (double) (snd_buf_frame_size) / g_info.sample_freq;
			g_play_time += incr;
			snd_buf_frame_size = 0;
		} else {
			send_to_sndbuf(audio_buf, &g_buff[g_buff_frame_start * g_info.channels], avail_frame, g_info.channels);
			snd_buf_frame_size -= avail_frame;
			audio_buf += avail_frame * 2;

			if (g_samples_decoded >= g_info.samples) {
				__end();
				return -1;
			}

			if (data.use_buffer) {
				ret = buffered_reader_read(data.r, g_buff, WAVE_BUFFER_SIZE * sizeof(*g_buff));
			} else {
				ret = sceIoRead(data.fd, g_buff, WAVE_BUFFER_SIZE * sizeof(*g_buff));
			}

			if (ret <= 0) {
				__end();
				return -1;
			}

			g_buff_frame_size = ret / g_wav_byte_per_frame;
			g_buff_frame_start = 0;

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
	g_samples_decoded = 0;
	g_wav_data_offset = 0;

	memset(&g_info, 0, sizeof(g_info));

	return 0;
}

static int wave_get_16(SceUID fd, uint16_t * buf)
{
	int ret = sceIoRead(fd, buf, sizeof(*buf));

	if (ret == 2) {
		return 0;
	}

	return -1;
}

static int wave_get_32(SceUID fd, uint32_t * buf)
{
	int ret = sceIoRead(fd, buf, sizeof(*buf));

	if (ret == 4) {
		return 0;
	}

	return -1;
}

static int wave_skip_n_bytes(SceUID fd, int n)
{
	return sceIoLseek(fd, n, SEEK_CUR) >= 0 ? 0 : -1;
}

/**
 * 装载Wave音乐文件 
 *
 * @param spath 短路径名
 * @param lpath 长路径名
 *
 * @return 成功时返回0
 */
static int wav_load(const char *spath, const char *lpath)
{
	uint32_t temp;
	uint32_t fmt_chunk_size;

	__init();

	if (g_buff != NULL) {
		free(g_buff);
		g_buff = NULL;
	}
	g_buff = calloc_64(WAVE_BUFFER_SIZE, sizeof(*g_buff));
	if (g_buff == NULL) {
		__end();
		return -1;
	}

	data.use_buffer = true;
	data.fd = sceIoOpen(spath, PSP_O_RDONLY, 0777);

	if (data.fd < 0) {
		__end();
		return -1;
	}

	g_info.filesize = sceIoLseek(data.fd, 0, PSP_SEEK_END);
	sceIoLseek(data.fd, 0, PSP_SEEK_SET);

	// 'RIFF' keyword
	if (wave_get_32(data.fd, &temp) != 0) {
		__end();
		return -1;
	}
	if (temp != 0x46464952) {
		__end();
		return -1;
	}
	// chunk size: 36 + SubChunk2Size
	if (wave_get_32(data.fd, &temp) != 0) {
		__end();
		return -1;
	}
	// 'WAVE' keyword
	if (wave_get_32(data.fd, &temp) != 0) {
		__end();
		return -1;
	}
	if (temp != 0x45564157) {
		__end();
		return -1;
	}
	// 'fmt' keyword
	if (wave_get_32(data.fd, &temp) != 0) {
		__end();
		return -1;
	}
	if (temp != 0x20746d66) {
		__end();
		return -1;
	}

	if (wave_get_32(data.fd, &fmt_chunk_size) != 0) {
		__end();
		return -1;
	}
	// Format tag: 1 if PCM
	temp = 0;
	if (wave_get_16(data.fd, (uint16_t *) & temp) != 0) {
		__end();
		return -1;
	}
	if (temp != 1) {
		__end();
		return -1;
	}
	// Channels
	if (wave_get_16(data.fd, (uint16_t *) & g_info.channels) != 0) {
		__end();
		return -1;
	}
	// Sample freq
	if (wave_get_32(data.fd, (uint32_t *) & g_info.sample_freq) != 0) {
		__end();
		return -1;
	}
	// u8 rate
	if (wave_get_32(data.fd, (uint32_t *) & temp) != 0) {
		__end();
		return -1;
	}
	g_info.avg_bps = (double) temp *8;

	// u8 per sample
	if (wave_get_16(data.fd, (uint16_t *) & g_wav_byte_per_frame) != 0) {
		__end();
		return -1;
	}
	// bit per sample
	if (wave_get_16(data.fd, (uint16_t *) & temp) != 0) {
		__end();
		return -1;
	}
	// skip addition information
	if (fmt_chunk_size == 18) {
		uint16_t addition_size = 0;

		if (wave_get_16(data.fd, &addition_size) != 0) {
			__end();
			return -1;
		}
		wave_skip_n_bytes(data.fd, addition_size);
		g_wav_data_offset = 44 + 2 + addition_size;
	} else {
		g_wav_data_offset = 44;
	}
	if (wave_get_32(data.fd, (uint32_t *) & temp) != 0) {
		__end();
		return -1;
	}
	if (temp != 0x61746164) {
		__end();
		return -1;
	}
	// get size of data
	if (wave_get_32(data.fd, (uint32_t *) & temp) != 0) {
		__end();
		return -1;
	}
	if (g_wav_byte_per_frame == 0 || g_info.sample_freq == 0 || g_info.channels == 0) {
		__end();
		return -1;
	}

	if (data.use_buffer) {
		u32 cur_pos;

		cur_pos = sceIoLseek(data.fd, 0, PSP_SEEK_CUR);
		sceIoClose(data.fd);
		data.fd = -1;

		data.r = buffered_reader_open(spath, g_io_buffer_size, 1);

		if (data.r == NULL) {
			__end();
			return -1;
		}

		buffered_reader_seek(data.r, cur_pos);
	}

	g_info.samples = temp / g_wav_byte_per_frame;
	g_info.duration = (double) (temp) / g_wav_byte_per_frame / g_info.sample_freq;

	generic_readtag(&g_info, spath);

	if (config.use_vaudio)
		xAudioSetFrameSize(2048);
	else
		xAudioSetFrameSize(4096);

	if (xAudioInit() < 0) {
		__end();
		return -1;
	}

	if (xAudioSetFrequency(g_info.sample_freq) < 0) {
		__end();
		return -1;
	}

	xAudioSetChannelCallback(0, wav_audiocallback, NULL);
	generic_set_status(ST_LOADED);

	return 0;
}

/**
 * 停止Wave音乐文件的播放，销毁资源等
 *
 * @note 可以在播放线程中调用
 *
 * @return 成功时返回0
 */
static int __end(void)
{
	xAudioEndPre();

	generic_set_status(ST_STOPPED);

	if (data.use_buffer) {
		if (data.r != NULL) {
			buffered_reader_close(data.r);
			data.r = NULL;
		}
	} else {
		if (data.fd >= 0) {
			sceIoClose(data.fd);
			data.fd = -1;
		}
	}

	g_play_time = 0.;

	return 0;
}

/**
 * 停止Wave音乐文件的播放，销毁所占有的线程、资源等
 *
 * @note 不可以在播放线程中调用，必须能够多次重复调用而不死机
 *
 * @return 成功时返回0
 */
static int wav_end(void)
{
	__end();

	xAudioEnd();

	if (g_buff != NULL) {
		free(g_buff);
		g_buff = NULL;
	}

	generic_set_status(ST_STOPPED);
	generic_end();

	return 0;
}

/**
 * PSP准备休眠时Wave的操作
 *
 * @return 成功时返回0
 */
static int wav_suspend(void)
{
	generic_suspend();
	wav_end();

	return 0;
}

/**
 * PSP准备从休眠时恢复的Wave的操作
 *
 * @param spath 当前播放音乐名，8.3路径形式
 * @param lpath 当前播放音乐名，长文件或形式
 *
 * @return 成功时返回0
 */
static int wav_resume(const char *spath, const char *lpath)
{
	int ret;

	ret = wav_load(spath, lpath);
	if (ret != 0) {
		dbg_printf(d, "%s: wav_load failed %d", __func__, ret);
		return -1;
	}

	g_play_time = g_suspend_playing_time;
	wav_seek_seconds(g_play_time);
	g_suspend_playing_time = 0;

	generic_resume(spath, lpath);

	return 0;
}

/**
 * 得到Wave音乐文件相关信息
 *
 * @param pinfo 信息结构体指针
 *
 * @return
 */
static int wav_get_info(struct music_info *pinfo)
{
	if (pinfo->type & MD_GET_CURTIME) {
		pinfo->cur_time = g_play_time;
	}
	if (pinfo->type & MD_GET_CPUFREQ) {
		pinfo->psp_freq[0] = 33;
		pinfo->psp_freq[1] = 16;
	}
	if (pinfo->type & MD_GET_FREQ) {
		pinfo->freq = g_info.sample_freq;
	}
	if (pinfo->type & MD_GET_CHANNELS) {
		pinfo->channels = g_info.channels;
	}
	if (pinfo->type & MD_GET_AVGKBPS) {
		pinfo->avg_kbps = g_info.avg_bps / 1000;
	}
	if (pinfo->type & MD_GET_DECODERNAME) {
		STRCPY_S(pinfo->decoder_name, "wave");
	}
	if (pinfo->type & MD_GET_ENCODEMSG) {
		SPRINTF_S(pinfo->encode_msg, "");
	}

	return generic_get_info(pinfo);
}

/**
 * 检测是否为WAV文件，目前只检查文件后缀名
 *
 * @param spath 当前播放音乐名，8.3路径形式
 *
 * @return 是WAV文件返回1，否则返回0
 */
static int wav_probe(const char *spath)
{
	const char *p;

	p = utils_fileext(spath);

	if (p) {
		if (stricmp(p, "wav") == 0) {
			return 1;
		}
		if (stricmp(p, "wave") == 0) {
			return 1;
		}
	}

	return 0;
}

static int wav_set_opt(const char *unused, const char *values)
{
	int argc, i;
	char **argv;

	dbg_printf(d, "%s: options are %s", __func__, values);

	build_args(values, &argc, &argv);

	if (config.use_vaudio)
		g_io_buffer_size = BUFFERED_READER_BUFFER_SIZE / 2;
	else
		g_io_buffer_size = BUFFERED_READER_BUFFER_SIZE;

	for (i = 0; i < argc; ++i) {
		if (!strncasecmp(argv[i], "wav_buffer_size", sizeof("wav_buffer_size") - 1)) {
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

	generic_set_opt(unused, values);

	return 0;
}

static struct music_ops wav_ops = {
	.name = "wave",
	.set_opt = wav_set_opt,
	.load = wav_load,
	.play = NULL,
	.pause = NULL,
	.end = wav_end,
	.get_status = NULL,
	.fforward = NULL,
	.fbackward = NULL,
	.suspend = wav_suspend,
	.resume = wav_resume,
	.get_info = wav_get_info,
	.probe = wav_probe,
	.next = NULL
};

int wav_init(void)
{
	return register_musicdrv(&wav_ops);
}

#endif
