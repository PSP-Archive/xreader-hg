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

#include <stdio.h>
#include <pspkernel.h>
#include <stdint.h>
#include <string.h>
#include <malloc.h>
#include "strsafe.h"
#include "musicinfo.h"
#include "conf.h"
#include "charsets.h"
#include "apetaglib/APETag.h"
#include "buffer.h"
#include "dbg.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

#define ID3v1_TAG_SIZE 128
#define ID3v2_HEADER_SIZE 10

#define LB_CONV(x)	\
    (((x) & 0xff)<<24) |  \
    (((x>>8) & 0xff)<<16) |  \
    (((x>>16) & 0xff)<< 8) |  \
    (((x>>24) & 0xff)    )

#define MKBETAG(a,b,c,d) (d | (c << 8) | (b << 16) | (a << 24))

#define UNUSED(x) ((void)(x))

typedef int64_t offset_t;

typedef struct
{
	MusicInfoTagType type;

	MusicTagInfo id3v1;
	MusicTagInfo id3v2;
	MusicTagInfo apetag;
} MusicInfoInternalTag, *PMusicInfoInternalTag;

buffer *tag_lyric = NULL;

static void id3v1_get_string(char *str, int str_size, const uint8_t * buf, int buf_size)
{
	int i, c;
	char *q;

	q = str;

	for (i = 0; i < buf_size; i++) {
		c = buf[i];
		if (c == '\0')
			break;
		if ((q - str) >= str_size - 1)
			break;
		*q++ = c;
	}

	*q = '\0';
}

static int id3v1_parse_tag(MusicInfoInternalTag * tag, SceUID fd, const uint8_t * buf)
{
	if (!(buf[0] == 'T' && buf[1] == 'A' && buf[2] == 'G'))
		return -1;

	id3v1_get_string(tag->id3v1.title, sizeof(tag->id3v1.title), buf + 3, 30);
	id3v1_get_string(tag->id3v1.artist, sizeof(tag->id3v1.artist), buf + 33, 30);
	id3v1_get_string(tag->id3v1.album, sizeof(tag->id3v1.album), buf + 63, 30);
	id3v1_get_string(tag->id3v1.comment, sizeof(tag->id3v1.comment), buf + 97, 30);

	tag->type |= ID3V1;
	tag->id3v1.encode = config.mp3encode;

	return 0;
}

static int read_id3v1(MusicInfoInternalTag * tag, const MusicInfo * music_info, SceUID fd)
{
	int ret;

	if (music_info->filesize > ID3v1_TAG_SIZE) {
		uint8_t buf[ID3v1_TAG_SIZE];

		sceIoLseek(fd, music_info->filesize - 128, PSP_SEEK_SET);
		ret = sceIoRead(fd, buf, ID3v1_TAG_SIZE);
		if (ret == ID3v1_TAG_SIZE) {
			id3v1_parse_tag(tag, fd, buf);
		}
	}

	return 0;
}

/** Convert bytes endian */
static int big2little_endian(uint8_t * p, size_t n)
{
#if 1
	size_t i;
	uint8_t t;

	if ((n % 2) != 0) {
		return -1;
	}

	for (i = 0; i < n; i += 2) {
		t = p[i];
		p[i] = p[i + 1];
		p[i + 1] = t;
	}
#endif

	return 0;
}

static void id3v2_read_ttag(SceUID fd, int taglen, char *dst, int dstlen, MusicTagInfo * info)
{
	int len;
	uint8_t b;

	if (dstlen > 0)
		dst[0] = 0;
	if (taglen < 1)
		return;

	taglen--;					/* account for encoding type u8 */
	dstlen--;					/* Leave space for zero terminator */

	if (sceIoRead(fd, &b, sizeof(b)) != sizeof(b)) {
		return;
	}

	switch (b) {				/* encoding type */
			// TODO: non-standard hack...
		case 0:				/* ISO-8859-1 (0 - 255 maps directly into unicode) */
			info->encode = config.mp3encode;
			{
				char *p = malloc(taglen);

				if (p == NULL) {
					return;
				}

				if (sceIoRead(fd, p, taglen) != taglen) {
					free(p);
					return;
				}

				strncpy_s(dst, dstlen, p, taglen);
				free(p);
			}

			break;
		case 1:
			/*
			   UTF-16 [UTF-16] encoded Unicode [UNICODE] with BOM. All
			   strings in the same frame SHALL have the same byteorder.
			   Terminated with $00 00.
			 */
			{
				uint8_t *buf = malloc(taglen);

				if (buf == NULL)
					return;

				if (sceIoRead(fd, buf, taglen) != taglen) {
					free(buf);
					return;
				}

				if (buf[0] != 0xff || buf[1] != 0xfe) {
					free(buf);
					return;
				}

				info->encode = conf_encode_gbk;
				len = min(taglen - 2, dstlen - 1);
				memcpy(dst, buf + 2, len);
				dst[len] = 0;
				charsets_ucs_conv((const u8 *) dst, len, (u8 *) dst, len);
				free(buf);
				break;
			}
		case 2:
			/* UTF-16 (Big-Endian) */
			{
				uint8_t *buf = malloc(taglen);

				if (buf == NULL)
					return;

				if (sceIoRead(fd, buf, taglen) != taglen) {
					free(buf);
					return;
				}

				if (big2little_endian(buf, taglen) != 0) {
					free(buf);
					return;
				}

				info->encode = conf_encode_gbk;
				len = min(taglen, dstlen - 1);
				memcpy(dst, buf, len);
				dst[len] = 0;
				charsets_ucs_conv((const u8 *) dst, len, (u8 *) dst, len);
				free(buf);
				break;
			}
			break;
		case 3:				/* UTF-8 */
			info->encode = conf_encode_utf8;
			len = min(taglen, dstlen - 1);
			if (sceIoRead(fd, dst, len) < 0) {
				return;
			}
			dst[len] = 0;
			break;
	}
}

