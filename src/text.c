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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pspuser.h>

#include <unzip.h>
#include <chm_lib.h>
#include <unrar.h>
#include <ctype.h>
#include "common/utils.h"
#include "charsets.h"
#include "display.h"
#include "html.h"
#include "text.h"
#include "kubridge.h"
#include "dbg.h"
#include "buffer.h"
#include "scene.h"
#include "archive.h"
#include "conf.h"
#include "unumd.h"
#include "depdb.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

/**
 * 计算电子书内容所对应GI值
 *
 * @param txt 电子书结构指针
 */
static void calc_gi(p_text txt)
{
	int offset, i;

	for (i = 0, offset = 0; i < txt->row_count; ++i) {
		p_textrow tr;

		tr = txt->rows[i >> 10] + (i & 0x3FF);
		offset += tr->count;
		tr->GI = offset / 1023 + 1;
	}
}

/**
 * 根据编码encode解码电子书
 *
 * @param txt 电子书数据结构指针
 * @param encode 文本编码类型
 */
static void text_decode(p_text txt, t_conf_encode encode)
{
	if (txt->size < 2)
		return;
	if (*(u16 *) txt->buf == 0xFEFF) {
		txt->size = charsets_ucs_conv((const u8 *) (txt->buf + 2), txt->size - 2, (u8 *) txt->buf, txt->size);
		txt->ucs = 1;
	} else if (*(u16 *) txt->buf == 0xFFEF) {
		txt->size = charsets_utf16be_conv((const u8 *) (txt->buf + 2), txt->size - 2, (u8 *) txt->buf, txt->size);
		txt->ucs = 1;
	} else if (txt->size > 2 && (unsigned char) txt->buf[0] == 0xEF && (unsigned char) txt->buf[1] == 0xBB && (unsigned char) txt->buf[2] == 0xBF) {
		txt->size = charsets_utf8_conv((const u8 *) (txt->buf + 3), txt->size - 3, (u8 *) txt->buf, txt->size);
		txt->ucs = 2;
	} else {
		switch (encode) {
			case conf_encode_big5:
				txt->size = charsets_big5_conv((const u8 *) txt->buf, txt->size, (u8 *) txt->buf, txt->size);
				txt->ucs = 0;
				break;
			case conf_encode_sjis:
				{
					char *orgbuf = txt->buf;
					u8 *newbuf;

					charsets_sjis_conv((const u8 *) orgbuf, &newbuf, &txt->size);
					txt->buf = (char *) newbuf;
					if (txt->buf != NULL)
						free(orgbuf);
					else
						txt->buf = orgbuf;
					txt->ucs = 0;
				}
				break;
			case conf_encode_ucs:
				{
					txt->size = charsets_ucs_conv((const u8 *) txt->buf, txt->size, (u8 *) txt->buf, txt->size);
					txt->ucs = 1;
				}
				break;
			case conf_encode_utf8:
				{
					txt->size = charsets_utf8_conv((const u8 *) txt->buf, txt->size, (u8 *) txt->buf, txt->size);
					txt->ucs = 2;
				}
				break;
			default:;
		}
	}
}

bool bytetable[256] = {
	1, 0, 0, 0, 0, 0, 0, 0, 0, 2, 1, 0, 0, 1, 0, 0,	// 0x00
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 0x10
	2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 0x20
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 0x30
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 0x40
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 0x50
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 0x60
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 0x70
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 0x80
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 0x90
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 0xA0
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 0xB0
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 0xC0
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 0xD0
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 0xE0
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0	// 0xF0
};

#ifdef ENABLE_TTF
extern p_ttf ettf, cttf;
#endif

int text_get_string_width_sys(const u8 * pos, size_t size, u32 wordspace)
{
	int width = 0;
	const u8 *posend = pos + size;

	while (pos < posend && bytetable[*(u8 *) pos] != 1) {
		if ((*(u8 *) pos) >= 0x80) {
			width += DISP_FONTSIZE;
			width += wordspace * 2;
			pos += 2;
		} else {
			int j;

			for (j = 0; j < (*pos == 0x09 ? config.tabstop : 1); ++j)
				width += DISP_FONTSIZE / 2;
			width += wordspace;
			++pos;
		}
	}

	return width;
}

/*
 * 得到文本当前行的显示长度，以像素计，文本显示使用
 *
 * @note 结果将满足最大显示宽度要求
 *
 * @param pos 文本起始位置
 * @param posend 文件结束位置
 * @param maxpixel 允许的最大显示宽度，以像素计
 * @param wordspace 字间距
 * @param count 字节数指针
 *
 * @return 显示宽度，以像素计
 */
static int text_get_string_width(const char *pos, const char *posend, u32 maxpixel, u32 wordspace, u32 * count)
{
	int width = 0;
	const char *posstart = pos;

	while (pos < posend && bytetable[*(u8 *) pos] != 1) {
		if ((*(u8 *) pos) >= 0x80) {
			width += DISP_BOOK_FONTSIZE;
			if (width > maxpixel)
				break;
			width += wordspace * 2;
			pos += 2;
		} else {
			int j;

			for (j = 0; j < (*pos == 0x09 ? config.tabstop : 1); ++j)
				width += disp_ewidth[*(u8 *) pos];
			if (width > maxpixel)
				break;
			width += wordspace;
			++pos;
		}
	}

	*count = pos - posstart;

	return width;
}

bool is_untruncateable_chars(char ch)
{
	static char charlist[] = {
		'~', '!', '@', '#', '$', '^', '&', '*', '(', ')', '_', '+', '{', '}',
		'|', ':', '"', '<', ',',
		'>', '?', '`', '-', '=', '[', ']', '\\', ';', '\'', ',', '.', '/'
	};
	int i;

	if (isalnum(ch))
		return true;

	for (i = 0; i < NELEMS(charlist); ++i)
		if (ch == charlist[i])
			return true;
	return false;
}

