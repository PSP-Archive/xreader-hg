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
#include <malloc.h>
#include <unistd.h>
#include <assert.h>
#include <math.h>
#include <pspaudiocodec.h>
#include "config.h"
#include "ssv.h"
#include "scene.h"
#include "xaudiolib.h"
#include "musicmgr.h"
#include "musicdrv.h"
#include "strsafe.h"
#include "common/utils.h"
#include "genericplayer.h"
#include "musicinfo.h"
#include "dbg.h"
#include "pspasfparser.h"
#include "kubridge.h"
#include "mediaengine.h"
#include "cooleyesBridge.h"
#include "buffered_reader.h"
#include "scene.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

#ifdef ENABLE_WMA

#define WMA_MAX_BUF_SIZE 12288

struct WMAStdTag
{
	char *title;
	char *artist;
	char *copyright;
	char *comment;
	char *unknown;
};

struct WMAExTag
{
	u32 tag_size;
	char **key;
	short *key_size;
	char **value;
	short *value_size;
	short *flag;
};

/**
 * WMA音乐播放缓冲
 */
static uint16_t *g_buff = NULL;

/**
 * WMA音乐播放缓冲总大小, 以帧数计
 */
static unsigned g_buff_size;

/**
 * WMA音乐播放缓冲大小，以帧数计
 */
static unsigned g_buff_frame_size;

/**
 * WMA音乐播放缓冲当前位置，以帧数计
 */
static int g_buff_frame_start;

static SceAsfParser asfparser __attribute__ ((aligned(64)));

static unsigned long wma_codec_buffer[65] __attribute__ ((aligned(64)));

static short wma_mix_buffer[8192] __attribute__ ((aligned(64)));
static void *wma_frame_buffer;

static u8 wma_getEDRAM = 0;
static u16 wma_format_tag = 0x0161;
static u32 wma_avg_bytes_per_sec = 3998;
static u16 wma_block_align = 1485;
static u16 wma_bits_per_sample = 16;
static u16 wma_flag = 0x1F;
static u8 *codecdata;

static int64_t asf_read_cb(void *userdata, void *buf, SceSize size)
{
	int ret;
	reader_data *reader = (reader_data *) userdata;

	if (reader->use_buffer) {
		ret = buffered_reader_read(reader->r, buf, size);
	} else {
		ret = sceIoRead(reader->fd, buf, size);
	}

//  dbg_printf(d, "%s 0x%08x %d", __func__, (u32)buf, size);

	return ret;
}

