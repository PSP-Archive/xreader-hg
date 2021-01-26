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
#include "m4aplayer.h"
#include "buffered_reader.h"
#include "malloc.h"
#include "mediaengine.h"
#include "mp4.h"
#include "neaacdec.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

#ifdef ENABLE_M4A

#define BUFF_SIZE	8*1152

/**
 * MP3音乐播放缓冲
 */
static uint16_t *g_buff = NULL;

/**
 * MP3音乐播放缓冲大小，以帧数计
 */
static unsigned g_buff_frame_size;

/**
 * MP3音乐播放缓冲当前位置，以帧数计
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

static MP4FileHandle mp4file;
static gint mp4track;
static int mp4sample_id;

static char g_vendor_str[80];

static bool g_force_up_sampling = false;

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
	mp4sample_id = 1;
	g_vendor_str[0] = '\0';

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

static int m4a_seek_seconds(double seconds)
{
	mp4sample_id = MP4GetSampleIdFromTime(mp4file, mp4track, seconds * g_info.sample_freq, 0);

	dbg_printf(d, "%s: jump to frame %d", __func__, mp4sample_id);

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
 * MP3音乐播放回调函数，ME版本
 * 负责将解码数据填充声音缓存区
 *
 * @note 声音缓存区的格式为双声道，16位低字序
 *
 * @param buf 声音缓冲区指针
 * @param reqn 缓冲区帧大小
 * @param pdata 用户数据，无用
 */
static int m4a_audiocallback(void *buf, unsigned int reqn, void *pdata)
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
			m4a_seek_seconds(g_play_time);
		} else if (g_status == ST_FBACKWARD) {
			g_play_time -= g_seek_seconds;
			if (g_play_time < 0.) {
				g_play_time = 0.;
			}
			generic_lock();
			generic_set_status(ST_PLAYING);
			generic_set_playback(true);
			generic_unlock();
			m4a_seek_seconds(g_play_time);
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
			int res;
			u_int8_t *buffer = NULL;
			u_int32_t buffer_size = 0;
			uint16_t *output;

			send_to_sndbuf(audio_buf, &g_buff[g_buff_frame_start * 2], avail_frame, 2);
			snd_buf_frame_size -= avail_frame;
			audio_buf += avail_frame * 2;
			memset(aac_mix_buffer, 0, sizeof(aac_mix_buffer));
			res = MP4ReadSample(mp4file, mp4track, mp4sample_id++, &buffer, &buffer_size, NULL, NULL, NULL, NULL);

			if (res == 0 || buffer == NULL) {
				if (buffer != NULL) {
					free(buffer);
				}

				__end();
				return -1;
			}

			aac_codec_buffer[6] = (unsigned long) buffer;
			aac_codec_buffer[8] = (unsigned long) aac_mix_buffer;
			aac_codec_buffer[7] = buffer_size;
			aac_codec_buffer[9] = aac_sample_per_frame * 4;

			res = sceAudiocodecDecode(aac_codec_buffer, 0x1003);

			if (res < 0) {
				if (buffer != NULL) {
					free(buffer);
				}

				__end();
				return -1;
			}

			if (buffer != NULL) {
				free(buffer);
			}

			samplesdecoded = aac_sample_per_frame;
			output = &g_buff[0];
			memcpy(output, aac_mix_buffer, samplesdecoded * 4);
			g_buff_frame_size = samplesdecoded;
			g_buff_frame_start = 0;

			if (g_force_up_sampling) {
				incr = (double) samplesdecoded / (g_info.sample_freq / 2);
			} else {
				incr = (double) samplesdecoded / g_info.sample_freq;
			}

			g_play_time += incr;
		}
	}

	return 0;
}