/**
 * 得到文本长度，以像素计，显示文本使用
 *
 * @note: 这个版本会将不截断英文单词
 *
 * @param pos 文本起始位置
 * @param posend 文件结束位置
 * @param maxpixel 允许的最大显示宽度，以像素计
 * @param wordspace 字间距
 * @param count 字节数指针
 *
 * @return 显示宽度，以像素计
 */
static int text_get_string_width_english(const char *pos, const char *posend, u32 maxpixel, u32 wordspace, u32 * count)
{
	int width = 0;
	const char *posstart = pos;
	const char *word_start, *word_end;

	word_start = NULL, word_end = NULL;
	while (pos < posend && bytetable[*(u8 *) pos] != 1) {
		if ((*(u8 *) pos) >= 0x80) {
			width += DISP_BOOK_FONTSIZE;
			if (width > maxpixel)
				break;
			width += wordspace * 2;
			pos += 2;
		} else {
			int j;

			if (is_untruncateable_chars(*pos)) {
				if (word_start == NULL) {
					u32 cnt;
					int ret;

					// search for next English u16
					word_end = word_start = pos;
					while (word_end <= posend && is_untruncateable_chars(*word_end)) {
						word_end++;
					}
					ret = text_get_string_width(word_start, word_end, maxpixel, wordspace, &cnt);

					if (ret < maxpixel && width + ret > maxpixel) {
						goto finish;
					}
				}
			} else {
				word_end = word_start = NULL;
			}
			for (j = 0; j < (*pos == 0x09 ? config.tabstop : 1); ++j)
				width += disp_ewidth[*(u8 *) pos];
			if (width > maxpixel)
				break;
			width += wordspace;
			++pos;
		}

	}

  finish:
	*count = pos - posstart;

	return width;
}

extern bool text_format(p_text txt, u32 max_pixels, u32 wordspace, bool ttf_mode)
{
	char *pos = txt->buf, *posend = pos + txt->size;
	u32 curs;

	txt->row_count = 0;
	for (curs = 0; curs < 1024; ++curs)
		if (txt->rows[curs] != NULL) {
			free(txt->rows[curs]);
			txt->rows[curs] = NULL;
		}
	curs = 0;
	while (txt->row_count < 1024 * 1024 && pos < posend) {
		char *startp;

		if ((txt->row_count % 1024) == 0) {
			curs = txt->row_count >> 10;
			if ((txt->rows[curs] = calloc(1024, sizeof(*txt->rows[curs]))) == NULL)
				return false;
		}

		txt->rows[curs][txt->row_count & 0x3FF].start = pos;
		startp = pos;

#ifdef ENABLE_TTF
		if (ttf_mode) {
			if (config.englishtruncate)
				pos += ttf_get_string_width_english(cttf, ettf, (const u8 *) pos, max_pixels, posend - pos, wordspace);
			else
				pos += ttf_get_string_width(cttf, ettf, (const u8 *) pos, max_pixels, posend - pos, wordspace, NULL);
		} else
#endif
		{
			u32 count = 0;

			if (config.englishtruncate)
				text_get_string_width_english(pos, posend, max_pixels, wordspace, &count);
			else
				text_get_string_width(pos, posend, max_pixels, wordspace, &count);
			pos += count;
		}
		if (pos + 1 < posend && bytetable[*(u8 *) pos] == 1) {
			if (*pos == '\r' && *(pos + 1) == '\n')
				pos += 2;
			else
				++pos;
		}
		txt->rows[curs][txt->row_count & 0x3FF].count = pos - startp;
		txt->row_count++;
		if (pos + 1 == posend && bytetable[*(u8 *) pos] == 1) {
			break;
		}
	}
	return true;
}

/**
 * 决定是否应合并文本段落的最小比率
 *
 * @note 如果当前文件行长大于平均行长的(1 - 1 / min_ratio) * 100%的话，就应合并
 */
static int min_ratio = 20;

/**
 * 计算平均行长 
 *
 * @param txtBuf 文本起始地址
 * @param txtLen 文本大小，以字节计
 *
 * @return 平均每行的长度，以字节计
 */
static size_t getTxtAvgLength(char *txtBuf, size_t txtLen)
{
	int linesize = 0, linecnt = 0;

	while (txtLen-- > 0) {
		if (*txtBuf == '\n') {
			linecnt++;
		} else {
			if (*txtBuf != '\r') {
				linesize++;
			}
		}
		txtBuf++;
	}
	if (linecnt != 0)
		return linesize / linecnt;
	return 0;
}

/**
 * 合并文本段落
 *
 * @note 能够合并段落
 * @note 不会改变txtbuf指向
 * @note 速度慢
 *
 * @param txtbuf 输入TXT指针的指针
 * @param txtlen 输入TXT大小，以字节计
 *
 * @return 新文件大小，以字节计
 */
static size_t text_paragraph_join_without_memory(char **txtbuf, size_t txtlen)
{
	char *src = *txtbuf;
	size_t avgLength;
	int nlinesz = 0, nol = 0;
	int cnt = txtlen;

	if (txtlen == 0 || txtbuf == NULL || *txtbuf == NULL)
		return 0;

	// 计算平均行长
	avgLength = getTxtAvgLength(*txtbuf, txtlen);

	while (cnt-- > 0) {
		if (*src == '\n') {
			nol++;
			if (nlinesz >= avgLength - avgLength * 1.0 / min_ratio) {
				// 将本行和下一行合并
				if (*(src - 1) == '\r') {
					if (txtlen > src - *txtbuf) {
						memmove(src - 1, src + 1, txtlen - (src - *txtbuf));
						txtlen -= 2;
					}
				} else {
					if (txtlen > src - *txtbuf) {
						memmove(src, src + 1, txtlen - (src - *txtbuf));
						txtlen--;
					}
				}
			}
			nlinesz = 0;
		} else if (*src != '\r') {
			nlinesz++;
		}
		src++;
	}

	return txtlen;
}

