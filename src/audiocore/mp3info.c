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

#include "common/datatype.h"
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <pspkernel.h>
#include <malloc.h>
#include <stdlib.h>
#include "strsafe.h"
#include "musicdrv.h"
#include "mp3info.h"
#include "common/utils.h"
#include "charsets.h"
#include "dbg.h"
#include "config.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

#if defined(ENABLE_MP3)|| defined(ENABLE_TTA)

typedef int64_t offset_t;

typedef struct MPADecodeContext
{
	//    DECLARE_ALIGNED_8(uint8_t, last_buf[2*BACKSTEP_SIZE + EXTRABYTES]);
	int last_buf_size;
	int frame_size;
	/* next header (used in free format parsing) */
	uint32_t free_format_next_header;
	int error_protection;
	int layer;
	int sample_rate;
	int sample_rate_index;		/* between 0 and 8 */
	int bit_rate;
	//    GetBitContext gb;
	//    GetBitContext in_gb;
	int nb_channels;
	int mode;
	int mode_ext;
	int lsf;
	//    DECLARE_ALIGNED_16(MPA_INT, synth_buf[MPA_MAX_CHANNELS][512 * 2]);
	//    int synth_buf_offset[MPA_MAX_CHANNELS];
	//    DECLARE_ALIGNED_16(int32_t, sb_samples[MPA_MAX_CHANNELS][36][SBLIMIT]);
	//    int32_t mdct_buf[MPA_MAX_CHANNELS][SBLIMIT * 18]; /* previous samples, for layer 3 MDCT */
#ifdef DEBUG
	int frame_count;
#endif
	//    void (*compute_antialias)(struct MPADecodeContext *s, struct GranuleDef *g);
	int adu_mode;
	int dither_state;
	int error_recognition;
	//    AVCodecContext* avctx;
} MPADecodeContext;

#define ID3v2_HEADER_SIZE 10

static int id3v2_match(const uint8_t * buf)
{
	return buf[0] == 'I' &&
		buf[1] == 'D' &&
		buf[2] == '3' && buf[3] != 0xff && buf[4] != 0xff && (buf[6] & 0x80) == 0 && (buf[7] & 0x80) == 0 && (buf[8] & 0x80) == 0 && (buf[9] & 0x80) == 0;
}

static int _bitrate[9][16] = {
	{0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448,
	 0},
	{0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0},
	{0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0},
	{0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, 0},
	{0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0},
	{0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0},
	{0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, 0},
	{0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0},
	{0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0}
};

static int _sample_freq[3][4] = {
	{44100, 48000, 32000, 0},
	{22050, 24000, 16000, 0},
	{11025, 12000, 8000, 0}
};

static int check_bc_combination(int bitrate, uint8_t channel_mode)
{
	if (bitrate == 64 || bitrate == 96 || bitrate == 112 || bitrate == 128 || bitrate == 160 || bitrate == 192)
		return 0;

	if (channel_mode == 0 || channel_mode == 1 || channel_mode == 2) {
		if (bitrate == 224 || bitrate == 256 || bitrate == 320 || bitrate == 384)
			return 0;
	}

	if (channel_mode == 3) {
		if (bitrate == 32 || bitrate == 48 || bitrate == 56 || bitrate == 80)
			return 0;
	}

	return -1;
}

static bool check_next_frame_header(mp3_reader_data * data, uint8_t * buf, uint32_t bufpos, uint32_t bufsize, uint32_t this_frame, uint32_t next_frame)
{
	uint32_t orig;
	bool result = false;
	uint8_t tmp[2];

	if (bufpos + next_frame - this_frame + 1 < bufsize) {
		uint8_t *ptr;

		ptr = &buf[bufpos + next_frame - this_frame];

		if (ptr[0] == 0xff && (ptr[1] & 0xe0) == 0xe0) {
			result = true;
		} else {
			result = false;
		}
	} else {
		orig = buffered_reader_position(data->r);
		buffered_reader_seek(data->r, next_frame);

		if (buffered_reader_read(data->r, tmp, sizeof(tmp)) != sizeof(tmp)) {
			// EOF? Ignore this error
			result = true;
		} else {
			if (tmp[0] == 0xff && (tmp[1] & 0xe0) == 0xe0) {
				result = true;
			} else {
				result = false;
			}
		}

		buffered_reader_seek(data->r, orig);
	}

	return result;
}

