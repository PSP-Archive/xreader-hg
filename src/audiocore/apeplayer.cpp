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
#include "scene.h"
#include "xaudiolib.h"
#include "musicmgr.h"
#include "musicdrv.h"
#include "All.h"
#include "CharacterHelper.h"
#include "APEDecompress.h"
#include "APEInfo.h"
#include "CharacterHelper.h"
#include "strsafe.h"
#include "common/utils.h"
#include "genericplayer.h"
#include "musicinfo.h"
#include "dbg.h"
#include "ssv.h"
#include "config.h"
#include "simple_gettext.h"

#ifdef ENABLE_APE

#define BLOCKS_PER_DECODE (4096 / 4)
#define NUM_AUDIO_SAMPLES (BLOCKS_PER_DECODE * 4)

static int __end(void);

/**
 * APE音乐播放缓冲
 */
static short *g_buff = NULL;

/**
 * APE音乐播放缓冲大小，以帧数计
 */
static unsigned g_buff_frame_size;

/**
 * APE音乐播放缓冲当前位置，以帧数计
 */
static int g_buff_frame_start;

/**
 * APE音乐每样本位数
 */
static int g_ape_bits_per_sample = 0;

/**
 * APE编码器名字
 */
static char g_encode_name[80];

/**
 * APE解码器
 */
static CAPEDecompress *g_decoder = NULL;

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
static void send_to_sndbuf(void *buf, short *srcbuf, int frames, int channels)
{
	int n;
	signed short *p = (signed short *) buf;

	if (frames <= 0)
		return;

	for (n = 0; n < frames * channels; n++) {
		if (channels == 2)
			*p++ = srcbuf[n];
		else if (channels == 1) {
			*p++ = srcbuf[n];
			*p++ = srcbuf[n];
		}
	}
}

static int ape_seek_seconds(double seconds)
{
	uint32_t sample;

	if (g_info.duration == 0)
		return -1;

	if (seconds >= g_info.duration) {
		__end();
		return 0;
	}

	if (seconds < 0)
		seconds = 0;

	free_bitrate(&g_inst_br);
	sample = g_info.samples * seconds / g_info.duration;

	dbg_printf(d, "Seeking to sample %d.", sample);

	int ret = g_decoder->Seek(sample);

	if (ret == ERROR_SUCCESS) {
		g_play_time = seconds;

		return 0;
	}

	return -1;
}

/**
 * 处理快进快退
 *
 * @return
 * - -1 should exit
 * - 0 OK
 */
static int handle_seek(void)
{
	u64 timer_end;

	if (1) {

		if (g_status == ST_FFORWARD) {
			sceRtcGetCurrentTick(&timer_end);

			generic_lock();
			if (g_last_seek_is_forward) {
				generic_unlock();

				if (pspDiffTime(&timer_end, (u64 *) & g_last_seek_tick) <= 1.0) {
					generic_lock();

					if (g_seek_count > 0) {
						g_play_time += g_seek_seconds;
						g_seek_count--;
					}

					generic_unlock();

					if (g_play_time >= g_info.duration) {
						return -1;
					}

					sceKernelDelayThread(100000);
				} else {
					generic_lock();

					g_seek_count = 0;
					generic_set_playback(true);

					if (ape_seek_seconds(g_play_time) < 0) {
						generic_unlock();
						return -1;
					}

					generic_set_status(ST_PLAYING);

					generic_unlock();
					sceKernelDelayThread(100000);
				}
			} else {
				generic_unlock();
			}
		} else if (g_status == ST_FBACKWARD) {
			sceRtcGetCurrentTick(&timer_end);

			generic_lock();
			if (!g_last_seek_is_forward) {
				generic_unlock();

				if (pspDiffTime(&timer_end, (u64 *) & g_last_seek_tick) <= 1.0) {
					generic_lock();

					if (g_seek_count > 0) {
						g_play_time -= g_seek_seconds;
						g_seek_count--;
					}

					generic_unlock();

					if (g_play_time < 0) {
						g_play_time = 0;
					}

					sceKernelDelayThread(100000);
				} else {
					generic_lock();

					g_seek_count = 0;
					generic_set_playback(true);

					if (ape_seek_seconds(g_play_time) < 0) {
						generic_unlock();
						return -1;
					}

					generic_set_status(ST_PLAYING);

					generic_unlock();
					sceKernelDelayThread(100000);
				}
			} else {
				generic_unlock();
			}
		}

		return 0;
	}

	return 0;
}