/**
 * 合并文本段落，分配内存版本，速度快
 *
 * @note 能够合并段落
 * @note txtbuf可能将指向一段新的被malloc分配的TXT数据
 * @note 如果不能分配足够的内存，将自动使用text_paragraph_join_without_memory
 *
 * @param txtbuf 输入TXT指针的指针，必须指向一段在堆上分配的内存
 * @param txtlen 输入TXT大小，以字节计
 *
 * @return 新文件大小
 */
static size_t text_paragraph_join_alloc_memory(char **txtbuf, size_t txtlen)
{
	char *p;
	int nlinesz, nol;
	int cnt;
	size_t avgLength;
	char *src, *dst;

	if (txtlen == 0 || txtbuf == NULL || *txtbuf == NULL)
		return 0;

	// 计算平均行长
	avgLength = getTxtAvgLength(*txtbuf, txtlen);
	src = *txtbuf, dst = NULL;

	if ((dst = calloc(1, txtlen)) == NULL) {
		return text_paragraph_join_without_memory(txtbuf, txtlen);
	}

	p = dst;
	nlinesz = 0, nol = 0;
	cnt = txtlen;

	while (cnt-- > 0) {
		if (*src == '\n') {
			nol++;
			if (nlinesz < avgLength - avgLength * 1.0 / min_ratio) {
				if (*(src - 1) == '\r') {
					*p++ = '\r';
				}
				*p++ = '\n';
			}
			nlinesz = 0;
		} else if (*src != '\r') {
			nlinesz++;
			*p++ = *src;
		}
		src++;
	}

	free(*txtbuf);
	*txtbuf = dst;

	return p - dst;
}

/**
 * 重新编排文本
 *
 * @note 将删除空行
 *
 * @param string 文本开始地址
 * @param size 文本大小
 *
 * @return 新文本大小
 */
static u32 text_reorder(char *string, u32 size)
{
	int i;
	char *wtxt = string, *ctxt = string, *etxt = string + size;

	while (ctxt < etxt) {
		while (ctxt < etxt && bytetable[*(u8 *) ctxt] == 0)
			*wtxt++ = *ctxt++;
		if (ctxt >= etxt)
			break;
		switch (*ctxt) {
			case '\t':
				*wtxt++ = ' ';
				ctxt++;
				for (i = 0; i < 3; i++)
					if (*ctxt == ' ') {
						*wtxt++ = ' ';
						ctxt++;
					}
				while (ctxt < etxt && *ctxt == ' ')
					ctxt++;
				break;
			case ' ':
				*wtxt++ = ' ';
				ctxt++;
				if (ctxt - 1 > string && *(ctxt - 2) == '\n')
					for (i = 0; i < 2; i++)
						if (*ctxt == ' ') {
							*wtxt++ = ' ';
							ctxt++;
						}
				if (*ctxt == ' ') {
					*wtxt++ = ' ';
					ctxt++;
				}
				while (ctxt < etxt && *ctxt == ' ')
					ctxt++;
				break;
			case '\r':
			case '\n':
				i = ((*ctxt == '\n') ? 1 : 0);
				*wtxt++ = '\n';
				ctxt++;
				while (ctxt < etxt && (*ctxt == '\r' || *ctxt == '\n')) {
					i += ((*ctxt == '\n') ? 1 : 0);
					ctxt++;
				}
				if (i > 2)
					*wtxt++ = '\n';
				break;
			case 0:
				ctxt++;
		}
	}
	return wtxt - string;
}

/**
 * 打开文本文件
 *
 * @param filename 文件路径
 * @param ft 文件类型
 * @param max_pixels 最大显示宽度，以像素计
 * @param wordspace 字间距
 * @param encode 文本编码类型
 * @param reorder 文本是否重新编排
 *
 * @return 新的电子书结构指针
 */
static p_text text_open(const char *filename, t_fs_filetype ft, u32 max_pixels, u32 wordspace, t_conf_encode encode, bool reorder)
{
	int fd;
	p_text txt = calloc(1, sizeof(*txt));

	if (txt == NULL)
		return NULL;
	if ((fd = sceIoOpen(filename, PSP_O_RDONLY, 0777)) < 0) {
		text_close(txt);
		return NULL;
	}
	STRCPY_S(txt->filename, filename);
	txt->size = sceIoLseek32(fd, 0, PSP_SEEK_END);
	if ((txt->buf = calloc(1, txt->size + 1)) == NULL) {
		sceIoClose(fd);
		text_close(txt);
		return NULL;
	}
	sceIoLseek32(fd, 0, PSP_SEEK_SET);
	sceIoRead(fd, txt->buf, txt->size);
	sceIoClose(fd);
	text_decode(txt, encode);
	if (ft == fs_filetype_html)
		txt->size = html_to_text(txt->buf, txt->size, true);
	if (reorder) {
		txt->size = text_reorder(txt->buf, txt->size);
		txt->size = text_paragraph_join_alloc_memory(&txt->buf, txt->size);
	}
	if (!text_format(txt, max_pixels, wordspace, using_ttf)) {
		text_close(txt);
		return NULL;
	}

	if (txt->rows[0] == NULL) {
		text_close(txt);
		return NULL;
	}

	calc_gi(txt);
	return txt;
}

static int fix_symbian_crlf(unsigned char *pos, unsigned char *posend)
{
	while (pos < posend) {
		if (pos < posend - 1 && *pos == 0xa1 && *(pos + 1) == 0xf6) {
			*pos = '\r';
			*(pos + 1) = '\n';
			pos += 2;
		} else {
			pos++;
		}
	}

	return 0;
}

