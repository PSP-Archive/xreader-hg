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
#include <mpc/mpcdec.h>
#include <assert.h>
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
#ifdef DMALLOC
#include "dmalloc.h"
#endif

#ifdef ENABLE_MPC

static int __end(void);

static mpc_reader reader;
static mpc_demux *demux;
static mpc_streaminfo info;

/**
 * Musepack���ֲ��Ż���
 */
static MPC_SAMPLE_FORMAT *g_buff = NULL;

/**
 * Musepack���ֲ��Ż����С����֡����
 */
static unsigned g_buff_sample_size;

/**
 * Musepack���ֲ��Ż��嵱ǰλ�ã���֡����
 */
static int g_buff_sample_start;

#ifdef MPC_FIXED_POINT
static int shift_signed(MPC_SAMPLE_FORMAT val, int shift)
{
	if (shift > 0)
		val <<= shift;
	else if (shift < 0)
		val >>= -shift;
	return (int) val;
}
#endif

/**
 * �������ݵ�����������
 *
 * @note �����������ĸ�ʽΪ˫������16λ������
 *
 * @param buf ����������ָ��
 * @param srcbuf �������ݻ�����ָ��
 * @param samples ����������
 * @param channels ������
 * 
 * @return ���ƽ�����ַ
 */
static u16 *copy_to_sndbuf(u16 *buf, MPC_SAMPLE_FORMAT * srcbuf, int frames, int channels)
{
	unsigned n;
	unsigned bps = 16;
	u16 *p = buf;
	int clip_min = -1 << (bps - 1), clip_max = (1 << (bps - 1)) - 1;
#ifndef MPC_FIXED_POINT
	int float_scale = 1 << (bps - 1);
#endif

	if (frames <= 0)
		return buf;

	for (n = 0; n < frames * channels; n++) {
		int val;

#ifdef MPC_FIXED_POINT
		val = shift_signed(srcbuf[n], bps - MPC_FIXED_POINT_SCALE_SHIFT);
#else
		val = (int) (srcbuf[n] * float_scale);
#endif
		if (val < clip_min)
			val = clip_min;
		else if (val > clip_max)
			val = clip_max;
		if (channels == 2)
			*p++ = val;
		else if (channels == 1) {
			*p++ = val;
			*p++ = val;
		}
	}

	return p;
}

static int handle_seek(void)
{
	if (g_status == ST_FFORWARD) {
		g_play_time += g_seek_seconds;
	} else if (g_status == ST_FBACKWARD) {
		g_play_time -= g_seek_seconds;
	}

	if(g_play_time < 0.0) {
		g_play_time = 0.0;
	}

	if(g_info.duration < g_play_time) {
		return -1;
	}

	if (g_status == ST_FFORWARD) {
		generic_set_status(ST_PLAYING);
		generic_set_playback(true);
	} else if (g_status == ST_FBACKWARD) {
		generic_set_status(ST_PLAYING);
		generic_set_playback(true);
	}

	free_bitrate(&g_inst_br);
	mpc_demux_seek_second(demux, g_play_time);
	g_buff_sample_size = g_buff_sample_start = 0;

	return 0;
}

/**
 * Musepack���ֲ��Żص�������
 * ���𽫽��������������������
 *
 * @note �����������ĸ�ʽΪ˫������16λ������
 *
 * @param buf ����������ָ��
 * @param reqn ������֡��С
 * @param pdata �û����ݣ�����
 */
static int mpc_audiocallback(void *buf, unsigned int snd_buf_sample_size, void *pdata)
{
	int avail_sample, copy_sample;
	int ret;
	double incr;
	u16 *audio_buf = buf;

	UNUSED(pdata);

	if (g_status != ST_PLAYING) {
		if (handle_seek() < 0) {
			__end();
			return -1;
		}

		xAudioClearSndBuf(buf, snd_buf_sample_size);
		return 0;
	}

	while (snd_buf_sample_size != 0) {
		avail_sample = g_buff_sample_size - g_buff_sample_start;
		copy_sample = min(avail_sample, snd_buf_sample_size);
		audio_buf = copy_to_sndbuf(audio_buf, &g_buff[g_buff_sample_start * g_info.channels], copy_sample, g_info.channels);
		g_buff_sample_start += copy_sample;
		snd_buf_sample_size -= copy_sample;
		incr = (double) (copy_sample) / g_info.sample_freq;
		g_play_time += incr;

		if(g_buff_sample_start == g_buff_sample_size) {
			mpc_frame_info frame;

			frame.buffer = (MPC_SAMPLE_FORMAT *) g_buff;
			ret = mpc_demux_decode(demux, &frame);

			if (frame.bits == -1) {
				__end();
				return -1;
			}

			g_buff_sample_size = frame.samples;
			g_buff_sample_start = 0;

			incr = (double) g_buff_sample_size / g_info.sample_freq;
			add_bitrate(&g_inst_br, frame.bits * g_info.sample_freq / MPC_FRAME_LENGTH, incr);
		}
	}

	return 0;
}