/**
 * APE音乐播放回调函数，
 * 负责将解码数据填充声音缓存区
 *
 * @note 声音缓存区的格式为双声道，16位低字序
 *
 * @param buf 声音缓冲区指针
 * @param reqn 缓冲区帧大小
 * @param pdata 用户数据，无用
 */
static int ape_audiocallback(void *buf, unsigned int reqn, void *pdata)
{
	int avail_frame;
	int snd_buf_frame_size = (int) reqn;
	int ret;
	double incr;
	signed short *audio_buf = (signed short *) buf;

	UNUSED(pdata);

	if (g_status != ST_PLAYING) {
		if (handle_seek() == -1) {
			__end();
			return -1;
		}

		g_buff_frame_size = g_buff_frame_start = 0;
		xAudioClearSndBuf(buf, snd_buf_frame_size);
		return 0;
	}

	while (snd_buf_frame_size > 0) {
		avail_frame = g_buff_frame_size - g_buff_frame_start;

		if (avail_frame >= snd_buf_frame_size) {
			send_to_sndbuf(audio_buf,
						   &g_buff[g_buff_frame_start * g_info.channels],
						   snd_buf_frame_size, g_info.channels);
			g_buff_frame_start += snd_buf_frame_size;
			audio_buf += snd_buf_frame_size * 2;
			snd_buf_frame_size = 0;
		} else {
			send_to_sndbuf(audio_buf,
						   &g_buff[g_buff_frame_start * g_info.channels],
						   avail_frame, g_info.channels);
			snd_buf_frame_size -= avail_frame;
			audio_buf += avail_frame * 2;
			int block = -1;

			ret =
				g_decoder->GetData((char *) g_buff, BLOCKS_PER_DECODE, &block);

			if (ret != ERROR_SUCCESS || block == 0) {
				__end();
				return -1;
			}

			g_buff_frame_size = block;
			g_buff_frame_start = 0;

			incr = 1.0 * g_buff_frame_size / g_info.sample_freq;
			g_play_time += incr;

			add_bitrate(&g_inst_br,
						g_decoder->GetInfo(APE_DECOMPRESS_CURRENT_BITRATE) *
						1000, incr);
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
	memset(&g_info, 0, sizeof(g_info));
	g_decoder = NULL;
	g_encode_name[0] = '\0';

	return 0;
}

/**
 * 装载APE音乐文件 
 *
 * @param spath 短路径名
 * @param lpath 长路径名
 *
 * @return 成功时返回0
 */
static int ape_load(const char *spath, const char *lpath)
{
	__init();

	if (g_buff != NULL) {
		free(g_buff);
		g_buff = NULL;
	}

	g_buff = (short int *) calloc(NUM_AUDIO_SAMPLES, sizeof(*g_buff));

	if (g_buff == NULL) {
		__end();
		return -1;
	}

	CSmartPtr < str_utf16 > path(CAPECharacterHelper::GetUTF16FromANSI(spath));
	int err;
	CAPEInfo ape_info(&err, path);

	if (err == ERROR_SUCCESS) {
		g_info.sample_freq = ape_info.GetInfo(APE_INFO_SAMPLE_RATE);
		g_info.channels = ape_info.GetInfo(APE_INFO_CHANNELS);
		g_info.samples = ape_info.GetInfo(APE_INFO_TOTAL_BLOCKS);
		g_info.filesize = ape_info.GetInfo(APE_INFO_APE_TOTAL_BYTES);
		g_info.avg_bps = ape_info.GetInfo(APE_INFO_AVERAGE_BITRATE) * 1000;
		g_ape_bits_per_sample = ape_info.GetInfo(APE_INFO_BITS_PER_SAMPLE);

		if (g_info.samples > 0) {
			g_info.duration = 1.0 * g_info.samples / g_info.sample_freq;
		} else {
			g_info.duration = 0;
		}
	} else {
		__end();
		return -1;
	}

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

	generic_set_status(ST_LOADED);

	generic_readtag(&g_info, spath);

	dbg_printf(d,
			   "[%d channel(s), %d Hz, %.2f kbps, %02d:%02d, encoder: %s, Ratio: %.3f]",
			   g_info.channels, g_info.sample_freq, g_info.avg_bps / 1000,
			   (int) (g_info.duration / 60), (int) g_info.duration % 60,
			   g_encode_name,
			   1.0 * g_info.filesize / (g_info.samples * g_info.channels *
										(g_ape_bits_per_sample / 8))
		);

	dbg_printf(d, "[%s - %s - %s, tag type: %d]", g_info.tag.artist,
			   g_info.tag.album, g_info.tag.title, g_info.tag.type);

	g_decoder = (CAPEDecompress *) CreateIAPEDecompress(path, &err);

	if (err != ERROR_SUCCESS) {
		__end();
		return -1;
	}

	xAudioSetChannelCallback(0, ape_audiocallback, NULL);

	return 0;
}

/**
 * 停止APE音乐文件的播放，销毁资源等
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
 * 停止APE音乐文件的播放，销毁所占有的线程、资源等
 *
 * @note 不可以在播放线程中调用，必须能够多次重复调用而不死机
 *
 * @return 成功时返回0
 */
static int ape_end(void)
{
	__end();

	xAudioEnd();

	if (g_buff != NULL) {
		free(g_buff);
		g_buff = NULL;
	}

	generic_set_status(ST_STOPPED);

	if (g_decoder) {
		delete g_decoder;

		g_decoder = NULL;
	}

	free_bitrate(&g_inst_br);
	generic_end();

	return 0;
}

/**
 * PSP准备休眠时APE的操作
 *
 * @return 成功时返回0
 */
static int ape_suspend(void)
{
	generic_suspend();
	ape_end();

	return 0;
}

/**
 * PSP准备从休眠时恢复的APE的操作
 *
 * @param spath 当前播放音乐名，8.3路径形式
 * @param lpath 当前播放音乐名，长文件或形式
 *
 * @return 成功时返回0
 */
static int ape_resume(const char *spath, const char *lpath)
{
	int ret;

	ret = ape_load(spath, lpath);
	if (ret != 0) {
		dbg_printf(d, "%s: ape_load failed %d", __func__, ret);
		return -1;
	}

	g_play_time = g_suspend_playing_time;
	ape_seek_seconds(g_play_time);
	g_suspend_playing_time = 0;

	generic_resume(spath, lpath);

	return 0;
}

/**
 * 得到APE音乐文件相关信息
 *
 * @param pinfo 信息结构体指针
 *
 * @return
 */
static int ape_get_info(struct music_info *pinfo)
{
	if (pinfo->type & MD_GET_CURTIME) {
		pinfo->cur_time = g_play_time;
	}
	if (pinfo->type & MD_GET_CPUFREQ) {
		if (config.use_vaudio)
			pinfo->psp_freq[0] = 266;
		else
			pinfo->psp_freq[0] = 233;

		pinfo->psp_freq[1] = 116;
	}
	if (pinfo->type & MD_GET_INSKBPS) {
		pinfo->ins_kbps = get_inst_bitrate(&g_inst_br) / 1000;
	}
	if (pinfo->type & MD_GET_DECODERNAME) {
		STRCPY_S(pinfo->decoder_name, "ape");
	}
	if (pinfo->type & MD_GET_ENCODEMSG) {
		if (config.show_encoder_msg && g_status != ST_UNKNOWN) {
			SPRINTF_S(pinfo->encode_msg, "%s %s: %.3f", g_encode_name,
					  _("压缩率"),
					  1.0 * g_info.filesize / (g_info.samples *
											   g_info.channels *
											   (g_ape_bits_per_sample / 8)));
		} else {
			pinfo->encode_msg[0] = '\0';
		}
	}

	return generic_get_info(pinfo);
}

/**
 * 检测是否为APE文件，目前只检查文件后缀名
 *
 * @param spath 当前播放音乐名，8.3路径形式
 *
 * @return 是APE文件返回1，否则返回0
 */
static int ape_probe(const char *spath)
{
	const char *p;

	p = utils_fileext(spath);

	if (p) {
		if (stricmp(p, "ape") == 0) {
			return 1;
		}

		if (stricmp(p, "mac") == 0) {
			return 1;
		}
	}

	return 0;
}

static struct music_ops ape_ops = {
	"ape",
	NULL,
	ape_load,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	ape_get_info,
	ape_suspend,
	ape_resume,
	ape_end,
	ape_probe,
	NULL
};

extern "C" int ape_init(void)
{
	return register_musicdrv(&ape_ops);
}

#endif
