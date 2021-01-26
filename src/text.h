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
	/** �ļ�·���� */
	char filename[PATH_MAX];

	u32 crow;

	u32 size;

	char *buf;

	/** �ı���Unicode����ģʽ
	 *
	 * @note 0 - ��Unicode����
	 * @note 1 - UCS
	 * @note 2 - UTF-8
	 */
	int ucs;

	/** ���� */
	u32 row_count;

	/** �нṹָ������ */
	p_textrow rows[1024];
} t_text, *p_text;

/**
 * ��ʽ���ı�
 *
 * @note ����ʾģʽ���ı����Ϊ��
 *
 * @param txt ������ṹָ��
 * @param max_pixels �����ʾ���ȣ������ؼ�
 * @param wordspace �ּ��
 * @param ttf_mode �Ƿ�ΪTTF��ʾģʽ
 *
 * @return �Ƿ�ɹ�
 */
extern bool text_format(p_text txt, u32 max_pixels, u32 wordspace, bool ttf_mode);

/**
 * ���ڴ���һ��������Ϊ�ı�
 *
 * @note �ڴ����ݽ�������
 *
 * @param filename ����;
 * @param data
 * @param size
 * @param ft �ļ�����
 * @param max_pixels �����ʾ���ȣ������ؼ�
 * @param wordspace �ּ��
 * @param encode �ı���������
 * @param reorder �ı��Ƿ����±���
 *
 * @return �µĵ�����ṹָ��
 */
extern p_text text_open_in_raw(const char *filename, const unsigned char *data,
							   size_t size, t_fs_filetype ft, u32 max_pixels, u32 wordspace, t_conf_encode encode, bool reorder);

/**
 * ���ı��ļ��������ڵ����ļ�
 * 
 * @note ֧��GZ,CHM,RAR,ZIP�Լ�TXT
 *
 * @param filename �ļ�·��
 * @param archname ����·��
 * @param filetype �ı��ļ�����
 * @param max_pixels �����ʾ���ȣ������ؼ� 
 * @param wordspace �־�
 * @param encode �ı��ļ�����
 * @param reorder �Ƿ����±���
 * @param where ��������
 * @param vertread ��ʾ��ʽ
 *
 * @return �µĵ�����ṹָ��
 * - NULL ʧ��
 */
extern p_text text_open_archive(const char *filename,
								const char *archname,
								t_fs_filetype filetype, u32 max_pixels, u32 wordspace, t_conf_encode encode, bool reorder, int where, int vertread);

extern p_text chapter_open_in_umd(const char *umdfile, const char *chaptername, u_int index, u32 rowpixels, u32 wordspace, t_conf_encode encode, bool reorder);

/**
 * �ر��ı�
 *
 * @param fstext
 */
extern void text_close(p_text fstext);

/*
 * �õ��ı���ǰ�е���ʾ���ȣ������ؼƣ�ϵͳ�˵�ʹ��
 *
 * @param pos �ı���ʼλ��
 * @param size �ļ���С�����ֽڼ�
 * @param wordspace �ּ��
 *
 * @return ��ʾ���ȣ������ؼ�
 */
int text_get_string_width_sys(const u8 * pos, size_t size, u32 wordspace);

/**
 * �Ƿ�Ϊ���ܽض��ַ�
 *
 * @note ����ʾӢ���жϵ����Ƿ�Ӧ�����õ�
 *
 * @param ch �ַ�
 *
 * @return �Ƿ�Ϊ���ܽض��ַ�
 */
bool is_untruncateable_chars(char ch);

/**
 * �ֽڱ������ݱ���ֵ�õ�״̬
 *
 * @note 0 - �ַ����ػ���
 * @note 1 - �ַ�ʹ���н���
 * @note 2 - �ַ�Ϊ�ո�TAB
 */
extern bool bytetable[256];

#endif