static unsigned int id3v2_get_size(SceUID fd, int len)
{
	int v = 0;
	uint8_t b;

	while (len--) {
		sceIoRead(fd, &b, sizeof(b));
		v = (v << 8) + (b);
	}

	return v;
}

static unsigned int get_be16(SceUID fd)
{
	uint16_t val;

	sceIoRead(fd, &val, sizeof(val));
	val = ((val & 0xff) << 8) | (val >> 8);
	return val;
}

static unsigned int get_be32(SceUID fd)
{
	uint32_t val;

	sceIoRead(fd, &val, sizeof(val));
	val = LB_CONV(val);
	return val;
}

static uint8_t get_byte(SceUID fd)
{
	uint8_t val;

	sceIoRead(fd, &val, sizeof(val));
	return val;
}

static unsigned int get_be24(SceUID fd)
{
	uint32_t val;

	val = get_be16(fd) << 8;
	val |= get_byte(fd);
	return val;
}

/** Support ID3 or Sony OpenMG ID3 */
static int id3v2_match(const uint8_t * buf)
{
	return ((buf[0] == 'I' &&
			 buf[1] == 'D' &&
			 buf[2] == '3') || (buf[0] == 'e' &&
								buf[1] == 'a' &&
								buf[2] == '3')) &&
		buf[3] != 0xff && buf[4] != 0xff && (buf[6] & 0x80) == 0 && (buf[7] & 0x80) == 0 && (buf[8] & 0x80) == 0 && (buf[9] & 0x80) == 0;
}

static void id3v2_parse(MusicInfoInternalTag * tag_info, SceUID fd, int len, uint8_t version, uint8_t flags)
{
	int isv34, tlen;
	uint32_t tag;
	offset_t next;
	int taghdrlen;
	const char *reason;
	MusicTagInfo *info;

	info = &tag_info->id3v2;

	switch (version) {
		case 2:
			if (flags & 0x40) {
				reason = "compression";
				goto error;
			}
			isv34 = 0;
			taghdrlen = 6;
			break;

		case 3:
		case 4:
			isv34 = 1;
			taghdrlen = 10;
			break;

		default:
			reason = "version";
			goto error;
	}

#if 0
	if (flags & 0x80) {
		reason = "unsynchronization";
		goto error;
	}
#endif

	if (isv34 && flags & 0x40) {	/* Extended header present, just skip over it */
		sceIoLseek(fd, id3v2_get_size(fd, 4), PSP_SEEK_CUR);
	}

	while (len >= taghdrlen) {
		if (isv34) {
			tag = get_be32(fd);
			tlen = id3v2_get_size(fd, 4);
			get_be16(fd);		/* flags */
		} else {
			tag = get_be24(fd);
			tlen = id3v2_get_size(fd, 3);
		}
		len -= taghdrlen + tlen;

		if (len < 0)
			break;

		next = sceIoLseek(fd, 0, PSP_SEEK_CUR) + tlen;

		switch (tag) {
			case MKBETAG('T', 'I', 'T', '2'):
			case MKBETAG(0, 'T', 'T', '2'):
				id3v2_read_ttag(fd, tlen, info->title, sizeof(info->title), info);
				break;
			case MKBETAG('T', 'P', 'E', '1'):
			case MKBETAG(0, 'T', 'P', '1'):
				id3v2_read_ttag(fd, tlen, info->artist, sizeof(info->artist), info);
				break;
			case MKBETAG('T', 'A', 'L', 'B'):
			case MKBETAG(0, 'T', 'A', 'L'):
				id3v2_read_ttag(fd, tlen, info->album, sizeof(info->album), info);
				break;
			case MKBETAG('C', 'O', 'M', 'M'):
				id3v2_read_ttag(fd, tlen, info->comment, sizeof(info->comment), info);
				break;
			case MKBETAG('T', 'X', 'X', 'X'):
				{
					char desc[20], *p = desc;
					char ch;

					if (tag_lyric) {
						buffer_free(tag_lyric);
					}

					tag_lyric = buffer_init();

					sceIoLseek(fd, 1, PSP_SEEK_CUR);
					desc[0] = '\0';

					// Acount for text encoding u8
					tlen--;

					while (sceIoRead(fd, &ch, 1) == 1 && p - desc < sizeof(desc) && ch != '\0') {
						*p++ = ch;
						tlen--;
					}

					tlen--;
					*p = '\0';

					if (!strcmp(desc, "Lyrics")) {
						char *p = malloc(tlen);

						if (p == NULL) {
							return;
						}

						if (sceIoRead(fd, p, tlen) != tlen) {
							free(p);
							return;
						}

						buffer_copy_string_len(tag_lyric, p, tlen);
						free(p);
					}
				}
				break;
			case 0:
				/* padding, skip to end */
				sceIoLseek(fd, len, PSP_SEEK_CUR);
				len = 0;
				continue;
		}
		/* Skip to end of tag */
		sceIoLseek(fd, next, PSP_SEEK_SET);
	}

	tag_info->type |= ID3V2;

	if (version == 4 && flags & 0x10)	/* Footer preset, always 10 bytes, skip over it */
		sceIoLseek(fd, 10, PSP_SEEK_CUR);
	return;

  error:
	sceIoLseek(fd, len, PSP_SEEK_CUR);
}

