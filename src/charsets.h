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

#ifndef _CHARSETS_H_
#define _CHARSETS_H_

#include "common/datatype.h"

typedef u32 ucs4_t;

extern u32 charsets_utf32_conv(const u8 * ucs, size_t inputlen, u8 * cjk, size_t outputlen);
extern u32 charsets_ucs_conv(const u8 * ucs, size_t inputlen, u8 * cjk, size_t outputlen);
extern u32 charsets_big5_conv(const u8 * big5, size_t inputlen, u8 * cjk, size_t outputlen);
extern void charsets_sjis_conv(const u8 * jis, u8 ** cjk, u32 * newsize);
extern u32 charsets_utf8_conv(const u8 * ucs, size_t inputlen, u8 * cjk, size_t outputlen);
extern u32 charsets_utf16_conv(const u8 * ucs, size_t inputlen, u8 * cjk, size_t outputlen);
extern u32 charsets_utf16be_conv(const u8 * ucs, size_t inputlen, u8 * cjk, size_t outputlen);
extern int gbk_mbtowc(ucs4_t * pwc, const u8 * cjk, int n);
extern int gbk_wctomb(u8 * r, ucs4_t wc, int n);
extern int utf8_mbtowc(ucs4_t * pwc, const u8 * s, int n);

extern u16 charsets_gbk_to_ucs(const u8 * cjk);
extern u32 charsets_bg5hk2cjk(const u8 * big5hk, size_t inputlen, u8 * cjk, size_t outputlen);

#endif
