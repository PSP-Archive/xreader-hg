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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <pspsdk.h>
#include <pspkernel.h>
#include <pspaudio.h>
#include <psprtc.h>
#include <pspaudiocodec.h>
#include <limits.h>
#include "config.h"
#include "ssv.h"
#include "strsafe.h"
#include "musicdrv.h"
#include "xaudiolib.h"
#include "dbg.h"
#include "scene.h"
#include "genericplayer.h"
#include "common/utils.h"
#include "aacplayer.h"
#include "buffered_reader.h"
#include "malloc.h"
#include "mediaengine.h"
#include "neaacdec.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

#ifdef ENABLE_AAC

#define BUFF_SIZE	8*1152
#define AAC_BUFFER_SIZE	FAAD_MIN_STREAMSIZE*2

/**
 * AAC音乐播放缓冲
 */
static uint16_t *g_buff = NULL;

/**
 * AAC音乐播放缓冲大小，以帧数计
 */
static unsigned g_buff_frame_size;

/**
 * AAC音乐播放缓冲当前位置，以帧数计
 */
static int g_buff_frame_start;

/**
 * Media Engine buffer缓存
 */
static unsigned long aac_codec_buffer[65] __attribute__ ((aligned(64)));

static short aac_mix_buffer[2048 * 2] __attribute__ ((aligned(64)));

static u16 aac_data_align;
static u32 aac_data_start;
static u32 aac_data_size;
static u32 aac_sample_per_frame;
static bool aac_getEDRAM;

/**
 * 初始化驱动变量资源等
 *
 * @return 成功时返回0
 */
static int __init(void)
{
	generic_init();

	generic_set_status(ST_UNKNOWN);

	g_seek_seconds = 0;
	g_play_time = 0.;
	memset(&g_info, 0, sizeof(g_info));

	aac_data_align = 0;
	aac_data_start = 0;
	aac_data_size = 0;
	aac_getEDRAM = false;

	aac_sample_per_frame = 1024;

	memset(&data, 0, sizeof(data));
	data.fd = -1;

	return 0;
}

/**
 * 停止AT3音乐文件的播放，销毁资源等
 *
 * @note 可以在播放线程中调用
 *
 * @return 成功时返回0
 */
static int __end(void)
{
	xAudioEndPre();

	g_play_time = 0.;
	generic_set_status(ST_STOPPED);

	return 0;
}

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

/**
 * AAC音乐播放回调函数
 * 负责将解码数据填充声音缓存区
 *
 * @note 声音缓存区的格式为双声道，16位低字序
 *
 * @param buf 声音缓冲区指针
 * @param reqn 缓冲区帧大小
 * @param pdata 用户数据，无用
 */
static int aac_audiocallback(void *buf, unsigned int reqn, void *pdata)
{
	int avail_frame;
	int snd_buf_frame_size = (int) reqn;
	signed short *audio_buf = buf;
	double incr;

	UNUSED(pdata);

	if (g_status == ST_FFORWARD || g_status == ST_FBACKWARD) {
		generic_lock();
		generic_set_status(ST_PLAYING);
		generic_set_playback(true);
		generic_unlock();
	}

	if (g_status == ST_PAUSED) {
		xAudioClearSndBuf(buf, snd_buf_frame_size);
		return 0;
	}

	while (snd_buf_frame_size > 0) {
		avail_frame = g_buff_frame_size - g_buff_frame_start;

		if (avail_frame >= snd_buf_frame_size) {
			send_to_sndbuf(audio_buf, &g_buff[g_buff_frame_start * 2], snd_buf_frame_size, 2);
			g_buff_frame_start += snd_buf_frame_size;
			audio_buf += snd_buf_frame_size * 2;
			snd_buf_frame_size = 0;
		} else {
			int samplesdecoded;
			unsigned char aac_header_buf[7];
			int aac_header;
			int frame_size;
			u8 *aac_data_buffer;
			int res;
			uint16_t *output;

			send_to_sndbuf(audio_buf, &g_buff[g_buff_frame_start * 2], avail_frame, 2);
			snd_buf_frame_size -= avail_frame;
			audio_buf += avail_frame * 2;

			memset(aac_mix_buffer, 0, sizeof(aac_mix_buffer));

			if (data.use_buffer) {
				if (buffered_reader_read(data.r, aac_header_buf, 7) != 7) {
					__end();
					return -1;
				}
			} else {
				if (sceIoRead(data.fd, aac_header_buf, 7) != 7) {
					__end();
					return -1;
				}
			}

			aac_header = aac_header_buf[3];

			aac_header = (aac_header << 8) | aac_header_buf[4];
			aac_header = (aac_header << 8) | aac_header_buf[5];
			aac_header = (aac_header << 8) | aac_header_buf[6];

			frame_size = aac_header & 67100672;
			frame_size = frame_size >> 13;
			frame_size = frame_size - 7;

			aac_data_buffer = (u8 *) memalign(64, frame_size);

			if (data.use_buffer) {
				if (buffered_reader_read(data.r, aac_data_buffer, frame_size) != frame_size) {
					free(aac_data_buffer);
					__end();
					return -1;
				}
			} else {
				if (sceIoRead(data.fd, aac_data_buffer, frame_size) != frame_size) {
					free(aac_data_buffer);
					__end();
					return -1;
				}
			}

			aac_codec_buffer[6] = (unsigned long) aac_data_buffer;
			aac_codec_buffer[8] = (unsigned long) aac_mix_buffer;
			aac_codec_buffer[7] = frame_size;
			aac_codec_buffer[9] = aac_sample_per_frame * 4;

			res = sceAudiocodecDecode(aac_codec_buffer, 0x1003);

			free(aac_data_buffer);

			if (res < 0) {
				__end();
				return -1;
			}

			samplesdecoded = aac_sample_per_frame;
			output = &g_buff[0];
			memcpy(output, aac_mix_buffer, samplesdecoded * 4);
			g_buff_frame_size = samplesdecoded;
			g_buff_frame_start = 0;
			incr = (double) samplesdecoded / g_info.sample_freq;
			g_play_time += incr;
		}
	}

	return 0;
}

