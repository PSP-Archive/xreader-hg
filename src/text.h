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

#ifndef _TEXT_H_
#define _TEXT_H_

#include "common/datatype.h"
#include "fs.h"

typedef struct
{
	char *start;
	u32 count;
	u32 GI;
} t_textrow, *p_textrow;

typedef struct
{
	/** 文件路径名 */
	char filename[PATH_MAX];

	u32 crow;

	u32 size;

	char *buf;

	/** 文本的Unicode编码模式
	 *
	 * @note 0 - 非Unicode编码
	 * @note 1 - UCS
	 * @note 2 - UTF-8
	 */
	int ucs;

	/** 行数 */
	u32 row_count;

	/** 行结构指针数组 */
	p_textrow rows[1024];
} t_text, *p_text;

/**
 * 格式化文本
 *
 * @note 按显示模式将文本拆分为行
 *
 * @param txt 电子书结构指针
 * @param max_pixels 最大显示宽度，以像素计
 * @param wordspace 字间距
 * @param ttf_mode 是否为TTF显示模式
 *
 * @return 是否成功
 */
extern bool text_format(p_text txt, u32 max_pixels, u32 wordspace, bool ttf_mode);

/**
 * 打开内存中一段数据做为文本
 *
 * @note 内存数据将被复制
 *
 * @param filename 无用途
 * @param data
 * @param size
 * @param ft 文件类型
 * @param max_pixels 最大显示宽度，以像素计
 * @param wordspace 字间距
 * @param encode 文本编码类型
 * @param reorder 文本是否重新编排
 *
 * @return 新的电子书结构指针
 */
extern p_text text_open_in_raw(const char *filename, const unsigned char *data,
							   size_t size, t_fs_filetype ft, u32 max_pixels, u32 wordspace, t_conf_encode encode, bool reorder);

/**
 * 打开文本文件，可用于档案文件
 * 
 * @note 支持GZ,CHM,RAR,ZIP以及TXT
 *
 * @param filename 文件路径
 * @param archname 档案路径
 * @param filetype 文本文件类型
 * @param max_pixels 最大显示宽度，以像素计 
 * @param wordspace 字距
 * @param encode 文本文件编码
 * @param reorder 是否重新编排
 * @param where 档案类型
 * @param vertread 显示方式
 *
 * @return 新的电子书结构指针
 * - NULL 失败
 */
extern p_text text_open_archive(const char *filename,
								const char *archname,
								t_fs_filetype filetype, u32 max_pixels, u32 wordspace, t_conf_encode encode, bool reorder, int where, int vertread);

extern p_text chapter_open_in_umd(const char *umdfile, const char *chaptername, u_int index, u32 rowpixels, u32 wordspace, t_conf_encode encode, bool reorder);

/**
 * 关闭文本
 *
 * @param fstext
 */
extern void text_close(p_text fstext);

/*
 * 得到文本当前行的显示长度，以像素计，系统菜单使用
 *
 * @param pos 文本起始位置
 * @param size 文件大小，以字节计
 * @param wordspace 字间距
 *
 * @return 显示长度，以像素计
 */
int text_get_string_width_sys(const u8 * pos, size_t size, u32 wordspace);

/**
 * 是否为不能截断字符
 *
 * @note 在显示英文判断单词是否应换行用到
 *
 * @param ch 字符
 *
 * @return 是否为不能截断字符
 */
bool is_untruncateable_chars(char ch);

/**
 * 字节表，根据表查值得到状态
 *
 * @note 0 - 字符不必换行
 * @note 1 - 字符使本行结束
 * @note 2 - 字符为空格，TAB
 */
extern bool bytetable[256];

#endif
