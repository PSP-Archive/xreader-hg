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
#include "apetaglib/APETag.h"
#include "genericplayer.h"
#include "musicinfo.h"
#include "common/utils.h"
#include "at3player.h"
#include "buffered_reader.h"
#include "malloc.h"
#include "mediaengine.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

#ifdef ENABLE_AT3

#define BUFF_SIZE	8*1152

/**
 * AT3音乐播放缓冲
 */
static uint16_t *g_buff = NULL;

/**
 * AT3音乐播放缓冲大小，以帧数计
 */
static unsigned g_buff_frame_size;

/**
 * AT3音乐播放缓冲当前位置，以帧数计
 */
static int g_buff_frame_start;

/**
 * Media Engine buffer缓存
 */
static unsigned long at3_codec_buffer[65] __attribute__ ((aligned(64)));

static short at3_mix_buffer[2048 * 2] __attribute__ ((aligned(64)));

#define TYPE_ATRAC3 0x270
#define TYPE_ATRAC3PLUS 0xFFFE

static u16 at3_type;
static u16 at3_data_align;
static u32 at3_data_start;
static u32 at3_data_size;
static u8 at3_at3plus_flagdata[2];
static u16 at3_channel_mode;
static u32 at3_sample_per_frame;
static u8 *at3_data_buffer;
static bool at3_getEDRAM;

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
	memset(&data, 0, sizeof(data));
	memset(&g_info, 0, sizeof(g_info));

	data.fd = -1;
	data.use_buffer = true;

	at3_type = 0;
	at3_data_align = 0;
	at3_data_start = 0;
	at3_data_size = 0;
	at3_data_buffer = NULL;
	at3_getEDRAM = false;

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

static int at3_seek_seconds(double seconds)
{
	int ret;
	u32 frame_index;
	u32 pos;

	if (at3_sample_per_frame == 0) {
		__end();
		return -1;
	}

	frame_index = (u32) (seconds * g_info.sample_freq / at3_sample_per_frame);
	pos = at3_data_start;
	pos += frame_index * at3_data_align;

	dbg_printf(d, "%s: jump to frame %u pos 0x%08x", __func__, frame_index, pos);

	if (data.use_buffer) {
		ret = buffered_reader_seek(data.r, pos);
	} else {
		ret = sceIoLseek(data.fd, pos, SEEK_SET);
	}

	if (ret >= 0) {
		g_buff_frame_size = g_buff_frame_start = 0;
		g_play_time = seconds;
		return 0;
	}

	__end();
	return -1;
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
 * AT3音乐播放回调函数
 * 负责将解码数据填充声音缓存区
 *
 * @note 声音缓存区的格式为双声道，16位低字序
 *
 * @param buf 声音缓冲区指针
 * @param reqn 缓冲区帧大小
 * @param pdata 用户数据，无用
 */
static int at3_audiocallback(void *buf, unsigned int reqn, void *pdata)
{
	int avail_frame;
	int snd_buf_frame_size = (int) reqn;
	signed short *audio_buf = buf;
	double incr;

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
			at3_seek_seconds(g_play_time);
		} else if (g_status == ST_FBACKWARD) {
			g_play_time -= g_seek_seconds;
			if (g_play_time < 0.) {
				g_play_time = 0.;
			}
			generic_lock();
			generic_set_status(ST_PLAYING);
			generic_set_playback(true);
			generic_unlock();
			at3_seek_seconds(g_play_time);
		}
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
			unsigned long decode_type = 0x1001;
			int res;
			uint16_t *output;

			send_to_sndbuf(audio_buf, &g_buff[g_buff_frame_start * 2], avail_frame, 2);
			snd_buf_frame_size -= avail_frame;
			audio_buf += avail_frame * 2;
			memset(at3_mix_buffer, 0, 2048 * 2 * 2);

			if (at3_type == TYPE_ATRAC3) {
				memset(at3_data_buffer, 0, 0x180);
				if (data.use_buffer) {
					if (buffered_reader_read(data.r, at3_data_buffer, at3_data_align) != at3_data_align) {
						__end();
						return -1;
					}
				} else {
					if (sceIoRead(data.fd, at3_data_buffer, at3_data_align) != at3_data_align) {
						__end();
						return -1;
					}
				}
				if (at3_channel_mode) {
					memcpy(at3_data_buffer + at3_data_align, at3_data_buffer, at3_data_align);
				}
				decode_type = 0x1001;
			} else {
				memset(at3_data_buffer, 0, at3_data_align + 8);
				at3_data_buffer[0] = 0x0F;
				at3_data_buffer[1] = 0xD0;
				at3_data_buffer[2] = at3_at3plus_flagdata[0];
				at3_data_buffer[3] = at3_at3plus_flagdata[1];
				if (data.use_buffer) {
					if (buffered_reader_read(data.r, at3_data_buffer + 8, at3_data_align) != at3_data_align) {
						__end();
						return -1;
					}
				} else {
					if (sceIoRead(data.fd, at3_data_buffer + 8, at3_data_align)
						!= at3_data_align) {
						__end();
						return -1;
					}
				}
				decode_type = 0x1000;
			}

			at3_codec_buffer[6] = (unsigned long) at3_data_buffer;
			at3_codec_buffer[8] = (unsigned long) at3_mix_buffer;

			res = sceAudiocodecDecode(at3_codec_buffer, decode_type);

			if (res < 0) {
				__end();
				return -1;
			}

			samplesdecoded = at3_sample_per_frame;
			output = &g_buff[0];
			memcpy(output, at3_mix_buffer, samplesdecoded * 4);
			g_buff_frame_size = samplesdecoded;
			g_buff_frame_start = 0;
			incr = (double) samplesdecoded / g_info.sample_freq;
			g_play_time += incr;
		}
	}

	return 0;
}