static int get_aac_track(MP4FileHandle file)
{
	int n_tracks = MP4GetNumberOfTracks(file, NULL, 0);
	int i = 0;

	for (i = 0; i < n_tracks; i++) {
		MP4TrackId track_id = MP4FindTrackId(file, i, NULL, 0);
		const char *track_type = MP4GetTrackType(file, track_id);

		if (!strcmp(track_type, MP4_AUDIO_TRACK_TYPE)) {
			//we found audio track !
			u_int8_t audio_type = MP4GetTrackAudioMpeg4Type(file, track_id);

			if (audio_type != 0)
				return track_id;
			else
				return -1;
		}
	}

	return -1;
}

static void m4a_get_tag()
{
	char *s = NULL;
	const char *name;
	u_int8_t *buffer;
	u_int32_t buffer_size;
	int i;

	g_info.tag.encode = conf_encode_utf8;

	if (MP4GetMetadataName(mp4file, &s)) {
		STRCPY_S(g_info.tag.title, s);
	}

	if (s != NULL) {
		free(s);
		s = NULL;
	}

	if (MP4GetMetadataArtist(mp4file, &s)) {
		STRCPY_S(g_info.tag.artist, s);
	}

	if (s != NULL) {
		free(s);
		s = NULL;
	}

	if (MP4GetMetadataAlbum(mp4file, &s)) {
		STRCPY_S(g_info.tag.album, s);
	}

	if (s != NULL) {
		free(s);
		s = NULL;
	}

	name = NULL;
	buffer = NULL;
	buffer_size = 0;
	i = 0;

	while (MP4GetMetadataByIndex(mp4file, i, &name, &buffer, &buffer_size)) {
		if (*(unsigned long *) name == 0x6f6f74a9) {
			memset(g_vendor_str, 0, sizeof(g_vendor_str));
			strncpy_s(g_vendor_str, sizeof(g_vendor_str), (const char *) buffer, buffer_size);
			dbg_printf(d, "%s: Encoder %s", __func__, g_vendor_str);
			break;
		}

		name = NULL;

		if (buffer != NULL) {
			free(buffer);
			buffer = NULL;
		}

		buffer_size = 0;
		++i;
	}

	if (buffer != NULL) {
		free(buffer);
		buffer = NULL;
	}
}

static int m4a_load(const char *spath, const char *lpath)
{
	int ret;
	int fd;
	NeAACDecHandle decoder;
	u_int8_t *buffer = NULL;
	u_int32_t buffer_size = 0;
	mp4AudioSpecificConfig cfg;
	uint64_t ms_duration;

	__init();

	fd = sceIoOpen(spath, PSP_O_RDONLY, 0777);

	if (fd < 0) {
		goto failed;
	}

	g_info.filesize = sceIoLseek(fd, 0, PSP_SEEK_END);

	sceIoClose(fd);

	mp4file = MP4Read(spath, 0);

	if (mp4file == NULL) {
		goto failed;
	}

	mp4track = get_aac_track(mp4file);

	if (mp4track < 0) {
		goto failed;
	}

	decoder = NeAACDecOpen();

	MP4GetTrackESConfiguration(mp4file, mp4track, &buffer, &buffer_size);

	if (buffer == NULL) {
		free(buffer);
		NeAACDecClose(decoder);
		goto failed;
	}

	memset(&cfg, 0, sizeof(cfg));

	if (NeAACDecAudioSpecificConfig(buffer, buffer_size, &cfg) < 0) {
		free(buffer);
		NeAACDecClose(decoder);
		goto failed;
	}

	if (NeAACDecInit2(decoder, buffer, buffer_size, (unsigned long *) &(g_info.sample_freq), (unsigned char *) &(g_info.channels)) < 0) {
		free(buffer);
		NeAACDecClose(decoder);
		goto failed;
	}

	g_force_up_sampling = cfg.forceUpSampling;
	dbg_printf(d, "forceUpSampling is %s", g_force_up_sampling ? "true" : "false");

	free(buffer);
	NeAACDecClose(decoder);

	ms_duration = MP4ConvertFromTrackDuration(mp4file, mp4track, MP4GetTrackDuration(mp4file, mp4track), MP4_MSECS_TIME_SCALE);

	g_info.duration = ms_duration / 1000.0;

	if (g_info.channels == 0 || g_info.channels > 2) {
		goto failed;
	}

	m4a_get_tag();

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

	if (g_force_up_sampling) {
		aac_codec_buffer[10] = g_info.sample_freq / 2;
	} else {
		aac_codec_buffer[10] = g_info.sample_freq;
	}

	if (sceAudiocodecInit(aac_codec_buffer, 0x1003) < 0) {
		goto failed;
	}

	g_info.avg_bps = MP4GetTrackBitRate(mp4file, mp4track);

	ret = xAudioInit();

	if (ret < 0) {
		goto failed;
	}

	if (g_force_up_sampling) {
		ret = xAudioSetFrequency(g_info.sample_freq / 2);
	} else {
		ret = xAudioSetFrequency(g_info.sample_freq);
	}

	if (ret < 0) {
		goto failed;
	}

	g_buff = xAudioAlloc(0, BUFF_SIZE);

	if (g_buff == NULL) {
		goto failed;
	}

	xAudioSetChannelCallback(0, m4a_audiocallback, NULL);

	return 0;

  failed:
	__end();
	return -1;
}