/**
 * ��ʼ������������Դ��
 *
 * @return �ɹ�ʱ����0
 */
static int __init(void)
{
	generic_init();

	generic_set_status(ST_UNKNOWN);

	g_buff_sample_size = g_buff_sample_start = 0;
	g_seek_seconds = 0;

	g_play_time = 0.;
	demux = NULL;

	memset(&g_info, 0, sizeof(g_info));

	return 0;
}

/// mpc_reader callback implementations
static mpc_int32_t read_buffered_reader(mpc_reader * p_reader, void *ptr, mpc_int32_t size)
{
	buffered_reader_t *p_breader = (buffered_reader_t *) p_reader->data;

	return (mpc_int32_t) buffered_reader_read(p_breader, ptr, size);
}

static mpc_bool_t seek_buffered_reader(mpc_reader * p_reader, mpc_int32_t offset)
{
	buffered_reader_t *p_breader = (buffered_reader_t *) p_reader->data;

	return buffered_reader_seek(p_breader, offset) == 0;
}

static mpc_int32_t tell_buffered_reader(mpc_reader * p_reader)
{
	buffered_reader_t *p_breader = (buffered_reader_t *) p_reader->data;

	return buffered_reader_position(p_breader);
}

static mpc_int32_t get_size_buffered_reader(mpc_reader * p_reader)
{
	buffered_reader_t *p_breader = (buffered_reader_t *) p_reader->data;

	return buffered_reader_length(p_breader);
}

static mpc_bool_t canseek_buffered_reader(mpc_reader * p_reader)
{
	return MPC_TRUE;
}

static mpc_status mpc_reader_init_buffered_reader(mpc_reader * p_reader, const char *spath)
{
	mpc_reader tmp_reader;
	buffered_reader_t *reader;
	int bufsize;

	bufsize = g_io_buffer_size;

	if (config.use_vaudio)
		bufsize = bufsize / 2;

	reader = buffered_reader_open(spath, max(bufsize, 8192), 1);

	if (reader == NULL)
		return MPC_STATUS_FILE;

	if(g_io_buffer_size == 0) {
		buffered_reader_enable_cache(reader, 0);
	}

	memset(&tmp_reader, 0, sizeof(tmp_reader));

	tmp_reader.data = reader;
	tmp_reader.canseek = canseek_buffered_reader;
	tmp_reader.get_size = get_size_buffered_reader;
	tmp_reader.read = read_buffered_reader;
	tmp_reader.seek = seek_buffered_reader;
	tmp_reader.tell = tell_buffered_reader;

	*p_reader = tmp_reader;
	return MPC_STATUS_OK;
}

static void mpc_reader_exit_buffered_reader(mpc_reader * p_reader)
{
	buffered_reader_t *p_breader = (buffered_reader_t *) p_reader->data;

	buffered_reader_close(p_breader);
	p_reader->data = NULL;
}

/**
 * װ��Musepack�����ļ� 
 *
 * @param spath ��·����
 * @param lpath ��·����
 *
 * @return �ɹ�ʱ����0
 */
static int mpc_load(const char *spath, const char *lpath)
{
	__init();

	if (mpc_reader_init_buffered_reader(&reader, spath) < 0) {
		__end();
		return -1;
	}

	demux = mpc_demux_init(&reader);

	if (demux == NULL) {
		__end();
		return -1;
	}

	mpc_demux_get_info(demux, &info);

	if (info.average_bitrate != 0) {
		g_info.duration = (double) info.total_file_length * 8 / info.average_bitrate;
	}

	g_info.avg_bps = info.average_bitrate;
	g_info.sample_freq = info.sample_freq;
	g_info.channels = info.channels;
	g_info.filesize = info.total_file_length;

#if 0
	static bool gain_on = true;

	if (gain_on)
		mpc_set_replay_level(demux, MPC_OLD_GAIN_REF, MPC_TRUE, MPC_TRUE, MPC_TRUE);
	else
		mpc_set_replay_level(demux, MPC_OLD_GAIN_REF, MPC_FALSE, MPC_FALSE, MPC_FALSE);
	gain_on = !gain_on;
#endif

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

	g_buff = xAudioAlloc(0, sizeof(*g_buff) * MPC_DECODER_BUFFER_LENGTH);

	if (g_buff == NULL) {
		__end();
		return -1;
	}

	xAudioSetChannelCallback(0, mpc_audiocallback, NULL);

	generic_set_status(ST_LOADED);

	return 0;
}

/**
 * ֹͣMusepack�����ļ��Ĳ��ţ�������Դ��
 *
 * @note �����ڲ����߳��е���
 *
 * @return �ɹ�ʱ����0
 */
static int __end(void)
{
	xAudioEndPre();

	generic_set_status(ST_STOPPED);
	g_play_time = 0.;

	return 0;
}