static int aac_load(const char *spath, const char *lpath)
{
	int ret;
	FILE *aacfp = NULL;
	unsigned char *aac_buffer = NULL;
	NeAACDecHandle decoder = NULL;
	NeAACDecConfigurationPtr aac_config = NULL;
	unsigned long buffervalid;
	unsigned long bufferconsumed;
	unsigned long tempsamplerate;
	unsigned char tempchannels;

	__init();

	aacfp = fopen(spath, "rb");

	if (aacfp == NULL) {
		goto failed;
	}

	aac_config = NULL;
	buffervalid = bufferconsumed = 0;

	decoder = NeAACDecOpen();

	if (decoder == NULL) {
		goto failed;
	}

	aac_config = NeAACDecGetCurrentConfiguration(decoder);
	aac_config->useOldADTSFormat = 0;
	aac_config->dontUpSampleImplicitSBR = 1;

	NeAACDecSetConfiguration(decoder, aac_config);

	aac_buffer = malloc(AAC_BUFFER_SIZE);

	if (aac_buffer == NULL) {
		goto failed;
	}

	buffervalid = fread(aac_buffer, 1, AAC_BUFFER_SIZE, aacfp);

	if (buffervalid <= 0) {
		goto failed;
	}

	if (!strncmp((const char *) aac_buffer, "ID3", 3)) {
		int size = 0;

		fseek(aacfp, 0, SEEK_SET);
		size = (aac_buffer[6] << 21) | (aac_buffer[7] << 14) | (aac_buffer[8] << 7)
			| aac_buffer[9];
		size += 10;
		fread(aac_buffer, 1, size, aacfp);
		buffervalid = fread(aac_buffer, 1, AAC_BUFFER_SIZE, aacfp);
	}

	bufferconsumed = NeAACDecInit(decoder, aac_buffer, buffervalid, &tempsamplerate, &tempchannels);
	g_info.channels = tempchannels;
	g_info.sample_freq = tempsamplerate;

	if (g_info.channels <= 0 || g_info.channels > 2) {
		goto failed;
	}

	if (aacfp != NULL) {
		fclose(aacfp);
		aacfp = NULL;
	}

	if (decoder != NULL) {
		NeAACDecClose(decoder);
		decoder = NULL;
	}

	if (aac_buffer != NULL) {
		free(aac_buffer);
		aac_buffer = NULL;
	}

	aac_config = NULL;

	data.use_buffer = true;

	if (data.use_buffer) {
		data.r = buffered_reader_open(spath, g_io_buffer_size, 1);

		if (data.r == NULL) {
			__end();
			return -1;
		}

		g_info.filesize = buffered_reader_length(data.r);
	} else {
		data.fd = sceIoOpen(spath, PSP_O_RDONLY, 0777);

		if (data.fd < 0) {
			__end();
			return -1;
		}

		g_info.filesize = sceIoLseek(data.fd, 0, PSP_SEEK_END);
		sceIoLseek(data.fd, 0, PSP_SEEK_SET);
	}

	if (config.use_vaudio)
		ret = load_me_prx(VAUDIO | AVCODEC);
	else
		ret = load_me_prx(AVCODEC);

	if (ret < 0) {
		dbg_printf(d, "%s: load_me_prx failed", __func__);
		goto failed;
	}

	memset(aac_codec_buffer, 0, sizeof(aac_codec_buffer));

	if (sceAudiocodecCheckNeedMem(aac_codec_buffer, 0x1003) < 0) {
		goto failed;
	}

	if (sceAudiocodecGetEDRAM(aac_codec_buffer, 0x1003) < 0) {
		goto failed;
	}

	aac_getEDRAM = true;

	aac_codec_buffer[10] = g_info.sample_freq;
	if (sceAudiocodecInit(aac_codec_buffer, 0x1003) < 0) {
		goto failed;
	}

	g_info.avg_bps = 0;
	g_info.duration = 0;

	ret = xAudioInit();

	if (ret < 0) {
		goto failed;
	}

	ret = xAudioSetFrequency(g_info.sample_freq);

	if (ret < 0) {
		goto failed;
	}

	xAudioSetChannelCallback(0, aac_audiocallback, NULL);

	g_buff = xAudioAlloc(0, BUFF_SIZE);

	if (g_buff == NULL) {
		goto failed;
	}

	return 0;

  failed:
	if (aacfp != NULL) {
		fclose(aacfp);
		aacfp = NULL;
	}

	if (decoder != NULL) {
		NeAACDecClose(decoder);
		decoder = NULL;
	}

	if (aac_buffer != NULL) {
		free(aac_buffer);
		aac_buffer = NULL;
	}

	aac_config = NULL;

	__end();
	return -1;
}