extern p_text chapter_open_in_umd(const char *umdfile, const char *chaptername, u_int index, u32 rowpixels, u32 wordspace, t_conf_encode encode, bool reorder)
{
	extern p_umd_chapter p_umdchapter;
	buffer *pbuf = buffer_init();
	p_text txt;

	if (pbuf == NULL) {
		return NULL;
	}
	if (index < 1 || !p_umdchapter || 0 > read_umd_chapter_content(chaptername, index - 1, p_umdchapter, &pbuf)) {
		buffer_free(pbuf);
		return NULL;
	}

	txt = calloc(1, sizeof(*txt));

	if (txt == NULL) {
		buffer_free(pbuf);
		return NULL;
	}

	STRCPY_S(txt->filename, umdfile);
	if ((txt->buf = (char *) calloc(1, pbuf->used + 1)) == NULL) {
		buffer_free(pbuf);
		text_close(txt);
		return NULL;
	}

	txt->size = pbuf->used;
	memcpy(txt->buf, pbuf->ptr, txt->size);
	text_decode(txt, conf_encode_ucs);
	fix_symbian_crlf((unsigned char *) txt->buf, (unsigned char *) txt->buf + txt->size);
	buffer_free(pbuf);
	dbg_printf(d, "%s: after conv file length: %u", __func__, txt->size);
	/* if (ft == fs_filetype_html)
	   txt->size = html_to_text(txt->buf, txt->size, true); */
	if (reorder) {
		txt->size = text_reorder(txt->buf, txt->size);
		txt->size = text_paragraph_join_alloc_memory(&txt->buf, txt->size);
	}
	if (!text_format(txt, rowpixels, wordspace, using_ttf)) {
		text_close(txt);
		return NULL;
	}

	calc_gi(txt);
	return txt;
}

extern p_text text_open_in_umd(const char *umdfile, const char *chaptername, t_fs_filetype ft, u32 rowpixels, u32 wordspace, t_conf_encode encode, bool reorder)
{
	int fd;
	SceIoStat state;
	size_t filesize;
	buffer *pRaw, *pbuf;
	p_text txt;
	char *p;

	if (sceIoGetstat(umdfile, &state) < 0) {
		return NULL;
	}
	if ((fd = sceIoOpen(umdfile, PSP_O_RDONLY, 0777)) < 0) {
		return NULL;
	}
	filesize = state.st_size;
	pRaw = buffer_init();
	if (pRaw == NULL) {
		return NULL;
	}
	buffer_prepare_copy(pRaw, filesize);
	sceIoRead(fd, pRaw->ptr, filesize);
	sceIoClose(fd);
	p = pRaw->ptr;
	if (*(int *) p != 0xde9a9b89) {
		dbg_printf(d, "%s: not start with 0xde9a9b89, that umd must be corrupted.", __func__);
		buffer_free(pRaw);
		return NULL;
	}
	p += sizeof(int);
	while (*p != '#') {
		p++;
	}
	pbuf = buffer_init();
	if (pbuf == NULL) {
		buffer_free(pRaw);
		return NULL;
	}
	umd_readdata(&p, &pbuf);
	dbg_printf(d, "%s: parse file length: %d", __func__, pbuf->size);
	buffer_free(pRaw);
	txt = calloc(1, sizeof(*txt));
	if (txt == NULL) {
		buffer_free(pbuf);
		return NULL;
	}
	STRCPY_S(txt->filename, umdfile);
	if ((txt->buf = (char *) calloc(1, pbuf->used + 1)) == NULL) {
		buffer_free(pbuf);
		text_close(txt);
		return NULL;
	}
	txt->size = pbuf->used;
	memcpy(txt->buf, pbuf->ptr, txt->size);
	text_decode(txt, conf_encode_ucs);
	buffer_free(pbuf);
	dbg_printf(d, "%s: after conv file length %u", __func__, txt->size);
	fix_symbian_crlf((unsigned char *) txt->buf, (unsigned char *) txt->buf + txt->size);
	if (ft == fs_filetype_html)
		txt->size = html_to_text(txt->buf, txt->size, true);
	if (reorder) {
		txt->size = text_reorder(txt->buf, txt->size);
		txt->size = text_paragraph_join_alloc_memory(&txt->buf, txt->size);
	}
	if (!text_format(txt, rowpixels, wordspace, using_ttf)) {
		text_close(txt);
		return NULL;
	}

	calc_gi(txt);
	return txt;
}

extern p_text text_open_in_pdb(const char *pdbfile, const char *chaptername, t_fs_filetype ft, u32 rowpixels, u32 wordspace, t_conf_encode encode, bool reorder)
{
	SceIoStat state;
	size_t filesize;
	buffer *pbuf;
	p_text txt;

	if (sceIoGetstat(pdbfile, &state) < 0) {
		return NULL;
	}

	filesize = state.st_size;
	pbuf = calloc(1, sizeof(*pbuf));

	if (pbuf == NULL)
		return NULL;
	buffer_prepare_copy(pbuf, filesize);
	if (0 > read_pdb_data(pdbfile, &pbuf)) {
		buffer_free(pbuf);
		return NULL;
	}

	dbg_printf(d, "%s parse file length:%d!", __func__, pbuf->size);
	txt = calloc(1, sizeof(*txt));

	if (txt == NULL) {
		buffer_free(pbuf);
		return NULL;

	}
	STRCPY_S(txt->filename, pdbfile);
	if ((txt->buf = (char *) calloc(1, pbuf->used + 1)) == NULL) {
		buffer_free(pbuf);
		text_close(txt);
		return NULL;
	}

	txt->size = pbuf->used;
	memcpy(txt->buf, pbuf->ptr, txt->size);
	buffer_free(pbuf);
	txt->ucs = 0;

	if (reorder) {
		txt->size = text_reorder(txt->buf, txt->size);
		txt->size = text_paragraph_join_alloc_memory(&txt->buf, txt->size);
	}
	if (!text_format(txt, rowpixels, wordspace, using_ttf)) {
		text_close(txt);
		return NULL;
	}

	calc_gi(txt);
	return txt;
}

/*
 * 以十六进制打开文本文件
 *
 * @param filename 文件路径
 * @param vert 是否为垂直（左向、右向）显示方式
 *
 * @return 新的电子书结构指针
 */
