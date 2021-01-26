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
#include "mp3info.h"
#include "musicinfo.h"
#include "common/utils.h"
#include "mediaengine.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

#ifdef ENABLE_MP3

#define UNUSED(x) ((void)(x))
#define BUFF_SIZE	8*1152

static mp3_reader_data mp3_data;

static int __end(void);

/**
 * MP3���ֲ��Ż���
 */
static u16 *g_buff = NULL;

/**
 * MP3���ֲ��Ż����С������������
 */
static unsigned g_buff_sample_size;

/**
 * MP3���ֲ��Ż��嵱ǰλ�ã�����������
 */
static int g_buff_sample_start;

/**
 * MP3�ļ���Ϣ
 */
static struct MP3Info mp3info;

/*
 * ʹ�ñ���
 */
static bool use_brute_method = false;

/**
 * Media Engine buffer����
 */
static unsigned long mp3_codec_buffer[65] __attribute__ ((aligned(64)));

static bool mp3_getEDRAM = false;

static uint8_t memp3_input_buf[2889] __attribute__ ((aligned(64)));

int memp3_decode(void *data, u32 data_len, void *pcm_data);

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
static u16 *copy_to_sndbuf(u16 *buf, u16 *srcbuf, int samples, int channels)
{
	int n;
	u16 *p;

	if (channels == 2) {
		memcpy(buf, srcbuf, samples * channels * sizeof(*srcbuf));
		p = buf + samples * channels;
	} else {
		n = samples * channels;

		while(n-- > 0) {
			*buf++ = *srcbuf;
			*buf++ = *srcbuf;
			srcbuf++;
		}

		p = buf;
	}

	return p;
}

static int mp3_seek_seconds_offset_brute(double npt)
{
	int pos;

	pos = (int) ((double) g_info.samples * (npt) / g_info.duration);

	if (pos < 0) {
		pos = 0;
	}

	dbg_printf(d, "%s: jumping to %d frame, offset %08x", __func__, pos, (int) mp3info.frameoff[pos]);
	dbg_printf(d, "%s: frame range (0~%u)", __func__, (unsigned) g_info.samples);

	if (pos >= g_info.samples) {
		__end();
		return -1;
	}

	buffered_reader_seek(mp3_data.r, mp3info.frameoff[pos]);

	if (pos <= 0)
		g_play_time = 0.0;
	else
		g_play_time = npt;

	return 0;
}

/**
  * Seek until found one valid mp3 frame, and then decode it into g_buff
  */
static int seek_and_decode(int *brate, u32 * first_found_frame)
{
	int ret, frame_size;
	u32 pos;

	do {
		pos = buffered_reader_position(mp3_data.r);

		if(pos == 0) {
			skip_id3v2_tag(&mp3_data);
		}

		frame_size = search_valid_frame_me(&mp3_data, brate);

		if (frame_size < 0) {
			return -1;
		}

		if (frame_size > sizeof(memp3_input_buf)) {
			ret = -1;
			goto retry;
		}

		ret = buffered_reader_read(mp3_data.r, memp3_input_buf, frame_size);

		if (ret <= 0) {
			return -1;
		}

		ret = memp3_decode(memp3_input_buf, ret, g_buff);

	  retry:
		if (ret < 0) {
			pos++;

			buffered_reader_seek(mp3_data.r, pos);

//          dbg_printf(d, "%s: jump to 0x%08X", __func__, (unsigned)pos);
		}
	} while (ret < 0);

//  dbg_printf(d, "%s: found first OK frame at 0x%08X", __func__, (unsigned)pos);

	if (first_found_frame != NULL) {
		*first_found_frame = pos;
	}

	return 0;
}

/**
 * ������һ����ЧMP3 frame
 *
 * @return
 * - <0 ʧ��
 * - 0 �ɹ�
 */
static int seek_valid_frame(void)
{
	int ret, brate;
	u32 pos;

	memset(memp3_input_buf, 0, sizeof(memp3_input_buf));

	ret = seek_and_decode(&brate, &pos);

	if (ret < 0) {
		return ret;
	}

	dbg_printf(d, "%s: found first OK frame at 0x%08X", __func__, (unsigned) pos);
	buffered_reader_seek(mp3_data.r, pos);

	return 0;
}

static int mp3_seek_seconds_offset(double npt)
{
	int new_pos;

	if (npt < 0) {
		npt = 0;
	} else if (npt > g_info.duration) {
		__end();
		return -1;
	}

	new_pos = npt * mp3info.average_bitrate / 8;

	if (new_pos < 0) {
		new_pos = 0;
	}

	buffered_reader_seek(mp3_data.r, new_pos);

	if (seek_valid_frame() == -1) {
		__end();
		return -1;
	}

	memset(g_buff, 0, BUFF_SIZE);
	g_buff_sample_size = g_buff_sample_start = 0;

	if (g_info.filesize > 0) {
		long cur_pos;

		cur_pos = buffered_reader_position(mp3_data.r);
		g_play_time = g_info.duration * cur_pos / g_info.filesize;
	} else {
		g_play_time = npt;
	}

	if (g_play_time < 0)
		g_play_time = 0;

	return 0;
}

