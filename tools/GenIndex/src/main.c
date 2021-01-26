/*
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

/*
 * main.c
 *
 * Copyright (c) outmatch 2007
 *
 * You may distribute and modify this program under the terms of either
 * the GNU General Public License Version 3.
 */

#include <stdio.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#ifndef WIN32
#include <iconv.h>
#endif
#include "strsafe.h"
#include "config.h"
#include "dbg.h"

#if !(defined(MAGICTOKEN) && defined(SPLITTOKEN))
#error 没有定义MAGICTOKEN 或 SPLITTOKEN
#endif

DBG *d;

// 目录项
typedef struct
{
	char *name;
	size_t page;
} DirEntry;

typedef struct
{
	char *path;
	char *name;
	size_t size;
	size_t dirsize;
	size_t context;
	int newline_style;
} TxtFile;

DirEntry *AddDirEntry(const char *name, int page);
void ParseFile(void);
int PrintDir(FILE * fp);
int VPrintDir(void);
void FreeDirEntry(void);

TxtFile g_file;
DirEntry *g_dirs = 0;
size_t g_dircnt = 0;
size_t g_dircap = 0;

#if defined(WIN32) || defined(_MSC_VER)
#include <windows.h>
void GetTempFilename(char *str, size_t size)
{
	GetTempPath(size, str);
	GetTempFileName(str, ("GenIndex_"), 0, str);
}
#else
void GetTempFilename(char *str, size_t size)
{
	strcpy_s(str, size, "/tmp/genXXXXXX");
	const char *p = mktemp(str);
}
#endif

/* return 0 if newline is unix style, return 1 if newline is dos style, return -1 on error */
int detect_unix_dos(const char *fn)
{
	char buf[65536];
	unsigned cunix, cdos;
	FILE *fp;

	fp = fopen(fn, "rb");

	if (fp == NULL) {
		return -1;
	}

	cunix = cdos = 0;
	while (fgets(buf, sizeof(buf), fp) != NULL) {
		size_t len = strlen(buf);

		if (len >= 2 && buf[len - 1] == '\n' && buf[len - 2] == '\r') {
			cdos++;
		} else if (len >= 1 && buf[len - 1] == '\n') {
			cunix++;
		}
	}

	fclose(fp);

	return cdos > cunix;
}

char to_locale[20];

void set_output_locale(const char *p)
{
	if (strchr(p, '.')) {
		STRCPY_S(to_locale, strchr(p, '.') + 1);
	} else {
		STRCPY_S(to_locale, p);
	}
}

int conv_gbk_to_current_locale(const char *gbkstr, size_t size, char **ptr,
							   int *outputsize)
{
#ifdef WIN32
	*outputsize = size;
	*ptr = strdup(gbkstr);

	return 0;
#else
	iconv_t ic;
	int ret = 0;

	if (ptr == NULL) {
		return -1;
	}

	ic = iconv_open(to_locale, "GBK");

	if (ic == (iconv_t) (-1)) {
		return -1;
	}

	size_t len = size * 2;
	*ptr = malloc(len);

	if (*ptr == NULL) {
		ret = -1;
		goto exit;
	}

	char *p = (char *) gbkstr;
	char *q = *ptr;
	int left = size, oleft = len;

	while (left > 0 && (ret = (int) iconv(ic, &p, &left, &q, &oleft)) != -1) {
	}

	*q++ = '\0';

	*outputsize = q - *ptr;

	ret = 0;
  exit:
	iconv_close(ic);
	return ret;
#endif
}

static int err_msg(const char *fmt, ...)
{
	char buf[65536];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	buf[sizeof(buf) - 1] = '\0';

	char *t = NULL;
	int tsize;

	conv_gbk_to_current_locale(buf, strlen(buf), &t, &tsize);

	fprintf(stderr, "%s", t);

	free(t);

	va_end(ap);

	return 0;
}