static p_text text_open_binary(const char *filename, bool vert)
{
	int fd;
	p_text txt = calloc(1, sizeof(*txt));
	u8 *tmpbuf;
	u32 bpr = (vert ? 43 : 66);
	u32 curs = 0;
	u8 *cbuf;
	u32 i;

	if (txt == NULL)
		return NULL;
	if ((fd = sceIoOpen(filename, PSP_O_RDONLY, 0777)) < 0) {
		text_close(txt);
		return NULL;
	}
	STRCPY_S(txt->filename, filename);
	txt->size = sceIoLseek32(fd, 0, PSP_SEEK_END);
	if (txt->size > 256 * 1024) {
		if (psp_model != PSP_MODEL_STANDARD) {
			if (txt->size > 4 * 1024 * 1024)
				txt->size = 4 * 1024 * 1024;
		} else {
			txt->size = 256 * 1024;
		}
	}

	if ((txt->buf = calloc(1, (txt->size + 15) / 16 * bpr)) == NULL || (tmpbuf = calloc(1, txt->size)) == NULL) {
		sceIoClose(fd);
		text_close(txt);
		return NULL;
	}
	sceIoLseek32(fd, 0, PSP_SEEK_SET);
	sceIoRead(fd, tmpbuf, txt->size);
	sceIoClose(fd);

	txt->row_count = (txt->size + 15) / 16;
	cbuf = tmpbuf;

	for (i = 0; i < txt->row_count; i++) {
		if ((i % 1024) == 0) {
			curs = i >> 10;
			if ((txt->rows[curs] = calloc(1024, sizeof(*txt->rows[curs]))) == NULL) {
				free(tmpbuf);
				text_close(txt);
				return NULL;
			}
		}
		txt->rows[curs][i & 0x3FF].start = &txt->buf[bpr * i];
		txt->rows[curs][i & 0x3FF].count = bpr;
		if (vert) {
			snprintf(&txt->buf[bpr * i], bpr,
					 "%08X: %02X%02X%02X%02X%02X%02X%02X%02X %02X%02X%02X%02X%02X%02X%02X%02X",
					 (unsigned int) i * 0x10, cbuf[0], cbuf[1],
					 cbuf[2], cbuf[3], cbuf[4], cbuf[5], cbuf[6], cbuf[7], cbuf[8], cbuf[9], cbuf[10], cbuf[11], cbuf[12], cbuf[13], cbuf[14], cbuf[15]);
			if ((i + 1) * 16 > txt->size) {
				u32 padding = (i + 1) * 16 - txt->size;

				if (padding < 9)
					memset(&txt->buf[bpr * i + bpr - padding * 2], 0x20, padding * 2);
				else
					memset(&txt->buf[bpr * i + bpr - 1 - padding * 2], 0x20, padding * 2 + 1);
			}
		} else {
			u32 j;

			snprintf(&txt->buf[bpr * i], bpr,
					 "%08X: %02X%02X %02X%02X %02X%02X %02X%02X %02X%02X %02X%02X %02X%02X %02X%02X ",
					 (unsigned int) i * 0x10, cbuf[0], cbuf[1],
					 cbuf[2], cbuf[3], cbuf[4], cbuf[5], cbuf[6], cbuf[7], cbuf[8], cbuf[9], cbuf[10], cbuf[11], cbuf[12], cbuf[13], cbuf[14], cbuf[15]);

			for (j = 0; j < 16; j++)
				txt->buf[bpr * i + 40 + 10 + j] = (cbuf[j] > 0x1F && cbuf[j] < 0x7F) ? cbuf[j] : '.';
			if ((i + 1) * 16 > txt->size) {
				u32 padding = (i + 1) * 16 - txt->size;

				memset(&txt->buf[bpr * i + bpr - padding], 0x20, padding);
				if ((padding & 1) > 0)
					memset(&txt->buf[bpr * i + 40 + 10 - padding / 2 * 5 - 3], 0x20, padding / 2 * 5 + 3);
				else
					memset(&txt->buf[bpr * i + 40 + 10 - padding / 2 * 5], 0x20, padding / 2 * 5);
			}
		}
		cbuf += 16;
	}
	free(tmpbuf);
	calc_gi(txt);
	return txt;
}

/**
 * 打开GZIP压缩的文本文件
 *
 * @param gzfile GZ档案名
 * @param filename 显示用文件名
 * @param ft 文件类型
 * @param max_pixels 最大显示宽度，以像素计
 * @param wordspace 字间距
 * @param encode 文本编码类型
 * @param reorder 文本是否重新编排
 *
 * @return 新的电子书结构指针
 */
static p_text text_open_in_gz(const char *gzfile, const char *filename, t_fs_filetype ft, u32 max_pixels, u32 wordspace, t_conf_encode encode, bool reorder)
{
	p_text txt = calloc(1, sizeof(*txt));
	gzFile unzf;
	int len;
	char tempbuf[BUFSIZ];
	buffer *b;

	if (txt == NULL)
		return NULL;

	unzf = gzopen(gzfile, "rb");

	if (unzf == NULL) {
		text_close(txt);
		return NULL;
	}

	STRCPY_S(txt->filename, filename);
	b = buffer_init();

	while ((len = gzread(unzf, tempbuf, BUFSIZ)) > 0) {
		if (buffer_append_memory(b, tempbuf, len) < 0) {
			buffer_free(b);
			text_close(txt);
			gzclose(unzf);
			return NULL;
		}
	}
	if (len < 0) {
		buffer_free(b);
		text_close(txt);
		gzclose(unzf);
		return NULL;
	}
	gzclose(unzf);

	// get the buffer
	txt->size = b->used;
	txt->buf = buffer_free_weak(b);

	text_decode(txt, encode);
	if (ft == fs_filetype_html)
		txt->size = html_to_text(txt->buf, txt->size, true);
	if (reorder) {
		txt->size = text_reorder(txt->buf, txt->size);
		txt->size = text_paragraph_join_alloc_memory(&txt->buf, txt->size);
	}

	if (!text_format(txt, max_pixels, wordspace, using_ttf)) {
		text_close(txt);
		return NULL;
	}

	calc_gi(txt);
	return txt;
}