static int mp3_seek_seconds(double npt)
{
	int ret;

	if (mp3info.frameoff && g_info.samples > 0) {
		ret = mp3_seek_seconds_offset_brute(npt);
	} else {
		ret = mp3_seek_seconds_offset(npt);
	}

	return ret;
}

/**
 * ����������
 *
 * @return
 * - -1 should exit
 * - 0 OK
 */
static int handle_seek(void)
{
	if (g_status == ST_FFORWARD) {
		generic_set_status(ST_PLAYING);
		generic_set_playback(true);
		mp3_seek_seconds(g_play_time + g_seek_seconds);
	} else if (g_status == ST_FBACKWARD) {
		generic_set_status(ST_PLAYING);
		generic_set_playback(true);
		mp3_seek_seconds(g_play_time - g_seek_seconds);
	}

	free_bitrate(&g_inst_br);
	g_buff_sample_size = g_buff_sample_start = 0;

	return 0;
}

int memp3_decode(void *data, u32 data_len, void *pcm_data)
{
	mp3_codec_buffer[6] = (uint32_t) data;
	mp3_codec_buffer[8] = (uint32_t) pcm_data;
	mp3_codec_buffer[7] = data_len;
	mp3_codec_buffer[10] = data_len;
	mp3_codec_buffer[9] = mp3info.spf << 2;

	return sceAudiocodecDecode(mp3_codec_buffer, 0x1002);
}

/**
 * MP3���ֲ��Żص�������ME�汾
 * ���𽫽��������������������
 *
 * @note �����������ĸ�ʽΪ˫������16λ������
 *
 * @param buf ����������ָ��
 * @param reqn ������֡��С
 * @param pdata �û����ݣ�����
 */
static int memp3_audiocallback(void *buf, unsigned int snd_buf_sample_size, void *pdata)
{
	int avail_sample, copy_sample;
	u16 *audio_buf = buf;
	double incr;

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
			int brate = 0, ret;

			ret = seek_and_decode(&brate, NULL);

			if (ret < 0) {
				__end();
				return -1;
			}

			g_buff_sample_size = mp3info.spf;
			incr = (double) g_buff_sample_size / mp3info.sample_freq;
			add_bitrate(&g_inst_br, brate * 1000, incr);
			g_buff_sample_start = 0;
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

	memset(&g_inst_br, 0, sizeof(g_inst_br));
	g_buff_sample_size = g_buff_sample_start = 0;
	g_seek_seconds = 0;

	g_play_time = 0.;
	memset(&mp3info, 0, sizeof(mp3info));
	memset(&g_info, 0, sizeof(g_info));

	return 0;
}

static int me_init()
{
	int ret;

	if (config.use_vaudio)
		ret = load_me_prx(VAUDIO | AVCODEC);
	else
		ret = load_me_prx(AVCODEC);

	if (ret < 0)
		return ret;

	memset(mp3_codec_buffer, 0, sizeof(mp3_codec_buffer));
	ret = sceAudiocodecCheckNeedMem(mp3_codec_buffer, 0x1002);

	if (ret < 0)
		return ret;

	mp3_getEDRAM = false;
	ret = sceAudiocodecGetEDRAM(mp3_codec_buffer, 0x1002);

	if (ret < 0)
		return ret;

	mp3_getEDRAM = true;

	ret = sceAudiocodecInit(mp3_codec_buffer, 0x1002);

	if (ret < 0) {
		sceAudiocodecReleaseEDRAM(mp3_codec_buffer);
		return ret;
	}

	memset(memp3_input_buf, 0, sizeof(memp3_input_buf));

	return 0;
}

void readMP3ApeTag(const char *spath)
{
	APETag *tag = apetag_load(spath);

	if (tag != NULL) {
		char *title = apetag_get(tag, "Title");
		char *artist = apetag_get(tag, "Artist");
		char *album = apetag_get(tag, "Album");

		if (title) {
			STRCPY_S(g_info.tag.title, title);
			free(title);
			title = NULL;
		}
		if (artist) {
			STRCPY_S(g_info.tag.artist, artist);
			free(artist);
			artist = NULL;
		} else {
			artist = apetag_get(tag, "Album artist");
			if (artist) {
				STRCPY_S(g_info.tag.artist, artist);
				free(artist);
				artist = NULL;
			}
		}
		if (album) {
			STRCPY_S(g_info.tag.album, album);
			free(album);
			album = NULL;
		}

		apetag_free(tag);
		g_info.tag.encode = conf_encode_utf8;
	}
}

