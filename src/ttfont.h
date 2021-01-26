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

#ifndef _TTFONT_H_
#define _TTFONT_H_

#include <pspkernel.h>
#include <malloc.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include "./common/datatype.h"
#include "fontconfig.h"

/** Truetype字形缓冲大小 */
#define SBIT_HASH_SIZE (1024)

/** 缓冲字形位图结构 */
typedef struct Cache_Bitmap_
{
	int width;
	int height;
	int left;
	int top;
	char format;
	short max_grays;
	int pitch;
	unsigned char *buffer;
} Cache_Bitmap;

/** 缓冲字形结构 */
typedef struct SBit_HashItem_
{
	unsigned long ucs_code;
	int glyph_index;
	int size;
	bool anti_alias;
	bool cleartype;
	bool embolden;
	int xadvance;
	int yadvance;
	Cache_Bitmap bitmap;
} SBit_HashItem;

/** Truetype字体结构 */
typedef struct _ttf
{
	FT_Library library;
	FT_Face face;

	const char *fontName;
	int pixelSize;

	SBit_HashItem sbitHashRoot[SBIT_HASH_SIZE];
	int cacheSize;
	int cachePop;

	char fnpath[PATH_MAX];
	int fileSize;
	u8 *fileBuffer;
	bool cjkmode;

	font_config config;
} t_ttf, *p_ttf;

/**
 * 打开TTF字体
 *
 * @param filename TTF文件名
 * @param size 预设的字体大小
 * @param load2mem 是否加载到内存
 *
 * @return 描述TTF的指针
 * - NULL 失败
 */
extern p_ttf ttf_open(const char *filename, int size, bool load2mem, bool cjkmode);

/**
 * 从指定数据中打开TTF字体
 *
 * @param ttfBuf TTF字体数据
 * @param ttfLength TTF字体数据大小，以字节计
 * @param pixelSize 预设的字体大小
 * @param ttfName TTF字体名
 *
 * @return 描述TTF的指针
 * - NULL 失败
 */
extern p_ttf ttf_open_buffer(void *ttfBuf, size_t ttfLength, int pixelSize, const char *ttfName, bool cjkmode);

/**
 * 关闭TTF字形缓存
 *
 * @param ttf ttf指针
 */
extern void ttf_close_cache(p_ttf ttf);

/**
 * 释放TTF字体
 *
 * @param ttf ttf指针
 */
extern void ttf_close(p_ttf ttf);

/**
 * 设置TTF字体大小
 *
 * @param ttf ttf指针
 * @param size 字体大小
 *
 * @return 是否成功
 */
extern bool ttf_set_pixel_size(p_ttf ttf, int size);

/**
 * 设置TTF是否启用抗锯齿效果
 *
 * @param ttf ttf指针
 * @param aa 是否启用抗锯齿效果
 */
extern void ttf_set_anti_alias(p_ttf ttf, bool aa);

/**
 * 设置TTF是否启用ClearType(Sub-Pixel LCD优化效果)
 *
 * @param ttf ttf指针
 * @param cleartype 是否启用cleartype
 */
extern void ttf_set_cleartype(p_ttf ttf, bool cleartype);

/**
 * 设置TTF是否启用字体加粗
 *
 * @param ttf ttf指针
 * @param embolden 是否启用字体加粗
 */
extern void ttf_set_embolden(p_ttf ttf, bool embolden);

/**
 * 得到字符串所能显示在maxpixels中的长度
 *
 * @note 如果遇到换行，则字符串计数停止累加。
 * <br>  如果字符串绘制长度＞maxpixels，则字符串计数停止累加。
 * <br>  这个版本速度快，但对于英文字母可能有出界问题
 * <br>  这个版本为英文回绕版本
 * 
 * @param cttf 中文TTF字体
 * @param ettf 英文TTF字体
 * @param str 字符串
 * @param maxpixels 最大象素长度
 * @param maxbytes 最大字符长度，以字节计
 * @param wordspace 字间距（以像素点计）
 *
 * @return 字符串个数计数，以字节计
 */