/**
 * 以十六进制方式打开ZIP档案里的文本文件
 *
 * @param zipfile ZIP档案路径
 * @param filename ZIP档案中的文件路径
 * @param ft 文件类型
 * @param max_pixels 最大显示宽度，以像素计
 * @param wordspace 字间距
 * @param encode 文本编码类型
 * @param reorder 文本是否重新编排
 * @param vert 是否为垂直（左向、右向）显示方式
 *
 * @return 新的电子书结构指针
 */
static p_text text_open_binary_in_zip(const char *zipfile, const char *filename,
									  t_fs_filetype ft, u32 max_pixels, u32 wordspace, t_conf_encode encode, bool reorder, bool vert)
{
	p_text txt = calloc(1, sizeof(*txt));
	buffer *buf = NULL;
	u8 *tmpbuf;
	u32 bpr = (vert ? 43 : 66);
	u32 curs;
	u8 *cbuf;
	u32 i;

	if (txt == NULL)
		return NULL;

	extract_archive_file_into_buffer(&buf, zipfile, filename, fs_filetype_zip);

	if (buf == NULL) {
		text_close(txt);
		return NULL;
	}
	STRCPY_S(txt->filename, filename);
	txt->size = buf->used;
	txt->buf = buffer_free_weak(buf);

	if (txt->size > 256 * 1024) {
		if (psp_model != PSP_MODEL_STANDARD) {
			if (txt->size > 4 * 1024 * 1024)
				txt->size = 4 * 1024 * 1024;
		} else {
			txt->size = 256 * 1024;
		}
	}

	tmpbuf = (u8 *) txt->buf;

	if ((txt->buf = calloc(1, (txt->size + 15) / 16 * bpr)) == NULL) {
		free(tmpbuf);
		text_close(txt);
		return NULL;
	}

	curs = 0;

	txt->row_count = (txt->size + 15) / 16;
	cbuf = tmpbuf;

	for (i = 0; i < txt->row_count; i++) {
		if ((i % 1024) == 0) {
			curs = i >> 10;
			if ((txt->rows[curs] = calloc(1024, sizeof(*txt->rows[curs]))) == NULL) {
				free(tmpbuf);
				text_close(txt);
				return NULL;
			}
		}
		txt->rows[curs][i & 0x3FF].start = &txt->buf[bpr * i];
		txt->rows[curs][i & 0x3FF].count = bpr;
		if (vert) {
			snprintf(&txt->buf[bpr * i], bpr,
					 "%08X: %02X%02X%02X%02X%02X%02X%02X%02X %02X%02X%02X%02X%02X%02X%02X%02X",
					 (unsigned int) i * 0x10, cbuf[0], cbuf[1],
					 cbuf[2], cbuf[3], cbuf[4], cbuf[5], cbuf[6], cbuf[7], cbuf[8], cbuf[9], cbuf[10], cbuf[11], cbuf[12], cbuf[13], cbuf[14], cbuf[15]);
			if ((i + 1) * 16 > txt->size) {
				u32 padding = (i + 1) * 16 - txt->size;

				if (padding < 9)
					memset(&txt->buf[bpr * i + bpr - padding * 2], 0x20, padding * 2);
				else
					memset(&txt->buf[bpr * i + bpr - 1 - padding * 2], 0x20, padding * 2 + 1);
			}
		} else {
			u32 j;

			snprintf(&txt->buf[bpr * i], bpr,
					 "%08X: %02X%02X %02X%02X %02X%02X %02X%02X %02X%02X %02X%02X %02X%02X %02X%02X ",
					 (unsigned int) i * 0x10, cbuf[0], cbuf[1],
					 cbuf[2], cbuf[3], cbuf[4], cbuf[5], cbuf[6], cbuf[7], cbuf[8], cbuf[9], cbuf[10], cbuf[11], cbuf[12], cbuf[13], cbuf[14], cbuf[15]);

			for (j = 0; j < 16; j++)
				txt->buf[bpr * i + 40 + 10 + j] = (cbuf[j] > 0x1F && cbuf[j] < 0x7F) ? cbuf[j] : '.';
			if ((i + 1) * 16 > txt->size) {
				u32 padding = (i + 1) * 16 - txt->size;

				memset(&txt->buf[bpr * i + bpr - padding], 0x20, padding);
				if ((padding & 1) > 0)
					memset(&txt->buf[bpr * i + 40 + 10 - padding / 2 * 5 - 3], 0x20, padding / 2 * 5 + 3);
				else
					memset(&txt->buf[bpr * i + 40 + 10 - padding / 2 * 5], 0x20, padding / 2 * 5);
			}
		}
		cbuf += 16;
	}
	free(tmpbuf);
	calc_gi(txt);
	return txt;
}

extern p_text text_open_in_raw(const char *filename, const unsigned char *data,
							   size_t size, t_fs_filetype ft, u32 max_pixels, u32 wordspace, t_conf_encode encode, bool reorder)
{
	p_text txt;

	if (data == NULL || size == 0) {
		return NULL;
	}

	txt = calloc(1, sizeof(*txt));

	if (txt == NULL)
		return NULL;

	STRCPY_S(txt->filename, filename);
	txt->size = size;

	if ((txt->buf = calloc(1, txt->size)) == NULL) {
		text_close(txt);
		return NULL;
	}

	memcpy(txt->buf, data, txt->size);

	text_decode(txt, encode);
	if (ft == fs_filetype_html)
		txt->size = html_to_text(txt->buf, txt->size, true);
	if (reorder) {
		txt->size = text_reorder(txt->buf, txt->size);
		txt->size = text_paragraph_join_alloc_memory(&txt->buf, txt->size);
	}
	if (!text_format(txt, max_pixels, wordspace, using_ttf)) {
		text_close(txt);
		return NULL;
	}

	calc_gi(txt);
	return txt;
}