static int mp3_load(const char *spath, const char *lpath)
{
	int ret;

	__init();

	dbg_printf(d, "%s: loading %s", __func__, spath);
	generic_set_status(ST_UNKNOWN);

	mp3_data.r = buffered_reader_open(spath, max(g_io_buffer_size, 8192), 1);

	if (mp3_data.r == NULL) {
		__end();
		return -1;
	}

	if(g_io_buffer_size == 0) {
		buffered_reader_enable_cache(mp3_data.r, 0);
	}

	g_info.filesize = buffered_reader_length(mp3_data.r);
	mp3_data.size = g_info.filesize;

	if ((ret = me_init()) < 0) {
		dbg_printf(d, "me_init failed: %d", ret);
		__end();
		return -1;
	}

	if (use_brute_method) {
		if (read_mp3_info_brute(&mp3info, &mp3_data) < 0) {
			__end();
			return -1;
		}
	} else {
		if (read_mp3_info(&mp3info, &mp3_data) < 0) {
			__end();
			return -1;
		}
	}

	/* MediaEngine always decodes mp3 data into stereo */
	g_info.channels = 2;
	g_info.sample_freq = mp3info.sample_freq;
	g_info.avg_bps = mp3info.average_bitrate;
	g_info.samples = mp3info.frames;
	g_info.duration = mp3info.duration;

	generic_readtag(&g_info, spath);

	dbg_printf(d, "[%d channel(s), %d Hz, %.2f kbps, %02d:%02d%sframes %d]",
			   g_info.channels, g_info.sample_freq,
			   g_info.avg_bps / 1000,
			   (int) (g_info.duration / 60), (int) g_info.duration % 60, mp3info.frameoff != NULL ? ", frame table, " : ", ", g_info.samples);

#ifdef _DEBUG
	if (mp3info.lame_encoded) {
		char lame_method[80];
		char encode_msg[80];

		switch (mp3info.lame_mode) {
			case ABR:
				STRCPY_S(lame_method, "ABR");
				break;
			case CBR:
				STRCPY_S(lame_method, "CBR");
				break;
			case VBR:
				SPRINTF_S(lame_method, "VBR V%1d", mp3info.lame_vbr_quality);
				break;
			default:
				break;
		}

		if (mp3info.lame_str[strlen(mp3info.lame_str) - 1] == ' ')
			SPRINTF_S(encode_msg, "%s%s", mp3info.lame_str, lame_method);
		else
			SPRINTF_S(encode_msg, "%s %s", mp3info.lame_str, lame_method);
		dbg_printf(d, "[ %s ]", encode_msg);
	}
#endif

	if (config.use_vaudio)
		xAudioSetFrameSize(2048);
	else
		xAudioSetFrameSize(4096);

	ret = xAudioInit();

	if (ret < 0) {
		__end();
		return -1;
	}

	ret = xAudioSetFrequency(g_info.sample_freq);
	if (ret < 0) {
		__end();
		return -1;
	}

	g_buff = xAudioAlloc(64, BUFF_SIZE);

	if (g_buff == NULL) {
		__end();
		return -1;
	}

	memset(g_buff, 0, BUFF_SIZE);
	xAudioSetChannelCallback(0, memp3_audiocallback, NULL);
	generic_set_status(ST_LOADED);

	return 0;
}

static void init_default_option(void)
{
	g_io_buffer_size = BUFFERED_READER_BUFFER_SIZE;
}