static int at3_load(const char *spath, const char *lpath)
{
	int ret;
	u32 riff_header[2];
	u32 wavefmt_header[3];
	u8 *wavefmt_data;
	u32 data_header[2];

	__init();

	data.fd = sceIoOpen(spath, PSP_O_RDONLY, 0777);

	if (data.fd < 0) {
		goto failed;
	}

	if (sceIoRead(data.fd, riff_header, sizeof(riff_header)) != sizeof(riff_header)) {
		goto failed;
	}
	// RIFF
	if (riff_header[0] != 0x46464952) {
		goto failed;
	}

	if (sceIoRead(data.fd, wavefmt_header, sizeof(wavefmt_header)) != sizeof(wavefmt_header)) {
		goto failed;
	}
	// WAVEfmt
	if (wavefmt_header[0] != 0x45564157 || wavefmt_header[1] != 0x20746D66) {
		goto failed;
	}

	wavefmt_data = (u8 *) malloc(wavefmt_header[2]);

	if (wavefmt_data == NULL) {
		goto failed;
	}

	if (sceIoRead(data.fd, wavefmt_data, wavefmt_header[2]) != wavefmt_header[2]) {
		free(wavefmt_data);
		goto failed;
	}

	at3_type = *((u16 *) wavefmt_data);
	g_info.channels = *((u16 *) (wavefmt_data + 2));
	g_info.sample_freq = *((u32 *) (wavefmt_data + 4));
	at3_data_align = *((u16 *) (wavefmt_data + 12));

	if (at3_type == TYPE_ATRAC3PLUS) {
		at3_at3plus_flagdata[0] = wavefmt_data[42];
		at3_at3plus_flagdata[1] = wavefmt_data[43];
	}

	free(wavefmt_data);

	// Search for data header
	if (sceIoRead(data.fd, data_header, sizeof(data_header)) != sizeof(data_header)) {
		goto failed;
	}

	while (data_header[0] != 0x61746164) {
		sceIoLseek32(data.fd, data_header[1], PSP_SEEK_CUR);
		if (sceIoRead(data.fd, data_header, sizeof(data_header)) != sizeof(data_header)) {
			goto failed;
		}
	}

	at3_data_start = sceIoLseek32(data.fd, 0, PSP_SEEK_CUR);
	at3_data_size = data_header[1];

	if (at3_data_size % at3_data_align != 0) {
		dbg_printf(d, "%s: at3_data_size %d at3_data_align %d not align", __func__, at3_data_size, at3_data_align);
		goto failed;
	}

	if (config.use_vaudio)
		ret = load_me_prx(VAUDIO | ATRAC3PLUS);
	else
		ret = load_me_prx(AVCODEC);

	if (ret < 0) {
		dbg_printf(d, "%s: load_me_prx failed", __func__);
		goto failed;
	}

	if (at3_type == TYPE_ATRAC3) {
		at3_channel_mode = 0x0;
		// atract3 have 3 bitrate, 132k,105k,66k, 132k align=0x180, 105k align = 0x130, 66k align = 0xc0

		if (at3_data_align == 0xC0)
			at3_channel_mode = 0x1;

		at3_sample_per_frame = 1024;
		at3_data_buffer = (u8 *) memalign(64, 0x180);

		if (at3_data_buffer == NULL)
			goto failed;

		at3_codec_buffer[26] = 0x20;

		if (sceAudiocodecCheckNeedMem(at3_codec_buffer, 0x1001) < 0)
			goto failed;

		if (sceAudiocodecGetEDRAM(at3_codec_buffer, 0x1001) < 0)
			goto failed;

		at3_getEDRAM = true;
		at3_codec_buffer[10] = 4;
		at3_codec_buffer[44] = 2;

		if (at3_data_align == 0x130)
			at3_codec_buffer[10] = 6;

		if (sceAudiocodecInit(at3_codec_buffer, 0x1001) < 0) {
			goto failed;
		}
	} else if (at3_type == TYPE_ATRAC3PLUS) {
		int temp_size;
		int mod_64;

		at3_sample_per_frame = 2048;
		temp_size = at3_data_align + 8;
		mod_64 = temp_size & 0x3f;

		if (mod_64 != 0)
			temp_size += 64 - mod_64;
		at3_data_buffer = (u8 *) memalign(64, temp_size);

		if (at3_data_buffer == NULL)
			goto failed;

		at3_codec_buffer[5] = 0x1;
		at3_codec_buffer[10] = at3_at3plus_flagdata[1];
		at3_codec_buffer[10] = (at3_codec_buffer[10] << 8) | at3_at3plus_flagdata[0];
		at3_codec_buffer[12] = 0x1;
		at3_codec_buffer[14] = 0x1;

		if (sceAudiocodecCheckNeedMem(at3_codec_buffer, 0x1000) < 0)
			goto failed;

		if (sceAudiocodecGetEDRAM(at3_codec_buffer, 0x1000) < 0)
			goto failed;

		at3_getEDRAM = true;

		if (sceAudiocodecInit(at3_codec_buffer, 0x1000) < 0) {
			goto failed;
		}
	} else {
		goto failed;
	}

	g_info.duration = 0;
	g_info.avg_bps = 0;

	if (g_info.sample_freq != 0 && at3_data_align != 0) {
		g_info.duration = (double) at3_data_size *at3_sample_per_frame / at3_data_align / g_info.sample_freq;
		if (g_info.duration != 0) {
			g_info.avg_bps = (double) at3_data_size *8 / g_info.duration;
		}
	}

	if (data.use_buffer) {
		SceOff cur = sceIoLseek(data.fd, 0, PSP_SEEK_CUR);

		sceIoClose(data.fd);
		data.fd = -1;
		data.r = buffered_reader_open(spath, g_io_buffer_size, 1);

		if (data.r == NULL) {
			__end();
			return -1;
		}

		buffered_reader_seek(data.r, cur);
	}

	if (config.use_vaudio)
		xAudioSetFrameSize(2048);
	else
		xAudioSetFrameSize(4096);

	ret = xAudioInit();

	if (ret < 0) {
		goto failed;
	}

	ret = xAudioSetFrequency(g_info.sample_freq);

	if (ret < 0) {
		goto failed;
	}

	g_buff = xAudioAlloc(64, BUFF_SIZE);

	if (g_buff == NULL) {
		goto failed;
	}

	xAudioSetChannelCallback(0, at3_audiocallback, NULL);

	return 0;

  failed:
	__end();
	return -1;
}