static int m4a_end(void)
{
	dbg_printf(d, "%s", __func__);

	__end();

	xAudioEnd();

	generic_set_status(ST_STOPPED);
	generic_end();

	if (mp4file != NULL) {
		MP4Close(mp4file);
		mp4file = NULL;
	}

	if (aac_getEDRAM) {
		sceAudiocodecReleaseEDRAM(aac_codec_buffer);
		aac_getEDRAM = false;
	}

	if (g_buff != NULL) {
		xAudioFree(g_buff);
		g_buff = NULL;
	}

	return 0;
}

static int m4a_get_info(struct music_info *info)
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
		if (config.show_encoder_msg) {
			STRCPY_S(info->encode_msg, g_vendor_str);
		} else {
			info->encode_msg[0] = '\0';
		}
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
static int m4a_probe(const char *spath)
{
	const char *p;

	p = utils_fileext(spath);

	if (p) {
		if (stricmp(p, "mp4") == 0) {
			return 1;
		}
		if (stricmp(p, "m4a") == 0) {
			return 1;
		}
	}

	return 0;
}

/**
 * PSP准备休眠时m4a的操作
 *
 * @return 成功时返回0
 */
static int m4a_suspend(void)
{
	generic_suspend();
	m4a_end();

	return 0;
}

/**
 * PSP准备从休眠时恢复的m4a的操作
 *
 * @param spath 当前播放音乐名，8.3路径形式
 * @param lpath 当前播放音乐名，长文件或形式
 *
 * @return 成功时返回0
 */
static int m4a_resume(const char *spath, const char *lpath)
{
	int ret;

	ret = m4a_load(spath, lpath);

	if (ret != 0) {
		dbg_printf(d, "%s: m4a_load failed %d", __func__, ret);
		return -1;
	}

	g_play_time = g_suspend_playing_time;
	m4a_seek_seconds(g_play_time);
	g_suspend_playing_time = 0;

	generic_resume(spath, lpath);

	return 0;
}

static int m4a_set_opt(const char *unused, const char *values)
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
		if (!strncasecmp(argv[i], "m4a_buffer_size", sizeof("m4a_buffer_size") - 1)) {
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

static struct music_ops m4a_ops = {
	.name = "m4a",
	.set_opt = m4a_set_opt,
	.load = m4a_load,
	.play = NULL,
	.pause = NULL,
	.end = m4a_end,
	.get_status = NULL,
	.fforward = NULL,
	.fbackward = NULL,
	.suspend = m4a_suspend,
	.resume = m4a_resume,
	.get_info = m4a_get_info,
	.probe = m4a_probe,
	.next = NULL,
};

int m4a_init()
{
	return register_musicdrv(&m4a_ops);
}

#endif
