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
#include <psprtc.h>
#include "config.h"
#include "scene.h"
#include "xaudiolib.h"
#include "musicmgr.h"
#include "musicdrv.h"
#include "FLAC/stream_decoder.h"
#include "FLAC/metadata.h"
#include "strsafe.h"
#include "common/utils.h"
#include "dbg.h"
#include "ssv.h"
#include "genericplayer.h"
#include "musicinfo.h"
#include "simple_gettext.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

#ifdef ENABLE_FLAC

#define NUM_AUDIO_SAMPLES (1024 * 8)

static int __end(void);

/**
 * Flac音乐播放缓冲
 */
static uint16_t *g_buff = NULL;

/**
 * Flac音乐播放缓冲总大小, 以帧数计
 */
static unsigned g_buff_size;

/**
 * Flac音乐播放缓冲大小，以帧数计
 */
static unsigned g_buff_frame_size;

/**
 * Flac音乐播放缓冲当前位置，以帧数计
 */
static int g_buff_frame_start;

/**
 * Flac音乐每样本位数
 */
static int g_flac_bits_per_sample = 0;

/**
 * 一次Flac解码操作已解码帧样本数
 */
static int g_decoded_sample_size = 0;

/**
 * Flac编码器名字
 */
static char g_encode_name[80];

/**
 * FLAC解码器
 */
static FLAC__StreamDecoder *g_decoder = NULL;

static FLAC__uint64 decode_position = 0;
static FLAC__uint64 last_decode_position = 0;

static buffered_reader_t *flacfile = NULL;

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

static int flac_seek_seconds(double seconds)
{
	FLAC__StreamDecoderState state;

	if (g_info.duration == 0)
		return -1;

	if (seconds >= g_info.duration) {
		__end();
		return 0;
	}

	if (seconds < 0)
		seconds = 0;

	free_bitrate(&g_inst_br);

	if (!FLAC__stream_decoder_seek_absolute(g_decoder, g_info.samples * seconds / g_info.duration)) {
		return -1;
	}

	state = FLAC__stream_decoder_get_state(g_decoder);

	if (state == FLAC__STREAM_DECODER_SEEK_ERROR) {
		dbg_printf(d, "Reflush the stream");
		FLAC__stream_decoder_flush(g_decoder);
	}

	g_play_time = seconds;

	if (!FLAC__stream_decoder_get_decode_position(g_decoder, &decode_position))
		decode_position = 0;

	last_decode_position = decode_position;

	return 0;
}