static int at3_end(void)
{
	dbg_printf(d, "%s", __func__);

	__end();

	xAudioEnd();

	generic_set_status(ST_STOPPED);
	generic_end();

	if (data.use_buffer && data.r != NULL) {
		buffered_reader_close(data.r);
		data.r = NULL;
	}

	if (data.fd >= 0) {
		sceIoClose(data.fd);
		data.fd = -1;
	}

	if (at3_data_buffer) {
		free(at3_data_buffer);
		at3_data_buffer = NULL;
	}

	if (at3_getEDRAM) {
		sceAudiocodecReleaseEDRAM(at3_codec_buffer);
		at3_getEDRAM = false;
	}

	if (g_buff != NULL) {
		xAudioFree(g_buff);
		g_buff = NULL;
	}

	return 0;
}

static int at3_get_info(struct music_info *info)
{
	if (info->type & MD_GET_CURTIME) {
		info->cur_time = g_play_time;
	}
	if (info->type & MD_GET_CPUFREQ) {
		if (data.use_buffer)
			info->psp_freq[0] = 49;
		else
			info->psp_freq[0] = 33;

		info->psp_freq[1] = 16;
	}
	if (info->type & MD_GET_DECODERNAME) {
		if (at3_type == TYPE_ATRAC3)
			STRCPY_S(info->decoder_name, "atrac3");
		else if (at3_type == TYPE_ATRAC3PLUS)
			STRCPY_S(info->decoder_name, "atrac3plus");
		else
			STRCPY_S(info->decoder_name, "at3");
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
static int at3_probe(const char *spath)
{
	const char *p;

	p = utils_fileext(spath);

	if (p) {
		if (stricmp(p, "at3") == 0) {
			return 1;
		}
	}

	return 0;
}

/**
 * PSP准备休眠时at3的操作
 *
 * @return 成功时返回0
 */
static int at3_suspend(void)
{
	generic_suspend();
	at3_end();

	return 0;
}

/**
 * PSP准备从休眠时恢复的at3的操作
 *
 * @param spath 当前播放音乐名，8.3路径形式
 * @param lpath 当前播放音乐名，长文件或形式
 *
 * @return 成功时返回0
 */
static int at3_resume(const char *spath, const char *lpath)
{
	int ret;

	ret = at3_load(spath, lpath);

	if (ret != 0) {
		dbg_printf(d, "%s: at3_load failed %d", __func__, ret);
		return -1;
	}

	g_play_time = g_suspend_playing_time;
	at3_seek_seconds(g_play_time);
	g_suspend_playing_time = 0;

	generic_resume(spath, lpath);

	return 0;
}

static int at3_set_opt(const char *unused, const char *values)
{
	int argc, i;
	char **argv;

	g_io_buffer_size = BUFFERED_READER_BUFFER_SIZE;

	dbg_printf(d, "%s: options are %s", __func__, values);

	build_args(values, &argc, &argv);

	for (i = 0; i < argc; ++i) {
		if (!strncasecmp(argv[i], "at3_buffer_size", sizeof("at3_buffer_size") - 1)) {
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

	return 0;
}

static struct music_ops at3_ops = {
	.name = "at3",
	.set_opt = at3_set_opt,
	.load = at3_load,
	.play = NULL,
	.pause = NULL,
	.end = at3_end,
	.get_status = NULL,
	.fforward = NULL,
	.fbackward = NULL,
	.suspend = at3_suspend,
	.resume = at3_resume,
	.get_info = at3_get_info,
	.probe = at3_probe,
	.next = NULL,
};

int at3_init()
{
	return register_musicdrv(&at3_ops);
}

#endif