/**
 * ֹͣMusepack�����ļ��Ĳ��ţ�������ռ�е��̡߳���Դ��
 *
 * @note �������ڲ����߳��е��ã������ܹ�����ظ����ö�������
 *
 * @return �ɹ�ʱ����0
 */
static int mpc_end(void)
{
	__end();

	xAudioEnd();

	generic_set_status(ST_STOPPED);

	if (demux != NULL) {
		mpc_demux_exit(demux);
		demux = NULL;
	}

	if (reader.data != NULL) {
		mpc_reader_exit_buffered_reader(&reader);
		reader.data = NULL;
	}

	free_bitrate(&g_inst_br);
	generic_end();

	if (g_buff != NULL) {
		xAudioFree(g_buff);
		g_buff = NULL;
	}

	return 0;
}

/**
 * PSP׼������ʱMusepack�Ĳ���
 *
 * @return �ɹ�ʱ����0
 */
static int mpc_suspend(void)
{
	generic_suspend();
	mpc_end();

	return 0;
}

/**
 * PSP׼��������ʱ�ָ���Musepack�Ĳ���
 *
 * @param spath ��ǰ������������8.3·����ʽ
 * @param lpath ��ǰ���������������ļ�����ʽ
 *
 * @return �ɹ�ʱ����0
 */
static int mpc_resume(const char *spath, const char *lpath)
{
	int ret;

	ret = mpc_load(spath, lpath);
	if (ret != 0) {
		dbg_printf(d, "%s: mpc_load failed %d", __func__, ret);
		return -1;
	}

	g_play_time = g_suspend_playing_time;
	free_bitrate(&g_inst_br);
	mpc_demux_seek_second(demux, g_play_time);
	g_suspend_playing_time = 0;

	generic_resume(spath, lpath);

	return 0;
}

/**
 * �õ�Musepack�����ļ������Ϣ
 *
 * @param pinfo ��Ϣ�ṹ��ָ��
 *
 * @return
 */
static int mpc_get_info(struct music_info *pinfo)
{
	if (pinfo->type & MD_GET_CURTIME) {
		pinfo->cur_time = g_play_time;
	}
	if (pinfo->type & MD_GET_CPUFREQ) {
		pinfo->psp_freq[0] = 66 + (120 - 66) * g_info.avg_bps / 1000 / 320;
		pinfo->psp_freq[1] = pinfo->psp_freq[0] / 2;
	}
	if (pinfo->type & MD_GET_INSKBPS) {
		pinfo->ins_kbps = get_inst_bitrate(&g_inst_br) / 1000;
	}
	if (pinfo->type & MD_GET_DECODERNAME) {
		STRCPY_S(pinfo->decoder_name, "musepack");
	}
	if (pinfo->type & MD_GET_ENCODEMSG) {
		if (config.show_encoder_msg) {
			SPRINTF_S(pinfo->encode_msg, "SV %lu.%lu, Profile %s (%s)", info.stream_version & 15, info.stream_version >> 4, info.profile_name, info.encoder);
		} else {
			pinfo->encode_msg[0] = '\0';
		}
	}

	return generic_get_info(pinfo);
}

/**
 * ����Ƿ�ΪMPC�ļ���Ŀǰֻ����ļ���׺��
 *
 * @param spath ��ǰ������������8.3·����ʽ
 *
 * @return ��MPC�ļ�����1�����򷵻�0
 */
static int mpc_probe(const char *spath)
{
	const char *p;

	p = utils_fileext(spath);

	if (p) {
		if (stricmp(p, "mpc") == 0) {
			return 1;
		}
		if (stricmp(p, "mp+") == 0) {
			return 1;
		}
		if (stricmp(p, "mpp") == 0) {
			return 1;
		}
	}

	return 0;
}

static int mpc_set_opt(const char *unused, const char *values)
{
	int argc, i;
	char **argv;

	g_io_buffer_size = BUFFERED_READER_BUFFER_SIZE;

	dbg_printf(d, "%s: options are %s", __func__, values);

	build_args(values, &argc, &argv);

	for (i = 0; i < argc; ++i) {
		if (!strncasecmp(argv[i], "mpc_buffer_size", sizeof("mpc_buffer_size") - 1)) {
			const char *p = argv[i];

			if ((p = strrchr(p, '=')) != NULL) {
				p++;
				g_io_buffer_size = atoi(p);

				if (g_io_buffer_size < 8192) {
					g_io_buffer_size = 0;
				}
			}
		}
	}

	clean_args(argc, argv);

	return 0;
}

static struct music_ops mpc_ops = {
	.name = "musepack",
	.set_opt = mpc_set_opt,
	.load = mpc_load,
	.play = NULL,
	.pause = NULL,
	.end = mpc_end,
	.get_status = NULL,
	.fforward = NULL,
	.fbackward = NULL,
	.suspend = mpc_suspend,
	.resume = mpc_resume,
	.get_info = mpc_get_info,
	.probe = mpc_probe,
	.next = NULL
};

int mpc_init(void)
{
	return register_musicdrv(&mpc_ops);
}

#endif