static FLAC__StreamDecoderReadStatus flac_read_cb(const FLAC__StreamDecoder * decoder, FLAC__byte buffer[], size_t * bytes, void *client_data)
{
	buffered_reader_t *pb = (buffered_reader_t *) client_data;

	if (*bytes > 0) {
		*bytes = buffered_reader_read(pb, buffer, sizeof(FLAC__byte) * (*bytes));

		if (*bytes == 0) {
			return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
		}
	}

	return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

static FLAC__StreamDecoderSeekStatus flac_seek_cb(const FLAC__StreamDecoder * decoder, FLAC__uint64 absolute_byte_offset, void *client_data)
{
	buffered_reader_t *pb = (buffered_reader_t *) client_data;

	buffered_reader_seek(pb, absolute_byte_offset);

	return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
}

static FLAC__StreamDecoderTellStatus flac_tell_cb(const FLAC__StreamDecoder * decoder, FLAC__uint64 * absolute_byte_offset, void *client_data)
{
	buffered_reader_t *pb = (buffered_reader_t *) client_data;

	*absolute_byte_offset = buffered_reader_position(pb);

	return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

static FLAC__StreamDecoderLengthStatus flac_length_cb(const FLAC__StreamDecoder * decoder, FLAC__uint64 * stream_length, void *client_data)
{
	buffered_reader_t *pb = (buffered_reader_t *) client_data;

	*stream_length = (FLAC__uint64) buffered_reader_length(pb);
	return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}

static FLAC__bool flac_eof_cb(const FLAC__StreamDecoder * decoder, void *client_data)
{
	buffered_reader_t *pb = (buffered_reader_t *) client_data;

	return buffered_reader_length(pb) == buffered_reader_position(pb);
}

static FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder *
													 decoder, const FLAC__Frame * frame, const FLAC__int32 * const buffer[], void *client_data)
{
	size_t i;
	uint16_t *output;

	(void) decoder;

	if (g_info.samples == 0) {
		dbg_printf(d, "ERROR: this example only works for FLAC files that have a g_info.samples count in STREAMINFO");
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}
	if (g_info.channels != 2 || g_flac_bits_per_sample != 16) {
		dbg_printf(d, "ERROR: this example only supports 16bit stereo streams");
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}

	g_decoded_sample_size = frame->header.blocksize;

	if (frame->header.blocksize > g_buff_size) {
		g_buff_size = frame->header.blocksize;
		g_buff = safe_realloc(g_buff, g_buff_size * frame->header.channels * sizeof(*g_buff));

		if (g_buff == NULL)
			return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}

	output = (uint16_t *) g_buff;

	if (frame->header.channels == 2) {
		for (i = 0; i < frame->header.blocksize; i++) {
			*output++ = (FLAC__int16) buffer[0][i];
			*output++ = (FLAC__int16) buffer[1][i];
		}
	} else if (frame->header.channels == 1) {
		for (i = 0; i < frame->header.blocksize; i++) {
			*output++ = (FLAC__int16) buffer[0][i];
			*output++ = (FLAC__int16) buffer[0][i];
		}
	}

	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static void metadata_callback(const FLAC__StreamDecoder * decoder, const FLAC__StreamMetadata * metadata, void *client_data)
{
	(void) decoder, (void) client_data;

	/* print some stats */
	if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
		/* save for later */
		g_info.samples = metadata->data.stream_info.total_samples;
		g_info.sample_freq = metadata->data.stream_info.sample_rate;
		g_info.channels = metadata->data.stream_info.channels;
		g_flac_bits_per_sample = metadata->data.stream_info.bits_per_sample;

		if (g_info.samples > 0) {
			g_info.duration = 1.0 * g_info.samples / g_info.sample_freq;
			g_info.avg_bps = g_info.filesize * 8 / g_info.duration;
		} else {
			g_info.duration = 0;
			g_info.avg_bps = 0;
		}

	}
}

static void error_callback(const FLAC__StreamDecoder * decoder, FLAC__StreamDecoderErrorStatus status, void *client_data)
{
	(void) decoder, (void) client_data;

	dbg_printf(d, "Got error callback: %s", FLAC__StreamDecoderErrorStatusString[status]);
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

				if (flac_seek_seconds(g_play_time) < 0) {
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

				if (flac_seek_seconds(g_play_time) < 0) {
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

/**
 * Flac音乐播放回调函数，
 * 负责将解码数据填充声音缓存区
 *
 * @note 声音缓存区的格式为双声道，16位低字序
 *
 * @param buf 声音缓冲区指针
 * @param reqn 缓冲区帧大小
 * @param pdata 用户数据，无用
 */
static int flac_audiocallback(void *buf, unsigned int reqn, void *pdata)
{
	int avail_frame;
	int snd_buf_frame_size = (int) reqn;
	double incr;
	signed short *audio_buf = buf;

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
			send_to_sndbuf(audio_buf, &g_buff[g_buff_frame_start * g_info.channels], snd_buf_frame_size, g_info.channels);
			g_buff_frame_start += snd_buf_frame_size;
			audio_buf += snd_buf_frame_size * 2;
			snd_buf_frame_size = 0;
		} else {
			FLAC__StreamDecoderState state;

			send_to_sndbuf(audio_buf, &g_buff[g_buff_frame_start * g_info.channels], avail_frame, g_info.channels);
			snd_buf_frame_size -= avail_frame;
			audio_buf += avail_frame * 2;

			if (!FLAC__stream_decoder_process_single(g_decoder)) {
				__end();
				return -1;
			}

			state = FLAC__stream_decoder_get_state(g_decoder);

			if (state == FLAC__STREAM_DECODER_END_OF_STREAM || state == FLAC__STREAM_DECODER_ABORTED || state == FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR) {
				__end();
				return -1;
			}

			g_buff_frame_size = g_decoded_sample_size;
			g_buff_frame_start = 0;

			incr = 1.0 * g_buff_frame_size / g_info.sample_freq;

			FLAC__stream_decoder_get_bits_per_sample(g_decoder);
			g_play_time += incr;

			/* Count the bitrate */

			if (!FLAC__stream_decoder_get_decode_position(g_decoder, &decode_position))
				decode_position = 0;

			if (decode_position > last_decode_position) {
				int bitrate;
				int bytes_per_sec = g_flac_bits_per_sample / 8 * g_info.sample_freq * g_info.channels;

				bitrate = (decode_position - last_decode_position) * 8.0 / (g_buff_frame_size * 4 / (float) bytes_per_sec);
				add_bitrate(&g_inst_br, bitrate, incr);
			}

			last_decode_position = decode_position;
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
 * 装载Flac音乐文件 
 *
 * @param spath 短路径名
 * @param lpath 长路径名
 *
 * @return 成功时返回0
 */
static int flac_load(const char *spath, const char *lpath)
{
	int fd;
	FLAC__StreamDecoderInitStatus init_status;
	FLAC__StreamMetadata *ptag;

	__init();

	if (g_buff != NULL) {
		free(g_buff);
		g_buff = NULL;
	}

	g_buff_size = NUM_AUDIO_SAMPLES / 2;
	g_buff = calloc(g_buff_size * 2, sizeof(*g_buff));

	if (g_buff == NULL) {
		__end();
		return -1;
	}

	fd = sceIoOpen(spath, PSP_O_RDONLY, 0777);

	if (fd < 0) {
		__end();
		return -1;
	}

	g_info.filesize = sceIoLseek(fd, 0, PSP_SEEK_END);
	sceIoClose(fd);

	if ((g_decoder = FLAC__stream_decoder_new()) == NULL) {
		__end();
		return -1;
	}

	if (FLAC__metadata_get_tags(spath, &ptag)) {
		if (ptag->type == FLAC__METADATA_TYPE_VORBIS_COMMENT) {
			const char *p = (const char *) ptag->data.vorbis_comment.vendor_string.entry;
			FLAC__uint32 i;

			p = strstr(p, "reference ");

			if (p == NULL)
				p = (const char *) ptag->data.vorbis_comment.vendor_string.entry;
			else
				p += sizeof("reference ") - 1;

			STRCPY_S(g_encode_name, (const char *) p);

			for (i = 0; i < ptag->data.vorbis_comment.num_comments; ++i) {
				const char *p = (const char *) ptag->data.vorbis_comment.comments[i].entry;
				const char *q;

				if (!strnicmp(p, "ALBUM=", sizeof("ALBUM=") - 1)) {
					q = p + sizeof("ALBUM=") - 1;
					STRCPY_S(g_info.tag.album, q);
				} else if (!strnicmp(p, "ARTIST=", sizeof("ARTIST=") - 1)) {
					q = p + sizeof("ARTIST=") - 1;
					STRCPY_S(g_info.tag.artist, q);
				} else if (!strnicmp(p, "PERFORMER=", sizeof("PERFORMER=") - 1)) {
					q = p + sizeof("ARTIST=") - 1;
					STRCPY_S(g_info.tag.artist, q);
				} else if (!strnicmp(p, "TITLE=", sizeof("TITLE=") - 1)) {
					q = p + sizeof("TITLE=") - 1;
					STRCPY_S(g_info.tag.title, q);
				}
			}

			g_info.tag.encode = conf_encode_utf8;
		}

		FLAC__metadata_object_delete(ptag);
	}
	// for speed issue
	(void) FLAC__stream_decoder_set_md5_checking(g_decoder, false);

	flacfile = buffered_reader_open(spath, g_io_buffer_size, 0);

	if (flacfile == NULL) {
		__end();
		return -1;
	}

	init_status =
		FLAC__stream_decoder_init_stream(g_decoder, flac_read_cb, flac_seek_cb,
										 flac_tell_cb, flac_length_cb, flac_eof_cb, write_callback, metadata_callback, error_callback, flacfile);

	if (init_status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
		__end();
		return -1;
	}

	if (!FLAC__stream_decoder_process_until_end_of_metadata(g_decoder)) {
		__end();
		return -1;
	}

	if (g_info.channels != 1 && g_info.channels != 2) {
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

	dbg_printf(d,
			   "[%d channel(s), %d Hz, %.2f kbps, %02d:%02d, encoder: %s, Ratio: %.3f]",
			   g_info.channels, g_info.sample_freq, g_info.avg_bps / 1000,
			   (int) (g_info.duration / 60), (int) g_info.duration % 60,
			   g_encode_name, 1.0 * g_info.filesize / (g_info.samples * g_info.channels * (g_flac_bits_per_sample / 8))
		);

	dbg_printf(d, "[%s - %s - %s, flac tag]", g_info.tag.artist, g_info.tag.album, g_info.tag.title);

	xAudioSetChannelCallback(0, flac_audiocallback, NULL);

	return 0;
}

/**
 * 停止Flac音乐文件的播放，销毁资源等
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
 * 停止Flac音乐文件的播放，销毁所占有的线程、资源等
 *
 * @note 不可以在播放线程中调用，必须能够多次重复调用而不死机
 *
 * @return 成功时返回0
 */
static int flac_end(void)
{
	__end();

	xAudioEnd();

	if (g_buff != NULL) {
		free(g_buff);
		g_buff = NULL;
	}

	generic_set_status(ST_STOPPED);

	if (g_decoder) {
		FLAC__stream_decoder_delete(g_decoder);
		g_decoder = NULL;
	}

	if (flacfile != NULL) {
		buffered_reader_close(flacfile);
		flacfile = NULL;
	}

	free_bitrate(&g_inst_br);
	generic_end();

	return 0;
}

/**
 * PSP准备休眠时Flac的操作
 *
 * @return 成功时返回0
 */
static int flac_suspend(void)
{
	generic_suspend();
	flac_end();

	return 0;
}

/**
 * PSP准备从休眠时恢复的Flac的操作
 *
 * @param spath 当前播放音乐名，8.3路径形式
 * @param lpath 当前播放音乐名，长文件或形式
 *
 * @return 成功时返回0
 */
static int flac_resume(const char *spath, const char *lpath)
{
	int ret;

	ret = flac_load(spath, lpath);
	if (ret != 0) {
		dbg_printf(d, "%s: flac_load failed %d", __func__, ret);
		return -1;
	}

	g_play_time = g_suspend_playing_time;
	flac_seek_seconds(g_play_time);
	g_suspend_playing_time = 0;

	generic_resume(spath, lpath);

	return 0;
}

/**
 * 得到Flac音乐文件相关信息
 *
 * @param pinfo 信息结构体指针
 *
 * @return
 */
static int flac_get_info(struct music_info *pinfo)
{
	if (pinfo->type & MD_GET_CURTIME) {
		pinfo->cur_time = g_play_time;
	}
	if (pinfo->type & MD_GET_CPUFREQ) {
		pinfo->psp_freq[0] = 66;
		pinfo->psp_freq[1] = 111;
	}
	if (pinfo->type & MD_GET_INSKBPS) {
		pinfo->ins_kbps = get_inst_bitrate(&g_inst_br) / 1000;
	}
	if (pinfo->type & MD_GET_DECODERNAME) {
		STRCPY_S(pinfo->decoder_name, "flac");
	}
	if (pinfo->type & MD_GET_ENCODEMSG) {
		if (config.show_encoder_msg && g_status != ST_UNKNOWN) {
			SPRINTF_S(pinfo->encode_msg, "%s %s: %.3f", g_encode_name,
					  _("压缩率"), 1.0 * g_info.filesize / (g_info.samples * g_info.channels * (g_flac_bits_per_sample / 8)));
		} else {
			pinfo->encode_msg[0] = '\0';
		}
	}

	return generic_get_info(pinfo);
}

/**
 * 检测是否为FLAC文件，目前只检查文件后缀名
 *
 * @param spath 当前播放音乐名，8.3路径形式
 *
 * @return 是FLAC文件返回1，否则返回0
 */
static int flac_probe(const char *spath)
{
	const char *p;

	p = utils_fileext(spath);

	if (p) {
		if (stricmp(p, "flac") == 0) {
			return 1;
		}
	}

	return 0;
}

static int flac_set_opt(const char *unused, const char *values)
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
		if (!strncasecmp(argv[i], "flac_buffer_size", sizeof("flac_buffer_size") - 1)) {
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

	if (config.use_vaudio)
		g_io_buffer_size = BUFFERED_READER_BUFFER_SIZE / 2;

	clean_args(argc, argv);

	return 0;
}

static struct music_ops flac_ops = {
	.name = "flac",
	.set_opt = flac_set_opt,
	.load = flac_load,
	.play = NULL,
	.pause = NULL,
	.end = flac_end,
	.get_status = NULL,
	.fforward = NULL,
	.fbackward = NULL,
	.suspend = flac_suspend,
	.resume = flac_resume,
	.get_info = flac_get_info,
	.probe = flac_probe,
	.next = NULL
};

int flac_init(void)
{
	return register_musicdrv(&flac_ops);
}

// Dummy function for libFLAC

int chown(const char *path, int owner, int group)
{
	return 0;
}

int utime(const char *filename, const void *buf)
{
	return 0;
}

#endif