static int mp3_set_opt(const char *unused, const char *values)
{
	int argc, i;
	char **argv;

	init_default_option();

	dbg_printf(d, "%s: options are %s", __func__, values);

	build_args(values, &argc, &argv);

	for (i = 0; i < argc; ++i) {
		if (!strncasecmp(argv[i], "mp3_brute_mode", sizeof("mp3_brute_mode") - 1)) {
			if (opt_is_on(argv[i])) {
				use_brute_method = true;
			} else {
				use_brute_method = false;
			}
		} else if (!strncasecmp(argv[i], "mp3_buffer_size", sizeof("mp3_buffer_size") - 1)) {
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

static int mp3_get_info(struct music_info *info)
{
	if (info->type & MD_GET_CURTIME) {
		info->cur_time = g_play_time;
	}
	if (info->type & MD_GET_CPUFREQ) {
		if(g_io_buffer_size) {
			info->psp_freq[0] = 49;
		} else {
			info->psp_freq[0] = 34;
		}

		info->psp_freq[1] = 16;
	}
	if (info->type & MD_GET_DECODERNAME) {
		STRCPY_S(info->decoder_name, "mp3");
	}
	if (info->type & MD_GET_ENCODEMSG) {
		if (config.show_encoder_msg && mp3info.lame_encoded) {
			char lame_method[80];

			switch (mp3info.lame_mode) {
				case ABR:
					STRCPY_S(lame_method, "ABR");
					break;
				case CBR:
					STRCPY_S(lame_method, "CBR");
					break;
				case VBR:
					SPRINTF_S(lame_method, "VBR V%1d", mp3info.lame_vbr_quality);
					break;
				default:
					break;
			}

			if (mp3info.lame_str[strlen(mp3info.lame_str) - 1] == ' ')
				SPRINTF_S(info->encode_msg, "%s%s", mp3info.lame_str, lame_method);
			else
				SPRINTF_S(info->encode_msg, "%s %s", mp3info.lame_str, lame_method);
		} else {
			info->encode_msg[0] = '\0';
		}
	}
	if (info->type & MD_GET_INSKBPS) {
		info->ins_kbps = get_inst_bitrate(&g_inst_br) / 1000;
	}

	return generic_get_info(info);
}

/**
 * ֹͣMP3�����ļ��Ĳ��ţ�������ռ�е��̡߳���Դ��
 *
 * @note �������ڲ����߳��е��ã������ܹ�����ظ����ö�������
 *
 * @return �ɹ�ʱ����0
 */
static int mp3_end(void)
{
	dbg_printf(d, "%s", __func__);

	__end();

	xAudioEnd();

	generic_set_status(ST_STOPPED);

	if (mp3_getEDRAM)
		sceAudiocodecReleaseEDRAM(mp3_codec_buffer);

	free_mp3_info(&mp3info);
	free_bitrate(&g_inst_br);

	if (mp3_data.r != NULL) {
		buffered_reader_close(mp3_data.r);
		mp3_data.r = NULL;
	}

	if (g_buff != NULL) {
		xAudioFree(g_buff);
		g_buff = NULL;
	}

	generic_end();

	return 0;
}

/**
 * PSP׼������ʱMP3�Ĳ���
 *
 * @return �ɹ�ʱ����0
 */
static int mp3_suspend(void)
{
	dbg_printf(d, "%s", __func__);

	generic_suspend();
	mp3_end();

	return 0;
}

/**
 * PSP׼��������ʱ�ָ���MP3�Ĳ���
 *
 * @param spath ��ǰ������������8.3·����ʽ
 * @param lpath ��ǰ���������������ļ�����ʽ
 *
 * @return �ɹ�ʱ����0
 */
static int mp3_resume(const char *spath, const char *lpath)
{
	int ret;

	dbg_printf(d, "%s", __func__);
	ret = mp3_load(spath, lpath);

	if (ret != 0) {
		dbg_printf(d, "%s: mp3_load failed %d", __func__, ret);
		return -1;
	}

	mp3_seek_seconds(g_suspend_playing_time);
	g_suspend_playing_time = 0;

	generic_resume(spath, lpath);

	return 0;
}

/**
 * ����Ƿ�ΪMP3�ļ���Ŀǰֻ����ļ���׺��
 *
 * @param spath ��ǰ������������8.3·����ʽ
 *
 * @return ��MP3�ļ�����1�����򷵻�0
 */
static int mp3_probe(const char *spath)
{
	const char *p;

	p = utils_fileext(spath);

	if (p) {
		if (stricmp(p, "mp1") == 0) {
			return 1;
		}
		if (stricmp(p, "mp2") == 0) {
			return 1;
		}
		if (stricmp(p, "mp3") == 0) {
			return 1;
		}
		if (stricmp(p, "mpa") == 0) {
			return 1;
		}
		if (stricmp(p, "mpeg") == 0) {
			return 1;
		}
	}

	return 0;
}

static struct music_ops mp3_ops = {
	.name = "mp3",
	.set_opt = mp3_set_opt,
	.load = mp3_load,
	.play = NULL,
	.pause = NULL,
	.end = mp3_end,
	.get_status = NULL,
	.fforward = NULL,
	.fbackward = NULL,
	.suspend = mp3_suspend,
	.resume = mp3_resume,
	.get_info = mp3_get_info,
	.probe = mp3_probe,
	.next = NULL,
};

int mp3_init()
{
	return register_musicdrv(&mp3_ops);
}

/**
 * ֹͣMP3�����ļ��Ĳ��ţ�������Դ��
 *
 * @note �����ڲ����߳��е���
 *
 * @return �ɹ�ʱ����0
 */
static int __end(void)
{
	xAudioEndPre();

	g_play_time = 0.;
	generic_lock();
	generic_set_status(ST_STOPPED);
	generic_unlock();

	return 0;
}

#endif