static inline int parse_frame(uint8_t * buf, size_t bufpos, size_t bufsize, int *lv, int *br, struct MP3Info *info, mp3_reader_data * data, offset_t start)
{
	uint8_t *h = buf + bufpos;
	uint8_t mp3_version, mp3_level;
	uint8_t layer;
	uint8_t bitrate_bit;
	uint8_t pad;
	uint8_t channel_mode;
	uint32_t framelenbyte;
	int bitrate;
	int freq;
	uint16_t mp3_samples_per_frames[9] = { 384, 1152, 1152, 384, 1152, 576, 384, 1152, 576 };
	uint16_t spf;

	if (h[0] != 0xff)
		return -1;

	if ((h[1] & 0xe0) != 0xe0)
		return -1;

	switch ((h[1] >> 3) & 0x03) {
		case 0:
			mp3_version = 2;	// MPEG 2.5
			break;
		case 2:
			mp3_version = 1;	// MPEG 2
			break;
		case 3:
			mp3_version = 0;	// MPEG 1
			break;
		default:
			mp3_version = 0;
			break;
	}

	mp3_level = 3 - ((h[1] >> 1) & 0x03);

	if (mp3_level == 3)
		mp3_level--;

	spf = mp3_samples_per_frames[mp3_version * 3 + mp3_level];

	layer = 4 - ((h[1] >> 1) & 3);

	if (layer < 1 || layer > 3)
		return -1;

	bitrate_bit = (h[2] >> 4) & 0xf;
	bitrate = _bitrate[mp3_version * 3 + mp3_level][bitrate_bit];

	if (bitrate == 0)
		return -1;

	freq = _sample_freq[mp3_version][(h[2] >> 2) & 0x03];

	if (freq == 0) {
		return -1;
	}

	pad = (h[2] >> 1) & 1;
	channel_mode = (h[3] >> 6) & 3;

	if (layer == 2 && check_bc_combination(bitrate, channel_mode) != 0)
		return -1;

	if (*lv == 0)
		*lv = layer;

	*br = bitrate;

	if (mp3_level == 0)
		framelenbyte = ((spf / 8 * 1000) * bitrate / freq + pad) << 2;
	else
		framelenbyte = (spf / 8 * 1000) * bitrate / freq + pad;

	if (!check_next_frame_header(data, buf, bufpos, bufsize, start, start + framelenbyte)) {
		return -1;
	}

	if (info->sample_freq == 0)
		info->sample_freq = freq;

	if (info->channels == 0) {
		if (channel_mode == 3)
			info->channels = 1;
		else
			info->channels = 2;
	}

	if (info->spf == 0) {
		info->spf = spf;
	}

	return framelenbyte;
}

int skip_id3v2_tag(mp3_reader_data * data)
{
	int len;
	uint8_t buf[ID3v2_HEADER_SIZE];

	if (buffered_reader_read(data->r, buf, sizeof(buf)) != sizeof(buf)) {
		return -1;
	}

	if (id3v2_match(buf)) {
		struct MP3Info info;

		memset(&info, 0, sizeof(info));
		/* parse ID3v2 header */
		len = ((buf[6] & 0x7f) << 21) | ((buf[7] & 0x7f) << 14) | ((buf[8] & 0x7f) << 7) | (buf[9] & 0x7f);
		buffered_reader_seek(data->r, buffered_reader_position(data->r) + len);
	} else {
		buffered_reader_seek(data->r, 0);
	}

	return 0;
}

int search_valid_frame_me(mp3_reader_data * data, int *brate)
{
	uint32_t off, start, dcount = 0;
	int size = 0;
	int end;
	int level;
	uint8_t buf[1024 + 4];
	struct MP3Info inf, *info;

	info = &inf;
	memset(&inf, 0, sizeof(inf));

	level = info->sample_freq = info->channels = info->frames = 0;
	off = buffered_reader_position(data->r);

	if (buffered_reader_read(data->r, buf, 4) != 4) {
		return -1;
	}

	start = 0;
	while ((end = buffered_reader_read(data->r, &buf[4], sizeof(buf) - 4)) > 0) {
		while (start < end) {
			size = parse_frame(buf, start, end, &level, brate, info, data, dcount * (sizeof(buf) - 4) + off + start);

			if (size > 0) {
				goto found;
				break;
			}

			start++;
		}

		start -= end;
		memmove(buf, &buf[end], 4);
		dcount++;
	}

	if (end <= 0) {
		return -1;
	}

  found:
	buffered_reader_seek(data->r, dcount * (sizeof(buf) - 4) + off + start);

	return size;
}

static const uint16_t ff_mpa_freq_tab[3] = { 44100, 48000, 32000 };

static const uint16_t ff_mpa_bitrate_tab[2][3][15] = {
	{{0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448},
	 {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384},
	 {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320}},
	{{0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256},
	 {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160},
	 {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160}
	 }
};