static int read_id3v2_tag(MusicInfoInternalTag * tag, const MusicInfo * music_info, SceUID fd)
{
	uint8_t buf[ID3v2_HEADER_SIZE];
	int len;

	if (sceIoRead(fd, buf, sizeof(buf)) != sizeof(buf)) {
		return -1;
	}

	if (id3v2_match(buf)) {
		/* parse ID3v2 header */
		len = ((buf[6] & 0x7f) << 21) | ((buf[7] & 0x7f) << 14) | ((buf[8] & 0x7f) << 7) | (buf[9] & 0x7f);
		id3v2_parse(tag, fd, len, buf[3], buf[5]);
	} else {
		sceIoLseek(fd, 0, SEEK_SET);
	}

	return 0;
}

void read_ape_tag(MusicInfoInternalTag * tag_info, const char *spath)
{
	APETag *tag = apetag_load(spath);

	if (tag != NULL) {
		char *title = apetag_get(tag, "Title");
		char *artist = apetag_get(tag, "Artist");
		char *album = apetag_get(tag, "Album");

		if (title) {
			STRCPY_S(tag_info->apetag.title, title);
			free(title);
			title = NULL;
		}
		if (artist) {
			STRCPY_S(tag_info->apetag.artist, artist);
			free(artist);
			artist = NULL;
		} else {
			artist = apetag_get(tag, "Album artist");
			if (artist) {
				STRCPY_S(tag_info->apetag.artist, artist);
				free(artist);
				artist = NULL;
			}
		}
		if (album) {
			STRCPY_S(tag_info->apetag.album, album);
			free(album);
			album = NULL;
		}

		apetag_free(tag);
		tag_info->apetag.encode = conf_encode_utf8;
		tag_info->type |= APETAG;
	}
}

int generic_readtag(MusicInfo * music_info, const char *spath)
{
	SceUID fd;
	MusicInfoInternalTag tag;

	if (music_info == NULL || spath == NULL) {
		return -1;
	}

	memset(&tag, 0, sizeof(tag));
	tag.type = NONE;

	fd = sceIoOpen(spath, PSP_O_RDONLY, 0777);

	if (fd < 0) {
		return -1;
	}
	// Search ID3v1
	read_id3v1(&tag, music_info, fd);

	sceIoLseek(fd, 0, PSP_SEEK_SET);

	// Search ID3v2
	read_id3v2_tag(&tag, music_info, fd);

	sceIoLseek(fd, 0, PSP_SEEK_SET);

	sceIoClose(fd);

	// Search for APETag
	read_ape_tag(&tag, spath);

	if (config.apetagorder) {
		if (tag.type & APETAG) {
			memcpy(&music_info->tag, &tag.apetag, sizeof(music_info->tag));
		} else if (tag.type & ID3V2) {
			memcpy(&music_info->tag, &tag.id3v2, sizeof(music_info->tag));
		} else if (tag.type & ID3V1) {
			memcpy(&music_info->tag, &tag.id3v1, sizeof(music_info->tag));
		} else {
			memset(&music_info->tag, 0, sizeof(music_info->tag));
			music_info->tag.type = NONE;
		}
	} else {
		if (tag.type & ID3V2) {
			memcpy(&music_info->tag, &tag.id3v2, sizeof(music_info->tag));
		} else if (tag.type & APETAG) {
			memcpy(&music_info->tag, &tag.apetag, sizeof(music_info->tag));
		} else if (tag.type & ID3V1) {
			memcpy(&music_info->tag, &tag.id3v1, sizeof(music_info->tag));
		} else {
			memset(&music_info->tag, 0, sizeof(music_info->tag));
			music_info->tag.type = NONE;
		}
	}

	return 0;
}