static int64_t asf_seek_cb(void *userdata, void *buf, SceOff offset, int whence)
{
	int ret = -1;
	reader_data *reader = (reader_data *) userdata;

	if (reader->use_buffer) {
		if (whence == PSP_SEEK_SET) {
			ret = buffered_reader_seek(reader->r, offset);
		} else if (whence == PSP_SEEK_CUR) {
			ret = buffered_reader_seek(reader->r, offset + buffered_reader_position(reader->r));
		} else if (whence == PSP_SEEK_END) {
			ret = buffered_reader_seek(reader->r, buffered_reader_length(reader->r) - offset);
		}
	} else {
		ret = sceIoLseek(reader->fd, offset, whence);
	}

//  dbg_printf(d, "%s@0x%08x 0x%08x %d", __func__, ret, offset, whence);

	return ret;
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

static int wma_seek_seconds(SceAsfParser * parser, double npt)
{
	u32 ms = (u32) (npt * 1000L);
	int ret;

	ret = sceAsfSeekTime(parser, 1, &ms);

	if (ret < 0) {
		dbg_printf(d, "sceAsfSeekTime = 0x%08x", ret);
		return ret;
	}

	return 0;
}

/**
 * 解码WMA
 */
static int wma_process_single(void)
{
	int ret;
	SceAsfFrame *frame;

	memset(wma_frame_buffer, 0, wma_block_align);
	frame = (SceAsfFrame *) (((u8 *) & asfparser) + 14536 - 32);
	frame->pData = wma_frame_buffer;
	ret = sceAsfGetFrameData(&asfparser, 1, frame);

	if (ret < 0) {
		dbg_printf(d, "sceAsfGetFrameData = 0x%08x", ret);
		return -1;
	}

	memset(wma_mix_buffer, 0, 16384);

	wma_codec_buffer[6] = (unsigned long) wma_frame_buffer;
	wma_codec_buffer[8] = (unsigned long) wma_mix_buffer;
	wma_codec_buffer[9] = 16384;

	wma_codec_buffer[15] = frame->iUnk1;
	wma_codec_buffer[16] = frame->iUnk2;
	wma_codec_buffer[17] = frame->iUnk4;
	wma_codec_buffer[18] = frame->iUnk5;
	wma_codec_buffer[19] = frame->iUnk3;

	ret = sceAudiocodecDecode(wma_codec_buffer, 0x1005);

	if (ret < 0) {
		dbg_printf(d, "sceAudiocodecDecode = 0x%08x", ret);
		return -1;
	}

	memcpy(g_buff, wma_mix_buffer, wma_codec_buffer[9]);

	return wma_codec_buffer[9] / 4;
}

/**
 * 停止WMA音乐文件的播放，销毁资源等
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
 * WMA音乐播放回调函数，
 * 负责将解码数据填充声音缓存区
 *
 * @note 声音缓存区的格式为双声道，16位低字序
 *
 * @param buf 声音缓冲区指针
 * @param reqn 缓冲区帧大小
 * @param pdata 用户数据，无用
 */
static int wma_audiocallback(void *buf, unsigned int reqn, void *pdata)
{
	int avail_frame;
	int snd_buf_frame_size = (int) reqn;
	int ret;
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
			free_bitrate(&g_inst_br);
			wma_seek_seconds(&asfparser, g_play_time);
			g_buff_frame_size = g_buff_frame_start = 0;
		} else if (g_status == ST_FBACKWARD) {
			g_play_time -= g_seek_seconds;
			if (g_play_time < 0.) {
				g_play_time = 0.;
			}
			generic_lock();
			generic_set_status(ST_PLAYING);
			generic_set_playback(true);
			generic_unlock();
			free_bitrate(&g_inst_br);
			wma_seek_seconds(&asfparser, g_play_time);
			g_buff_frame_size = g_buff_frame_start = 0;
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
			send_to_sndbuf(audio_buf, &g_buff[g_buff_frame_start * 2], avail_frame, 2);
			snd_buf_frame_size -= avail_frame;
			audio_buf += avail_frame * 2;

			ret = wma_process_single();

			if (ret == -1) {
				__end();
				return -1;
			}

			g_buff_frame_size = ret;
			g_buff_frame_start = 0;

			g_play_time += ((double) ret) / g_info.sample_freq;
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
//  decoder = NULL;
	g_buff = NULL;
	g_buff_size = 0;

	codecdata = NULL;

	memset(wma_mix_buffer, 0, sizeof(wma_mix_buffer));
	memset(&data, 0, sizeof(data));
	data.fd = -1;

	return 0;
}

static int parse_standard_tag(struct WMAStdTag *tag)
{
	u32 ms = 0;
	int ret;
	u64 start;
	u64 size;
	u8 *framedata, *p;
	short tagsize[5];

	memset(tag, 0, sizeof(*tag));

	ret = sceAsfParser_685E0DA7(&asfparser, &ms, 0x00004000, NULL, &start, &size);

	if (ret < 0) {
		dbg_printf(d, "%s: sceAsfParser_685E0DA7@0x%08x", __func__, ret);
		return ret;
	}

	dbg_printf(d, "%s: standard arg @0x%08x size %u", __func__, (u32) start, (u32) size);

	framedata = malloc(size);

	if (framedata == NULL) {
		dbg_printf(d, "%s: out of memory", __func__);
		return -1;
	}

	asf_seek_cb(&data, NULL, start, PSP_SEEK_SET);
	asf_read_cb(&data, framedata, size);

	if (size <= 16) {
		free(framedata);
		return -1;
	}

	p = framedata;

	// skip 24 bytes
	p += 24;

	if (size >= 16 + 8) {
		size = size - 16 - 8;
	} else {
		free(framedata);
		return 0;
	}

	if (size <= 10) {
		free(framedata);
		return 0;
	}

	memcpy(tagsize, p, sizeof(tagsize));
	p += sizeof(tagsize);

	if (size < sizeof(tagsize)) {
		free(framedata);
		return 0;
	} else {
		size -= sizeof(tagsize);
	}

	if (tagsize[0] != 0) {
		tag->title = malloc(tagsize[0]);
		memcpy(tag->title, p, tagsize[0]);
	}

	p += tagsize[0];

	if (size < tagsize[0]) {
		free(framedata);
		return 0;
	} else {
		size -= tagsize[0];
	}

	if (tagsize[1] != 0) {
		tag->artist = malloc(tagsize[1]);
		memcpy(tag->artist, p, tagsize[1]);
	}

	p += tagsize[1];

	if (size < tagsize[1]) {
		free(framedata);
		return 0;
	} else {
		size -= tagsize[1];
	}

	if (tagsize[2] != 0) {
		tag->copyright = malloc(tagsize[2]);
		memcpy(tag->copyright, p, tagsize[2]);
	}

	p += tagsize[2];

	if (size < tagsize[2]) {
		free(framedata);
		return 0;
	} else {
		size -= tagsize[2];
	}

	if (tagsize[3] != 0) {
		tag->comment = malloc(tagsize[3]);
		memcpy(tag->comment, p, tagsize[3]);
	}

	p += tagsize[3];

	if (size < tagsize[3]) {
		free(framedata);
		return 0;
	} else {
		size -= tagsize[3];
	}

	if (tagsize[4] != 0) {
		tag->unknown = malloc(tagsize[4]);
		memcpy(tag->unknown, p, tagsize[4]);
	}

	free(framedata);
	return 0;
}

static int parse_ex_tag(struct WMAExTag *tag)
{
	u32 ms = 0;
	int ret;
	u64 start;
	u64 size;
	u8 *framedata, *p;
	int i;

	tag->tag_size = 0;
	tag->key = NULL;
	tag->value = NULL;

	ret = sceAsfParser_685E0DA7(&asfparser, &ms, 0x00008000, NULL, &start, &size);

	if (ret < 0) {
		dbg_printf(d, "%s: sceAsfParser_685E0DA7@0x%08x", __func__, ret);
		return ret;
	}

	dbg_printf(d, "%s: ex arg @0x%08x size %u", __func__, (u32) start, (u32) size);

	framedata = calloc(1, size);

	if (framedata == NULL) {
		dbg_printf(d, "%s: out of memory", __func__);
		return -1;
	}

	asf_seek_cb(&data, NULL, start, PSP_SEEK_SET);
	asf_read_cb(&data, framedata, size);

	if (size <= 16) {
		free(framedata);
		return -1;
	}

	p = framedata;
	// skip 24 bytes
	p += 16 + 8;

	if (size >= 16 + 8) {
		size -= 16 + 8;
	} else {
		free(framedata);
		return 0;
	}

	tag->tag_size = *(u16 *) p;

	p += 2;

	if (size >= 2) {
		size -= 2;
	} else {
		free(framedata);
		return 0;
	}

	tag->key = calloc(tag->tag_size, sizeof(tag->key[0]));
	tag->value = calloc(tag->tag_size, sizeof(tag->value[0]));
	tag->flag = calloc(tag->tag_size, sizeof(tag->flag[0]));
	tag->key_size = calloc(tag->tag_size, sizeof(tag->key_size[0]));
	tag->value_size = calloc(tag->tag_size, sizeof(tag->value_size[0]));

	for (i = 0; i < tag->tag_size; ++i) {
		tag->key_size[i] = *(u16 *) p;

		p += 2;

		if (size >= 2) {
			size -= 2;
		} else {
			free(framedata);
			return 0;
		}

		tag->key[i] = malloc(tag->key_size[i]);
		memcpy(tag->key[i], p, tag->key_size[i]);

		p += tag->key_size[i];

		if (size >= tag->key_size[i]) {
			size -= tag->key_size[i];
		} else {
			free(framedata);
			return 0;
		}

		tag->flag[i] = *(u16 *) p;

		p += 2;

		if (size >= 2) {
			size -= 2;
		} else {
			free(framedata);
			return 0;
		}

		tag->value_size[i] = *(u16 *) p;

		p += 2;

		if (size >= 2) {
			size -= 2;
		} else {
			free(framedata);
			return 0;
		}

		tag->value[i] = malloc(tag->value_size[i]);
		memcpy(tag->value[i], p, tag->value_size[i]);

		p += tag->value_size[i];

		if (size >= tag->value_size[i]) {
			size -= tag->value_size[i];
		} else {
			free(framedata);
			return 0;
		}
	}

	free(framedata);
	return 0;
}

static void free_standard_tag(struct WMAStdTag *tag)
{
	if (tag->title) {
		free(tag->title);
		tag->title = NULL;
	}

	if (tag->artist) {
		free(tag->artist);
		tag->artist = NULL;
	}

	if (tag->copyright) {
		free(tag->copyright);
		tag->copyright = NULL;
	}

	if (tag->comment) {
		free(tag->comment);
		tag->comment = NULL;
	}

	if (tag->unknown) {
		free(tag->unknown);
		tag->unknown = NULL;
	}
}

static void free_ex_tag(struct WMAExTag *tag)
{
	int i;

	if (tag->key) {
		for (i = 0; i < tag->tag_size; ++i) {
			if (tag->key[i] != NULL) {
				free(tag->key[i]);
				tag->key[i] = NULL;
			}
		}

		free(tag->key);
		tag->key = NULL;
	}

	if (tag->value) {
		for (i = 0; i < tag->tag_size; ++i) {
			if (tag->value[i] != NULL) {
				free(tag->value[i]);
				tag->value[i] = NULL;
			}
		}

		free(tag->value);
		tag->value = NULL;
	}

	if (tag->key_size) {
		free(tag->key_size);
		tag->key_size = NULL;
	}

	if (tag->value_size) {
		free(tag->value_size);
		tag->value_size = NULL;
	}

	if (tag->flag) {
		free(tag->flag);
		tag->flag = NULL;
	}

	tag->tag_size = 0;
}

static int lookup_value(struct WMAExTag *tag, char *key)
{
	int i;

	size_t len = strlen(key);

	u16 *str = calloc(len, sizeof(str[0]));

	for (i = 0; i < len; ++i) {
		str[i] = *key++;
	}

	for (i = 0; i < tag->tag_size; ++i) {
		if (!memcmp(tag->key[i], str, len * sizeof(str[0]))) {
			free(str);
			return i;
		}
	}

	free(str);
	return -1;
}

static int ucslen(const char *str)
{
	const u16 *p = (const u16 *) str;
	int l;

	l = 0;

	while (*p != 0) {
		p++;
		l++;
	}

	return l;
}

static void get_wma_tag(void)
{
	struct WMAStdTag tag;
	struct WMAExTag ex_tag;
	int r;

	memset(&tag, 0, sizeof(tag));
	memset(&ex_tag, 0, sizeof(ex_tag));

	parse_standard_tag(&tag);
	parse_ex_tag(&ex_tag);

	g_info.tag.type = WMATAG;
	g_info.tag.encode = conf_encode_ucs;

	if (tag.title != NULL) {
		memcpy(g_info.tag.title, tag.title, min(2 * (ucslen(tag.title) + 1), sizeof(g_info.tag.title)));
	}

	if (tag.artist != NULL) {
		memcpy(g_info.tag.artist, tag.artist, min(2 * (ucslen(tag.artist) + 1), sizeof(g_info.tag.artist)));
	}

	if (tag.comment != NULL) {
		memcpy(g_info.tag.comment, tag.comment, min(2 * (ucslen(tag.comment) + 1), sizeof(g_info.tag.comment)));
	}

	r = lookup_value(&ex_tag, "WM/AlbumTitle");

	if (r >= 0) {
		memcpy(g_info.tag.album, ex_tag.value[r], min(ex_tag.value_size[r], sizeof(g_info.tag.album)));
	}

	free_standard_tag(&tag);
	free_ex_tag(&ex_tag);
}

/** 
 * Dump from Subroutine music_parser_2B003AD8(music_parser_module) in 5.00 firmware
 * Warning: different firmwares may have different values
 */
static void init_asf_codecdata1(SceAsfParser * parser)
{
	parser->iUnk0 = 0x01;
	parser->iUnk1 = 0x00;
	parser->iUnk2 = 0x000CC003;
	parser->iUnk3 = 0x00;
	parser->iUnk4 = 0x00000000;
	parser->iUnk5 = 0x00000000;
	parser->iUnk6 = 0x00000000;
	parser->iUnk7 = 0x00000000;
}

/** 
 * Dump from Subroutine music_parser_2B003AD8(music_parser_module) in 5.00 firmware
 * Warning: different firmwares may have different values
 */
static void init_asf_codecdata2(SceAsfParser * parser)
{
	parser->iUnk3644 = 0;
	parser->pNeedMemBuffer = (void *) codecdata;
}

/** 
 * Dump from Subroutine music_parser_2B003AD8(music_parser_module) in 5.00 firmware
 * Warning: different firmwares may have different values
 */
static int check_asf_codecdata(SceAsfParser * parser)
{
	if (parser->iNeedMem > 0x8000) {
		dbg_printf(d, "%s: check failed", __func__);
		return 0;
	}

	return 1;
}

static int init_audiocodec()
{
	int ret;
	u16 *p16;

	memset(wma_codec_buffer, 0, sizeof(wma_codec_buffer));
	wma_codec_buffer[5] = 1;
	ret = sceAudiocodecCheckNeedMem(wma_codec_buffer, 0x1005);

	if (ret < 0) {
		dbg_printf(d, "sceAudiocodecCheckNeedMem=0x%08X", ret);
		return -1;
	}

	ret = sceAudiocodecGetEDRAM(wma_codec_buffer, 0x1005);

	if (ret < 0) {
		dbg_printf(d, "sceAudiocodecGetEDRAM=0x%08X", ret);
		return -1;
	}

	wma_getEDRAM = 1;

	p16 = (u16 *) (&wma_codec_buffer[10]);

	p16[0] = wma_format_tag;
	p16[1] = g_info.channels;
	wma_codec_buffer[11] = g_info.sample_freq;
	wma_codec_buffer[12] = wma_avg_bytes_per_sec;
	p16 = (u16 *) (&wma_codec_buffer[13]);
	p16[0] = wma_block_align;
	p16[1] = wma_bits_per_sample;
	p16[2] = wma_flag;

	ret = sceAudiocodecInit(wma_codec_buffer, 0x1005);
	if (ret < 0) {
		dbg_printf(d, "sceAudiocodecInit=0x%08X", ret);
		return -1;
	}

	return 0;
}

static int wma_set_opt(const char *unused, const char *values)
{
	int argc, i;
	char **argv;

	dbg_printf(d, "%s: options are %s", __func__, values);

	build_args(values, &argc, &argv);

	for (i = 0; i < argc; ++i) {
		if (!strncasecmp(argv[i], "wma_buffer_size", sizeof("wma_buffer_size") - 1)) {
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

/**
 * 装载WMA音乐文件 
 *
 * @param spath 短路径名
 * @param lpath 长路径名
 *
 * @return 成功时返回0
 */
static int wma_load(const char *spath, const char *lpath)
{
	int ret;

	__init();

	g_buff_size = WMA_MAX_BUF_SIZE;
	g_buff = calloc_64(1, g_buff_size * sizeof(g_buff[0]));

	if (g_buff == NULL) {
		__end();
		return -1;
	}

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
		ret = load_me_prx(VAUDIO | AVCODEC | COOLEYEBRIDGE | ASFPARSER);
	else
		ret = load_me_prx(AVCODEC);

	if (ret < 0) {
		__end();
		return -1;
	}

	ret = cooleyesMeBootStart(psp_fw_version, 3);

	if (ret < 0) {
		dbg_printf(d, "cooleyesMeBootStart@0x%08x", ret);
		goto failed;
	}

	memset(&asfparser, 0, sizeof(asfparser));
	init_asf_codecdata1(&asfparser);

	ret = sceAsfCheckNeedMem(&asfparser);

	if (ret < 0) {
		dbg_printf(d, "sceAsfCheckNeedMem = 0x%08x", ret);
		goto failed;
	}

	if (check_asf_codecdata(&asfparser) == 0) {
		goto failed;
	}

	codecdata = memalign(64, asfparser.iNeedMem);
	memset(codecdata, 0, asfparser.iNeedMem);

	init_asf_codecdata2(&asfparser);

	ret = sceAsfInitParser(&asfparser, &data, &asf_read_cb, &asf_seek_cb);

	if (ret < 0) {
		dbg_printf(d, "sceAsfInitParser = 0x%08x", ret);
		goto failed;
	}

	dbg_printf(d, "sceAsfInitParser OK");

	g_info.channels = *((u16 *) codecdata);
	g_info.sample_freq = *((u32 *) (codecdata + 4));
	wma_format_tag = 0x0161;
	wma_avg_bytes_per_sec = *((u32 *) (codecdata + 8));
	wma_block_align = *((u16 *) (codecdata + 12));
	wma_bits_per_sample = 16;
	wma_flag = *((u16 *) (codecdata + 14));

	g_info.duration = (double) asfparser.iDuration / 10000 / 1000;

	dbg_printf(d, "Duration %f Channels %d, Samplerate %d Block %d", g_info.duration, g_info.channels, g_info.sample_freq, wma_block_align);

	wma_frame_buffer = memalign(64, wma_block_align);

	if (!wma_frame_buffer) {
		dbg_printf(d, "malloc_64(frame_data_buffer) fail");
		goto failed;
	}

	if (init_audiocodec() < 0) {
		goto failed;
	}

	if (g_info.channels > 2 || g_info.channels <= 0) {
		__end();
		return -1;
	}

	g_info.duration = (double) asfparser.iDuration / 10000 / 1000;

	if (g_info.duration != 0) {
		g_info.avg_bps = g_info.filesize * 8 / g_info.duration;
	} else {
		g_info.avg_bps = 0;
	}

	get_wma_tag();

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

	xAudioSetChannelCallback(0, wma_audiocallback, NULL);
	generic_set_status(ST_LOADED);

	return 0;

  failed:
	__end();
	return -1;
}

/**
 * 停止WMA音乐文件的播放，销毁所占有的线程、资源等
 *
 * @note 不可以在播放线程中调用，必须能够多次重复调用而不死机
 *
 * @return 成功时返回0
 */
static int wma_end(void)
{
	__end();

	xAudioEnd();

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

	if (wma_getEDRAM) {
		sceAudiocodecReleaseEDRAM(wma_codec_buffer);
		wma_getEDRAM = 0;
	}

	if (codecdata != NULL) {
		free(codecdata);
		codecdata = NULL;
	}

	if (wma_frame_buffer != NULL) {
		free(wma_frame_buffer);
		wma_frame_buffer = NULL;
	}

	if (g_buff != NULL) {
		free(g_buff);
		g_buff = NULL;
	}

	g_buff_size = 0;
	free_bitrate(&g_inst_br);
	cooleyesMeBootStart(psp_fw_version, 4);
	generic_end();

	return 0;
}

/**
 * PSP准备休眠时WMA的操作
 *
 * @return 成功时返回0
 */
static int wma_suspend(void)
{
	generic_suspend();
	wma_end();

	return 0;
}

/**
 * PSP准备从休眠时恢复的WMA的操作
 *
 * @param spath 当前播放音乐名，8.3路径形式
 * @param lpath 当前播放音乐名，长文件名形式
 *
 * @return 成功时返回0
 */
static int wma_resume(const char *spath, const char *lpath)
{
	int ret;

	ret = wma_load(spath, lpath);
	if (ret != 0) {
		dbg_printf(d, "%s: wma_load failed %d", __func__, ret);
		return -1;
	}

	g_play_time = g_suspend_playing_time;
	free_bitrate(&g_inst_br);
	wma_seek_seconds(&asfparser, g_play_time);
	g_suspend_playing_time = 0;

	generic_resume(spath, lpath);

	return 0;
}

/**
 * 得到WMA音乐文件相关信息
 *
 * @param pinfo 信息结构体指针
 *
 * @return
 */
static int wma_get_info(struct music_info *pinfo)
{
	if (pinfo->type & MD_GET_CURTIME) {
		pinfo->cur_time = g_play_time;
	}
	if (pinfo->type & MD_GET_CPUFREQ) {
		pinfo->psp_freq[0] = 49;
		pinfo->psp_freq[1] = 33;
	}
	if (pinfo->type & MD_GET_INSKBPS) {
		pinfo->ins_kbps = get_inst_bitrate(&g_inst_br) / 1000;
	}
	if (pinfo->type & MD_GET_DECODERNAME) {
		STRCPY_S(pinfo->decoder_name, "WMA");
	}
	if (pinfo->type & MD_GET_ENCODEMSG) {
		pinfo->encode_msg[0] = '\0';
	}

	return generic_get_info(pinfo);
}

/**
 * 检测是否为MPC文件，目前只检查文件后缀名
 *
 * @param spath 当前播放音乐名，8.3路径形式
 *
 * @return 是MPC文件返回1，否则返回0
 */
static int wma_probe(const char *spath)
{
	const char *p;

	p = utils_fileext(spath);

	if (p) {
		if (stricmp(p, "asf") == 0) {
			return 1;
		}
		if (stricmp(p, "wma") == 0) {
			return 1;
		}
		if (stricmp(p, "wmv") == 0) {
			return 1;
		}
	}

	return 0;
}

static struct music_ops wma_ops = {
	.name = "wma",
	.set_opt = wma_set_opt,
	.load = wma_load,
	.play = NULL,
	.pause = NULL,
	.end = wma_end,
	.get_status = NULL,
	.fforward = NULL,
	.fbackward = NULL,
	.suspend = wma_suspend,
	.resume = wma_resume,
	.get_info = wma_get_info,
	.probe = wma_probe,
	.next = NULL
};

int wmadrv_init(void)
{
	return register_musicdrv(&wma_ops);
}

#endif