int do_each_file(const char *fn)
{
	struct stat statbuf;

	if (stat(fn, &statbuf) != 0) {
		err_msg("文件%s没有找到", fn);
		return -1;
	} else {
		dbg_printf(d, "文件: %s 大小: %ld字节", fn, statbuf.st_size);
		g_file.name = strdup(fn);
		if (strrchr(g_file.name, '\\') != 0) {
			memmove(g_file.name, strrchr(g_file.name, '\\') + 1,
					strlen(strrchr(g_file.name, '\\') + 1) + 1);
		}
		g_file.path = strdup(fn);
		g_file.size = statbuf.st_size;
	}

	g_file.newline_style = detect_unix_dos(g_file.name);

	if (g_file.newline_style == 0) {
		dbg_printf(d, "%s: UNIX", g_file.name);
	} else if (g_file.newline_style == 1) {
		dbg_printf(d, "%s: DOS", g_file.name);
	} else {
		dbg_printf(d, "%s: Unknown, assume DOS", g_file.name);
		g_file.newline_style = 1;
	}

	ParseFile();
	FreeDirEntry();

	if (g_file.name != NULL) {
		free(g_file.name);
		g_file.name = NULL;
	}

	if (g_file.path != NULL) {
		free(g_file.path);
		g_file.path = NULL;
	}
}

int main(int argc, char *argv[])
{
	struct stat statbuf;
	int i;

	d = dbg_init();
	dbg_open_stream(d, stderr);
	dbg_switch(d, 0);

	if (getenv("LANG") != NULL) {
		set_output_locale(getenv("LANG"));
	}

	if (argc < 2) {
		err_msg("xReader 目录生成工具 GenIndex (version 0.1)\n");
		err_msg("用法: GenIndex.exe 文件名.txt [-d]\n");
		err_msg("                               -d: 调试模式\n");
		err_msg("使用方法\n");
		err_msg("1. 在你的TXT电子书里加入<<<<符号标注每章开始\n");
		err_msg("如\n");
		err_msg("<<<<第一章 XXX\n");
		err_msg("....\n");
		err_msg("<<<<第二章 YYY\n");
		err_msg
			("2. 保存后，将TXT拖到GenIndex图标中，GenIndex就会为你的电子书生成\n");
		err_msg("一个简便的目录：\n");
		err_msg("===================\n");
		err_msg("页数 目录\n");
		err_msg("1    第一章 XXX\n");
		err_msg("2    第二章 YYY\n");
		err_msg("===================\n");
		err_msg
			("3. 页数数字就是GI值，看书时在信息栏里有显示，如果你要移动到某章，翻页直到GI相等即可找到该章。GI是绝对页码，不会因为字体大小而改变。\n");
#ifdef WIN32
		system("pause");
#endif
		return -1;
	}

	for (i = 1; i < argc; ++i) {
#ifdef WIN32
		if (stricmp(argv[i], "-d") == 0) {
			dbg_switch(d, 1);
		}
#else
		if (strcasecmp(argv[i], "-d") == 0) {
			dbg_switch(d, 1);
		}
#endif
	}

	for (i = 1; i < argc; ++i) {
#ifdef WIN32
		if (stricmp(argv[i], "-d") == 0) {
			continue;
		}
#else
		if (strcasecmp(argv[i], "-d") == 0) {
			continue;
		}
#endif

		do_each_file(argv[i]);
	}

	dbg_close(d);
	return 0;
}

int GetCurPageNum(FILE * fp, int offset)
{
	size_t pos;
	pos = ftell(fp) + offset;
	if (pos / 1023 != 0) {
		return (int) ((double) (pos) / 1023.0 + 0.5);
	} else
		return 1;				/* 避免返回0页 */
}