/**
 * 打开ZIP档案里的文本文件
 *
 * @param zipfile ZIP档案路径
 * @param filename ZIP档案中的文件路径
 * @param ft 文件类型
 * @param max_pixels 最大显示宽度，以像素计
 * @param wordspace 字间距
 * @param encode 文本编码类型
 * @param reorder 文本是否重新编排
 *
 * @return 新的电子书结构指针
 */
static p_text text_open_in_zip(const char *zipfile, const char *filename, t_fs_filetype ft, u32 max_pixels, u32 wordspace, t_conf_encode encode, bool reorder)
{
	p_text txt = calloc(1, sizeof(*txt));
	buffer *buf = NULL;

	if (txt == NULL)
		return NULL;

	extract_archive_file_into_buffer(&buf, zipfile, filename, fs_filetype_zip);

	if (buf == NULL) {
		text_close(txt);
		return NULL;
	}

	STRCPY_S(txt->filename, filename);
	txt->size = buf->used;
	txt->buf = buffer_free_weak(buf);

	text_decode(txt, encode);
	if (ft == fs_filetype_html)
		txt->size = html_to_text(txt->buf, txt->size, true);
	if (reorder) {
		txt->size = text_reorder(txt->buf, txt->size);
		txt->size = text_paragraph_join_alloc_memory(&txt->buf, txt->size);
	}
	if (!text_format(txt, max_pixels, wordspace, using_ttf)) {
		text_close(txt);
		return NULL;
	}
	calc_gi(txt);

	return txt;
}

/**
 * 以十六进制方式打开RAR档案里的文本文件
 *
 * @param rarfile RAR档案路径
 * @param filename RAR档案中的文件路径
 * @param ft 文件类型
 * @param max_pixels 最大显示宽度，以像素计
 * @param wordspace 字间距
 * @param encode 文本编码类型
 * @param reorder 文本是否重新编排
 * @param vert 是否为垂直（左向、右向）显示方式
 *
 * @return 新的电子书结构指针
 */
static p_text text_open_binary_in_rar(const char *rarfile, const char *filename,
									  t_fs_filetype ft, u32 max_pixels, u32 wordspace, t_conf_encode encode, bool reorder, bool vert)
{
	p_text txt = calloc(1, sizeof(*txt));
	buffer *buf = NULL;
	u32 bpr = (vert ? 43 : 66);
	u8 *tmpbuf;
	u32 curs = 0;
	u32 i;
	u8 *cbuf;

	if (txt == NULL)
		return NULL;

	extract_archive_file_into_buffer(&buf, rarfile, filename, fs_filetype_rar);

	if (buf == NULL) {
		text_close(txt);
		return NULL;
	}
	STRCPY_S(txt->filename, filename);
	txt->size = buf->used;
	txt->buf = buffer_free_weak(buf);

	if (txt->size > 256 * 1024) {
		if (psp_model != PSP_MODEL_STANDARD) {
			if (txt->size > 4 * 1024 * 1024)
				txt->size = 4 * 1024 * 1024;
		} else {
			txt->size = 256 * 1024;
		}
	}

	tmpbuf = (u8 *) txt->buf;

	if ((txt->buf = calloc(1, (txt->size + 15) / 16 * bpr)) == NULL) {
		free(tmpbuf);
		text_close(txt);
		return NULL;
	}

	txt->row_count = (txt->size + 15) / 16;
	cbuf = tmpbuf;

	for (i = 0; i < txt->row_count; i++) {
		if ((i % 1024) == 0) {
			curs = i >> 10;
			if ((txt->rows[curs] = calloc(1024, sizeof(*txt->rows[curs]))) == NULL) {
				free(tmpbuf);
				text_close(txt);
				return NULL;
			}
		}
		txt->rows[curs][i & 0x3FF].start = &txt->buf[bpr * i];
		txt->rows[curs][i & 0x3FF].count = bpr;
		if (vert) {
			snprintf(&txt->buf[bpr * i], bpr,
					 "%08X: %02X%02X%02X%02X%02X%02X%02X%02X %02X%02X%02X%02X%02X%02X%02X%02X",
					 (unsigned int) i * 0x10, cbuf[0], cbuf[1],
					 cbuf[2], cbuf[3], cbuf[4], cbuf[5], cbuf[6], cbuf[7], cbuf[8], cbuf[9], cbuf[10], cbuf[11], cbuf[12], cbuf[13], cbuf[14], cbuf[15]);
			if ((i + 1) * 16 > txt->size) {
				u32 padding = (i + 1) * 16 - txt->size;

				if (padding < 9)
					memset(&txt->buf[bpr * i + bpr - padding * 2], 0x20, padding * 2);
				else
					memset(&txt->buf[bpr * i + bpr - 1 - padding * 2], 0x20, padding * 2 + 1);
			}
		} else {
			u32 j;

			snprintf(&txt->buf[bpr * i], bpr,
					 "%08X: %02X%02X %02X%02X %02X%02X %02X%02X %02X%02X %02X%02X %02X%02X %02X%02X ",
					 (unsigned int) i * 0x10, cbuf[0], cbuf[1],
					 cbuf[2], cbuf[3], cbuf[4], cbuf[5], cbuf[6], cbuf[7], cbuf[8], cbuf[9], cbuf[10], cbuf[11], cbuf[12], cbuf[13], cbuf[14], cbuf[15]);

			for (j = 0; j < 16; j++)
				txt->buf[bpr * i + 40 + 10 + j] = (cbuf[j] > 0x1F && cbuf[j] < 0x7F) ? cbuf[j] : '.';
			if ((i + 1) * 16 > txt->size) {
				u32 padding = (i + 1) * 16 - txt->size;

				memset(&txt->buf[bpr * i + bpr - padding], 0x20, padding);
				if ((padding & 1) > 0)
					memset(&txt->buf[bpr * i + 40 + 10 - padding / 2 * 5 - 3], 0x20, padding / 2 * 5 + 3);
				else
					memset(&txt->buf[bpr * i + 40 + 10 - padding / 2 * 5], 0x20, padding / 2 * 5);
			}
		}
		cbuf += 16;
	}
	free(tmpbuf);
	calc_gi(txt);
	return txt;
}