/* fast header check for resync */
static inline int ff_mpa_check_header(uint32_t header)
{
	/* header */
	if ((header & 0xffe00000) != 0xffe00000)
		return -1;
	/* layer check */
	if ((header & (3 << 17)) == 0)
		return -1;
	/* bit rate */
	if ((header & (0xf << 12)) == 0xf << 12)
		return -1;
	/* frequency */
	if ((header & (3 << 10)) == 3 << 10)
		return -1;
	return 0;
}

static int ff_mpegaudio_decode_header(MPADecodeContext * s, uint32_t header)
{
	int sample_rate, frame_size, mpeg25, padding;
	int sample_rate_index, bitrate_index;

	if (header & (1 << 20)) {
		s->lsf = (header & (1 << 19)) ? 0 : 1;
		mpeg25 = 0;
	} else {
		s->lsf = 1;
		mpeg25 = 1;
	}

	s->layer = 4 - ((header >> 17) & 3);
	/* extract frequency */
	sample_rate_index = (header >> 10) & 3;
	sample_rate = ff_mpa_freq_tab[sample_rate_index] >> (s->lsf + mpeg25);
	sample_rate_index += 3 * (s->lsf + mpeg25);
	s->sample_rate_index = sample_rate_index;
	s->error_protection = ((header >> 16) & 1) ^ 1;
	s->sample_rate = sample_rate;

	bitrate_index = (header >> 12) & 0xf;
	padding = (header >> 9) & 1;
	//extension = (header >> 8) & 1;
	s->mode = (header >> 6) & 3;
	s->mode_ext = (header >> 4) & 3;
	//copyright = (header >> 3) & 1;
	//original = (header >> 2) & 1;
	//emphasis = header & 3;

	if (s->mode == MPA_MONO)
		s->nb_channels = 1;
	else
		s->nb_channels = 2;

	if (bitrate_index != 0) {
		frame_size = ff_mpa_bitrate_tab[s->lsf][s->layer - 1][bitrate_index];
		s->bit_rate = frame_size * 1000;
		switch (s->layer) {
			case 1:
				frame_size = (frame_size * 12000) / sample_rate;
				frame_size = (frame_size + padding) * 4;
				break;
			case 2:
				frame_size = (frame_size * 144000) / sample_rate;
				frame_size += padding;
				break;
			default:
			case 3:
				frame_size = (frame_size * 144000) / (sample_rate << s->lsf);
				frame_size += padding;
				break;
		}
		s->frame_size = frame_size;
	} else {
		/* if no frame size computed, signal it */
		return 1;
	}

	return 0;
}

static int mp3_parse_vbr_tags(mp3_reader_data * data, struct MP3Info *info, uint32_t off)
{
	const uint32_t xing_offtbl[2][2] = { {32, 17}, {17, 9} };
	uint32_t b, frames;
	MPADecodeContext ctx;
	char *p;

	frames = -1;

	if (buffered_reader_read(data->r, &b, sizeof(b)) != sizeof(b)) {
		return -1;
	}

	b = LB_CONV(b);

	if (ff_mpa_check_header(b) < 0)
		return -1;

	ff_mpegaudio_decode_header(&ctx, b);

	if (ctx.layer != 3) {
		info->is_mpeg1or2 = true;
		return -1;
	}

	info->channels = ctx.nb_channels;
	info->sample_freq = ctx.sample_rate;

	buffered_reader_seek(data->r, xing_offtbl[ctx.lsf == 1][ctx.nb_channels == 1] + buffered_reader_position(data->r));

	if (buffered_reader_read(data->r, &b, sizeof(b)) != sizeof(b)) {
		return -1;
	}

	b = LB_CONV(b);

	if (b == MKBETAG('X', 'i', 'n', 'g') || b == MKBETAG('I', 'n', 'f', 'o')) {
		u32 cur_pos = buffered_reader_position(data->r);

		if (buffered_reader_read(data->r, &b, sizeof(b)) != sizeof(b)) {
			return -1;
		}

		b = LB_CONV(b);

		if (b & 0x1) {
			if (buffered_reader_read(data->r, &frames, sizeof(frames)) != sizeof(frames)) {
				return -1;
			}
			frames = LB_CONV(frames);
		}

		buffered_reader_seek(data->r, cur_pos + 140 - 20 - 4 - 4);

		if (buffered_reader_read(data->r, &b, sizeof(b)) != sizeof(b)) {
			return -1;
		}

		b = LB_CONV(b);

		if (b != 0) {
			info->lame_vbr_quality = (100 - b) / 10;
		}

		if (buffered_reader_read(data->r, info->lame_str, sizeof(info->lame_str)) != sizeof(info->lame_str)) {
			return -1;
		}

		switch (info->lame_str[9] & 0xf) {
			case 1:
			case 8:
				info->lame_mode = CBR;
				break;
			case 2:
			case 9:
				info->lame_mode = ABR;
				break;
			case 3:
			case 4:
			case 5:
			case 6:
				info->lame_mode = VBR;
				break;
		}

		info->lame_str[9] = '\0';
		p = &info->lame_str[strlen(info->lame_str) - 1];

		if (p >= info->lame_str) {
			if (*p == '.')
				*p = '\0';
		}

		if (!strncmp(info->lame_str, "LAME", 4)
			|| !strncmp(info->lame_str, "GOGO", 4))
			info->lame_encoded = true;
	}

	/* Check for VBRI tag (always 32 bytes after end of mpegaudio header) */
	buffered_reader_seek(data->r, off + 4 + 32);

	if (buffered_reader_read(data->r, &b, sizeof(b)) != sizeof(b)) {
		return -1;
	}

	b = LB_CONV(b);
	if (b == MKBETAG('V', 'B', 'R', 'I')) {
		uint16_t t;

		/* Check tag version */
		if (buffered_reader_read(data->r, &t, sizeof(t)) != sizeof(t)) {
			return -1;
		}

		t = ((t & 0xff) << 8) | (t >> 8);

		if (t == 1) {
			/* skip delay, quality and total bytes */
			buffered_reader_seek(data->r, 8 + buffered_reader_position(data->r));

			if (buffered_reader_read(data->r, &frames, sizeof(frames)) != sizeof(frames)) {
				return -1;
			}

			frames = LB_CONV(frames);
		}
	}

	if ((int) frames < 0)
		return -1;

	info->spf = ctx.lsf ? 576 : 1152;	/* Samples per frame, layer 3 */
	info->duration = (double) frames *info->spf / info->sample_freq;

	info->frames = frames;
	info->average_bitrate = (double) data->size * 8 / info->duration;

	return frames;
}