static int aac_end(void)
{
	dbg_printf(d, "%s", __func__);

	__end();

	xAudioEnd();

	generic_set_status(ST_STOPPED);
	generic_end();

	if (aac_getEDRAM) {
		sceAudiocodecReleaseEDRAM(aac_codec_buffer);
		aac_getEDRAM = false;
	}

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

	if (g_buff != NULL) {
		xAudioFree(g_buff);
		g_buff = NULL;
	}

	return 0;
}

static int aac_get_info(struct music_info *info)
{
	if (info->type & MD_GET_CURTIME) {
		info->cur_time = g_play_time;
	}
	if (info->type & MD_GET_CPUFREQ) {
		info->psp_freq[0] = 49;
		info->psp_freq[1] = 16;
	}
	if (info->type & MD_GET_DECODERNAME) {
		STRCPY_S(info->decoder_name, "AAC");
	}
	if (info->type & MD_GET_ENCODEMSG) {
		info->encode_msg[0] = '\0';
	}

	return generic_get_info(info);
}

/**
 * 检测是否为AT3文件，目前只检查文件后缀名
 *
 * @param spath 当前播放音乐名，8.3路径形式
 *
 * @return 是AT3文件返回1，否则返回0
 */
static int aac_probe(const char *spath)
{
	const char *p;

	p = utils_fileext(spath);

	if (p) {
		if (stricmp(p, "aac") == 0) {
			return 1;
		}
	}

	return 0;
}

/**
 * PSP准备休眠时AAC的操作
 *
 * @return 成功时返回0
 */
static int aac_suspend(void)
{
	generic_suspend();
	aac_end();

	return 0;
}

/**
 * PSP准备从休眠时恢复的AAC的操作
 *
 * @param spath 当前播放音乐名，8.3路径形式
 * @param lpath 当前播放音乐名，长文件或形式
 *
 * @return 成功时返回0
 */
static int aac_resume(const char *spath, const char *lpath)
{
	int ret;

	ret = aac_load(spath, lpath);

	if (ret != 0) {
		dbg_printf(d, "%s: aac_load failed %d", __func__, ret);
		return -1;
	}

	generic_resume(spath, lpath);

	return 0;
}

static int aac_set_opt(const char *unused, const char *values)
{
	int argc, i;
	char **argv;

	dbg_printf(d, "%s: options are %s", __func__, values);

	build_args(values, &argc, &argv);

	for (i = 0; i < argc; ++i) {
		if (!strncasecmp(argv[i], "aac_buffer_size", sizeof("aac_buffer_size") - 1)) {
			const char *p = argv[i];

			if ((p = strrchr(p, '=')) != NULL) {
				p++;

				g_io_buffer_size = atoi(p);

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

static struct music_ops aac_ops = {
	.name = "aac",
	.set_opt = aac_set_opt,
	.load = aac_load,
	.play = NULL,
	.pause = NULL,
	.end = aac_end,
	.get_status = NULL,
	.fforward = NULL,
	.fbackward = NULL,
	.suspend = aac_suspend,
	.resume = aac_resume,
	.get_info = aac_get_info,
	.probe = aac_probe,
	.next = NULL,
};

int aac_init()
{
	return register_musicdrv(&aac_ops);
}

#endif