extern int ttf_get_string_width_english(p_ttf cttf, p_ttf ettf, const u8 * str, u32 maxpixels, u32 maxbytes, u32 wordspace);
/**
 * 得到字符串所能显示在maxpixels中的长度
 *
 * @note 如果遇到换行，则字符串计数停止累加。
 * <br>  如果字符串绘制长度＞maxpixels，则字符串计数停止累加。
 * <br>  这个版本速度快，但对于英文字母可能有出界问题
 * 
 * @param cttf 中文TTF字体
 * @param ettf 英文TTF字体
 * @param str 字符串
 * @param maxpixels 最大象素长度
 * @param maxbytes 最大字符长度，以字节计
 * @param wordspace 字间距（以像素点计）
 * @param pwidth 最终字符串长度指针（以像素点计）
 *
 * @return 字符串个数计数，以字节计
 */
extern int ttf_get_string_width(p_ttf cttf, p_ttf ettf, const u8 * str, u32 maxpixels, u32 maxbytes, u32 wordspace, u32 * pwidth);

/**
 * 得到字符串所能显示在maxpixels中的长度
 *
 * @note 如果遇到换行，则字符串计数停止累加。
 * <br>  如果字符串绘制长度＞maxpixels，则字符串计数停止累加。
 * <br>  这个版本速度极慢，不要使用
 * 
 * @param cttf 中文TTF字体
 * @param ettf 英文TTF字体
 * @param str 字符串
 * @param maxpixels 最大长度
 * @param wordspace 字间距（以像素点计）
 */
extern int ttf_get_string_width_hard(p_ttf cttf, p_ttf ettf, const u8 * str, u32 maxpixels, u32 wordspace);

/**
 * 得到英文字母宽度信息
 * 
 * @param ttf
 * @param ewidth
 * @param size
 */
extern void ttf_load_ewidth(p_ttf ttf, u8 * ewidth, int size);

/**
 * 绘制水平TTF字体汉字到屏幕
 *
 * @param cttf
 * @param ettf
 * @param x
 * @param y
 * @param color
 * @param str
 * @param count
 * @param wordspace
 * @param top
 * @param height
 * @param bot
 */
extern void disp_putnstring_horz_truetype(p_ttf cttf, p_ttf ettf, int x, int y,
										  pixel color, const u8 * str, int count, u32 wordspace, int top, int height, int bot);

/**
 * 绘制水平颠倒TTF字体汉字到屏幕
 *
 * @param cttf
 * @param ettf
 * @param x
 * @param y
 * @param color
 * @param str
 * @param count
 * @param wordspace
 * @param top
 * @param height
 * @param bot
 */
extern void disp_putnstring_reversal_truetype(p_ttf cttf, p_ttf ettf, int x,
											  int y, pixel color, const u8 * str, int count, u32 wordspace, int top, int height, int bot);

/**
 * 绘制左向TTF字体汉字到屏幕
 *
 * @param cttf
 * @param ettf
 * @param x
 * @param y
 * @param color
 * @param str
 * @param count
 * @param wordspace
 * @param top
 * @param height
 * @param bot
 */
extern void disp_putnstring_lvert_truetype(p_ttf cttf, p_ttf ettf, int x, int y,
										   pixel color, const u8 * str, int count, u32 wordspace, int top, int height, int bot);

/**
 * 绘制右向TTF字体汉字到屏幕
 *
 * @param cttf
 * @param ettf
 * @param x
 * @param y
 * @param color
 * @param str
 * @param count
 * @param wordspace
 * @param top
 * @param height
 * @param bot
 */
extern void disp_putnstring_rvert_truetype(p_ttf cttf, p_ttf ettf, int x, int y,
										   pixel color, const u8 * str, int count, u32 wordspace, int top, int height, int bot);

/** TTF加锁 */
extern void ttf_lock(void);

/** TTF解锁 */
extern void ttf_unlock(void);

/** 初始化TTF模块 */
extern void ttf_init(void);

/** 释放TTF模块 */
extern void ttf_free(void);

#endif