/**
 * 打开RAR档案里的文本文件
 *
 * @param rarfile RAR档案路径
 * @param filename RAR档案中的文件路径
 * @param ft 文件类型
 * @param max_pixels 最大显示宽度，以像素计
 * @param wordspace 字间距
 * @param encode 文本编码类型
 * @param reorder 文本是否重新编排
 *
 * @return 新的电子书结构指针
 */
static p_text text_open_in_rar(const char *rarfile, const char *filename, t_fs_filetype ft, u32 max_pixels, u32 wordspace, t_conf_encode encode, bool reorder)
{
	p_text txt = calloc(1, sizeof(*txt));
	buffer *buf = NULL;

	if (txt == NULL)
		return NULL;

	extract_archive_file_into_buffer(&buf, rarfile, filename, fs_filetype_rar);

	if (buf == NULL) {
		text_close(txt);
		return NULL;
	}
	STRCPY_S(txt->filename, filename);
	txt->size = buf->used;
	txt->buf = buffer_free_weak(buf);

	text_decode(txt, encode);
	if (ft == fs_filetype_html)
		txt->size = html_to_text(txt->buf, txt->size, true);
	if (reorder) {
		txt->size = text_reorder(txt->buf, txt->size);
		txt->size = text_paragraph_join_alloc_memory(&txt->buf, txt->size);
	}
	if (!text_format(txt, max_pixels, wordspace, using_ttf)) {
		text_close(txt);
		return NULL;
	}
	calc_gi(txt);
	return txt;
}

/**
 * 打开CHM档案里的文本文件
 *
 * @param chmfile CHM档案路径
 * @param filename CHM档案中的文件路径
 * @param ft 文件类型
 * @param max_pixels 最大显示宽度，以像素计
 * @param wordspace 字间距
 * @param encode 文本编码类型
 * @param reorder 文本是否重新编排
 *
 * @return 新的电子书结构指针
 */
static p_text text_open_in_chm(const char *chmfile, const char *filename, t_fs_filetype ft, u32 max_pixels, u32 wordspace, t_conf_encode encode, bool reorder)
{
	p_text txt = calloc(1, sizeof(*txt));
	buffer *buf = NULL;

	if (txt == NULL)
		return NULL;

	extract_archive_file_into_buffer(&buf, chmfile, filename, fs_filetype_chm);

	if (buf == NULL) {
		text_close(txt);
		return NULL;
	}
	STRCPY_S(txt->filename, filename);
	txt->size = buf->used;
	txt->buf = buffer_free_weak(buf);

	text_decode(txt, encode);
	if (ft == fs_filetype_html)
		txt->size = html_to_text(txt->buf, txt->size, true);
	if (reorder) {
		txt->size = text_reorder(txt->buf, txt->size);
		txt->size = text_paragraph_join_alloc_memory(&txt->buf, txt->size);
	}
	if (!text_format(txt, max_pixels, wordspace, using_ttf)) {
		text_close(txt);
		return NULL;
	}
	calc_gi(txt);
	return txt;
}

extern void text_close(p_text fstext)
{
	if (fstext != NULL) {
		u32 i;

		if (fstext->buf != NULL)
			free(fstext->buf);
		for (i = 0; i < 1024; ++i)
			if (fstext->rows[i] != NULL)
				free(fstext->rows[i]);

		free(fstext);
	}
}

extern p_text text_open_archive(const char *filename,
								const char *archname,
								t_fs_filetype filetype, u32 max_pixels, u32 wordspace, t_conf_encode encode, bool reorder, int where, int vertread)
{
	p_text pText = NULL;
	const char *ext;

	if (filename == NULL)
		return NULL;

	if (where != scene_in_dir && (archname == NULL || archname[0] == '\0'))
		return NULL;

	ext = utils_fileext(filename);

	switch (where) {
		case scene_in_dir:
			if (ext && stricmp(ext, "gz") == 0) {
				pText = text_open_in_gz(archname, archname, filetype, max_pixels, wordspace, encode, reorder);
			} else if (filetype == fs_filetype_umd) {
				pText = text_open_in_umd(archname, filename, filetype, max_pixels, wordspace, encode, reorder);
			} else if (filetype == fs_filetype_pdb) {
				pText = text_open_in_pdb(archname, filename, filetype, max_pixels, wordspace, encode, reorder);
			} else if (filetype != fs_filetype_unknown) {
				pText = text_open(archname, filetype, max_pixels, wordspace, encode, reorder);
			} else {
				pText = text_open_binary(archname, (vertread == conf_vertread_lvert || vertread == conf_vertread_rvert)
					);
			}
			break;
		case scene_in_umd:
			pText = text_open_in_umd(archname, filename, filetype, max_pixels, wordspace, encode, reorder);
			break;
		case scene_in_chm:
			pText = text_open_in_chm(archname, filename, filetype, max_pixels, wordspace, encode, reorder);
			break;
		case scene_in_zip:
			if (filetype == fs_filetype_txt || filetype == fs_filetype_html)
				pText = text_open_in_zip(archname, filename, filetype, max_pixels, wordspace, encode, reorder);
			else
				pText = text_open_binary_in_zip(archname, filename,
												filetype, max_pixels,
												wordspace, encode, reorder, (vertread == conf_vertread_lvert || vertread == conf_vertread_rvert));
			break;
		case scene_in_rar:
			if (filetype == fs_filetype_txt || filetype == fs_filetype_html)
				pText = text_open_in_rar(archname, filename, filetype, max_pixels, wordspace, encode, reorder);
			else
				pText = text_open_binary_in_rar(archname, filename,
												filetype, max_pixels,
												wordspace, encode, reorder, (vertread == conf_vertread_lvert || vertread == conf_vertread_rvert)
					);
			break;
	}

	return pText;
}