int read_mp3_info_brute(struct MP3Info *info, mp3_reader_data * data)
{
	uint32_t off;
	int size, br = 0, dcount = 0;
	int end;
	int level;
	static uint8_t *buf;
	uint32_t first_frame = (uint32_t) - 1;

	if (data->r == NULL)
		return -1;

	buffered_reader_seek(data->r, 0);

	if (skip_id3v2_tag(data) == -1)
		return -1;

	buf = malloc(65536 + 4);

	if (!buf)
		return -1;

	off = buffered_reader_position(data->r);
	buffered_reader_seek(data->r, 0);

	if (buffered_reader_read(data->r, buf, 4) != 4) {
		free(buf);
		return -1;
	}

	level = info->sample_freq = info->channels = info->frames = 0;
	info->frameoff = NULL;

	while ((end = buffered_reader_read(data->r, &buf[4], 65536)) > 0) {
		while (off < end) {
			int brate = 0;

			if ((size = parse_frame(buf, off, end, &level, &brate, info, data, dcount * 65536 + off)) > 0) {
				br += brate;
				if (first_frame == (uint32_t) - 1)
					first_frame = dcount * 65536 + off;

				if (info->frames >= 0) {
					if (info->frames == 0)
						info->frameoff = malloc(sizeof(u32) * 1024);
					else
						info->frameoff = safe_realloc(info->frameoff, sizeof(u32) * (info->frames + 1024));
					if (info->frameoff == NULL)
						info->frames = -1;
					else
						info->frameoff[info->frames++] = dcount * 65536 + off;
				}
				off += size;
			} else
				off++;
		}
		off -= end;
		memmove(buf, &buf[end], 4);
		dcount++;
	}

	if (info->frames) {
		info->duration = 1.0 * info->spf * info->frames / info->sample_freq;
		info->average_bitrate = (double) data->size * 8 / info->duration;
	}

	buffered_reader_seek(data->r, first_frame);
	free(buf);

	return 0;
}

int read_mp3_info(struct MP3Info *info, mp3_reader_data * data)
{
	uint32_t off;

	info->frameoff = NULL;

	if (skip_id3v2_tag(data) == -1)
		return -1;

	off = buffered_reader_position(data->r);

	if (mp3_parse_vbr_tags(data, info, off) < 0) {
		dbg_printf(d, "%s: No Xing header found, use brute force search method", __func__);
		return read_mp3_info_brute(info, data);
	}

	buffered_reader_seek(data->r, off);

	return 0;
}

int free_mp3_info(struct MP3Info *info)
{
	if (info->frameoff) {
		free(info->frameoff);
		info->frameoff = NULL;
	}

	return 0;
}

#endif