void SearchDir(FILE * fp)
{
	int iSplitCnt = 0;
	char buf[65536];

	// 清除目录
	FreeDirEntry();

	fseek(fp, 0, SEEK_SET);

	// 找到目录后地址
	while (!feof(fp)) {
		if (fgets(buf, sizeof(buf) / sizeof(buf[0]), fp)) {
			size_t len = strlen(buf);

			if (len >= 2) {
				if (buf[len - 1] == '\n') {
					buf[len - 1] = '\0';
				}

				if (buf[len - 2] == '\r') {
					buf[len - 2] = '\0';
				}
			}

			if (strcmp(buf, SPLITTOKEN) == 0)
				iSplitCnt++;
		}
		if (iSplitCnt >= 2)
			break;
	}

	if (iSplitCnt == 0)
		g_file.context = 0;
	if (iSplitCnt >= 2) {
		g_file.context = ftell(fp);
	}

	dbg_printf(d, "正文开始于%d字节", g_file.context);

	fseek(fp, g_file.context, SEEK_SET);

	dbg_printf(d, "扫描文件,以%d字节偏移", g_file.dirsize);
	while (!feof(fp)) {
		if (fgets(buf, sizeof(buf) / sizeof(buf[0]), fp)) {
			size_t len = strlen(buf);

			if (len >= 2) {
				if (buf[len - 1] == '\n')
					buf[len - 1] = '\0';

				if (buf[len - 2] == '\r') {
					buf[len - 2] = '\0';
				}
			}

			if (strstr(buf, MAGICTOKEN) != 0) {
				char *p;
				dbg_printf(d, "查找到标记: %s", buf);

				p = strstr(buf, MAGICTOKEN) + strlen(MAGICTOKEN);
				AddDirEntry(p,
							GetCurPageNum(fp, g_file.dirsize - g_file.context));
				dbg_printf(d, "添加记录项到%d个(名字: %s 页数 %d)", g_dircnt,
						   g_dirs[g_dircnt - 1].name,
						   g_dirs[g_dircnt - 1].page);
			}
		}
	}
}

static int copy_file(const char *srcfile, const char *dstfile)
{
	FILE *fout, *fin;
	char buf[BUFSIZ];

	fout = fopen(dstfile, "wb");

	if (fout == NULL)
		return -1;

	fin = fopen(srcfile, "rb");

	if (fin == NULL) {
		fclose(fout);
		return -1;
	}

	size_t r;

	while ((r = fread(buf, 1, sizeof(buf), fin)) != 0) {
		if (fwrite(buf, 1, r, fout) != r) {
			fclose(fin);
			fclose(fout);
			return -1;
		}
	}

	fclose(fin);
	fclose(fout);

	return 0;
}

void ParseFile(void)
{
	int r;
	char buf[BUFSIZ];
	FILE *fp, *foutp;
	char *szOutFn = 0;
	dbg_printf(d, "处理TXT文件: 大小 %d", g_file.size);
	fp = fopen(g_file.path, "rb");
	char str[4];
	if (fgets(str, 4, fp) == NULL) {
		return;
	}

	if (str[0] == '\xef' && str[1] == '\xbb' && str[2] == '\xbf') {
		err_msg("GenIndex不能处理UTF8 TXT文件，请先转换为GBK编码\n");
#ifdef WIN32
		system("pause");
#endif
		return;
	}

	g_file.dirsize = 0;
	fseek(fp, 0, SEEK_SET);
	SearchDir(fp);

	g_file.dirsize = VPrintDir();
	dbg_printf(d, "VPrintDir 测试目录大小为%d字节", g_file.dirsize);

	SearchDir(fp);

	szOutFn = (char *) malloc(MAX_PATH + 1);
	GetTempFilename(szOutFn, MAX_PATH);
	dbg_printf(d, "得到临时文件名%s", szOutFn);
	foutp = fopen(szOutFn, "wb");
	if (!foutp) {
		dbg_printf(d, "创建文件%s失败", szOutFn);
		return;
	}
	PrintDir(foutp);
	// 打印其他内容
	fseek(fp, g_file.context, SEEK_SET);

	while ((r = fread(buf, 1, sizeof(buf), fp)) != 0) {
		if (fwrite(buf, 1, r, foutp) != r) {
			break;
		}
	}

	fclose(fp);
	fclose(foutp);
#ifdef WIN32
	DeleteFile(g_file.path);
#else
	unlink(g_file.path);
#endif
#ifdef WIN32
	MoveFile(szOutFn, g_file.path);
#else
	int ret = rename(szOutFn, g_file.path);
	if (ret != 0) {
		if (errno == EXDEV) {
			ret = copy_file(szOutFn, g_file.path);
			if (ret != 0) {
				err_msg("copy_file failed\n");
				return;
			}
			unlink(szOutFn);
		} else {
			err_msg("rename failed\n");
			return;
		}
	}
#endif
	dbg_printf(d, "目录大小%d字节 完成生成\n", g_file.dirsize);
	free(szOutFn);
}

int VPrintDirDOS()
{
	int i, bytes;
	char buf[BUFSIZ];
	if (g_dircnt == 0)
		return 0;
	bytes = SPRINTF_S(buf, "%s\r\n", SPLITTOKEN);
	bytes += SPRINTF_S(buf, "页数 目录\r\n");
	for (i = 0; i < g_dircnt; ++i) {
		bytes += SPRINTF_S(buf, "%-4d %s\r\n", g_dirs[i].page, g_dirs[i].name);
	}
	bytes += SPRINTF_S(buf, "%s\r\n", SPLITTOKEN);
	return bytes;
}

int PrintDirDOS(FILE * fp)
{
	int i, bytes;
	dbg_printf(d, "开始打印目录:");
	if (g_dircnt == 0)
		return 0;
	bytes = fprintf(fp, "%s\r\n", SPLITTOKEN);
	bytes += fprintf(fp, "页数 目录\r\n");
	for (i = 0; i < g_dircnt; ++i) {
		bytes += fprintf(fp, "%-4d %s\r\n", g_dirs[i].page, g_dirs[i].name);
	}
	bytes += fprintf(fp, "%s\r\n", SPLITTOKEN);
	return bytes;
}

int PrintDirUNIX(FILE * fp)
{
	int i, bytes;
	dbg_printf(d, "开始打印目录:");
	if (g_dircnt == 0)
		return 0;
	bytes = fprintf(fp, "%s\n", SPLITTOKEN);
	bytes += fprintf(fp, "页数 目录\n");
	for (i = 0; i < g_dircnt; ++i) {
		bytes += fprintf(fp, "%-4d %s\n", g_dirs[i].page, g_dirs[i].name);
	}
	bytes += fprintf(fp, "%s\n", SPLITTOKEN);
	return bytes;
}

int VPrintDirUNIX()
{
	int i, bytes;
	char buf[BUFSIZ];
	if (g_dircnt == 0)
		return 0;
	bytes = SPRINTF_S(buf, "%s\n", SPLITTOKEN);
	bytes += SPRINTF_S(buf, "页数 目录\n");
	for (i = 0; i < g_dircnt; ++i) {
		bytes += SPRINTF_S(buf, "%-4d %s\n", g_dirs[i].page, g_dirs[i].name);
	}
	bytes += SPRINTF_S(buf, "%s\n", SPLITTOKEN);
	return bytes;
}

int VPrintDir(void)
{
	if (g_file.newline_style == 0) {
		return VPrintDirUNIX();
	} else {
		return VPrintDirDOS();
	}
}

int PrintDir(FILE * fp)
{
	if (g_file.newline_style == 0) {
		return PrintDirUNIX(fp);
	} else {
		return PrintDirDOS(fp);
	}
}

DirEntry *AddDirEntry(const char *name, int page)
{
	DirEntry *p;

	dbg_printf(d, "g_dircnt %d g_dircap %d", g_dircnt, g_dircap);
	if (g_dirs == NULL) {
		g_dirs = (DirEntry *) malloc(sizeof(DirEntry));
		g_dirs->name = strdup(name);
		g_dirs->page = page;
		g_dircap = 1;
	} else {
		if (g_dircnt >= g_dircap) {
			g_dircap += 16;
			p = (DirEntry *) realloc(g_dirs, sizeof(DirEntry) * g_dircap);

			if (p != NULL) {
				g_dirs = p;
			} else {
				return NULL;
			}
		}
		g_dirs[g_dircnt].name = strdup(name);
		g_dirs[g_dircnt].page = page;
	}
	g_dircnt++;
	return g_dirs;
}

void FreeDirEntry(void)
{
	int i;

	if (g_dirs == NULL || g_dircnt == 0)
		return;
	for (i = 0; i < g_dircnt; ++i) {
		free(g_dirs[i].name);
	}
	free(g_dirs);
	g_dircnt = 0;
	g_dirs = NULL;
}
