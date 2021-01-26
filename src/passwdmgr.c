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
#include <string.h>
#include <pspsdk.h>
#include <pspkernel.h>
#include "common/datatype.h"
#include "common/utils.h"
#include "buffer.h"
#include "passwdmgr.h"
#include "scene.h"
#include "strsafe.h"
#include "dbg.h"
#include "rc4.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

enum
{
	LINEBUF = 256,
};

static char g_read_buf[LINEBUF] __attribute__ ((aligned(64)));
static char *read_ptr = NULL;
static int read_cnt = 0;

static rc4_key g_key;

#define CRYPT_KEY "xReader_rc4_default_key"
#define CRYPT_MAGIC 0xC01DB12D

static int read_char(SceUID fd, char *c, rc4_key * key)
{
	u8 prg;

	if (read_cnt <= 0) {
		read_cnt = sceIoRead(fd, g_read_buf, LINEBUF);

		if (read_cnt < 0) {
			return read_cnt;
		}

		if (read_cnt == 0) {
			return read_cnt;
		}

		read_ptr = g_read_buf;
	}

	read_cnt--;

	if (key != NULL) {
		prg = rc4_prga(key);
		*c = *read_ptr++ ^ prg;
	} else {
		*c = *read_ptr++;
	}

	return 1;
}

/**
  * @return how many bytes we have read, or -1 when EOF
  */
static int read_lines(SceUID fd, char *lines, size_t linebuf_size, rc4_key * key)
{
	char *p;
	int ret;
	size_t re;

	if (linebuf_size == 0) {
		return -1;
	}

	p = lines;
	re = linebuf_size;

	while (re-- != 0) {
		ret = read_char(fd, p, key);

		if (ret < 0) {
			break;
		}

		if (ret == 0) {
			if (p == lines) {
				ret = -1;
			}

			break;
		}

		if (*p == '\r') {
			continue;
		}

		if (*p == '\n') {
			break;
		}

		p++;
	}

	if (p < lines + linebuf_size) {
		*p = '\0';
	}

	return ret >= 0 ? p - lines : ret;
}

typedef struct _password
{
	buffer *b;
	struct _password *next;
} password;

static password g_pwd_head = { NULL, NULL };

static password *g_pwd_tail = &g_pwd_head;

int add_password(const char *passwd)
{
	password *pwd;

	for (pwd = g_pwd_head.next; pwd != NULL; pwd = pwd->next) {
		if (0 == strcmp(passwd, pwd->b->ptr)) {
			return 0;
		}
	}

	pwd = (password *) malloc(sizeof(*pwd));

	if (pwd == NULL) {
		return -1;
	}

	pwd->b = buffer_init_string(passwd);
	pwd->next = NULL;
	g_pwd_tail->next = pwd;
	g_pwd_tail = pwd;

	return 0;
}

static int is_encrypted(const char *path)
{
	SceUID fd = -1;
	u32 magic;
	u32 result = 0;

	fd = sceIoOpen(path, PSP_O_RDONLY, 0);

	if (fd < 0) {
		goto exit;
	}

	if (sizeof(magic) != sceIoRead(fd, &magic, sizeof(magic))) {
		goto exit;
	}

	if (CRYPT_MAGIC == magic) {
		result = 1;
	}

  exit:
	if (fd >= 0) {
		sceIoClose(fd);
		fd = -1;
	}

	return result;
}

bool load_passwords(void)
{
	SceUID fd;
	char linebuf[LINEBUF], path[PATH_MAX];
	rc4_key *pkey;

	STRCPY_S(path, scene_appdir());
	STRCAT_S(path, "password.lst");

	if (is_encrypted(path)) {
		rc4_prepare_key((u8 *) CRYPT_KEY, sizeof(CRYPT_KEY) - 1, &g_key);
		pkey = &g_key;
	} else {
		pkey = NULL;
	}

	fd = sceIoOpen(path, PSP_O_RDONLY, 0);

	if (fd < 0) {
		return false;
	}

	if (pkey != NULL) {
		sceIoLseek(fd, 4, PSP_SEEK_SET);
	}

	linebuf[sizeof(linebuf) - 1] = '\0';

	while (read_lines(fd, linebuf, sizeof(linebuf) - 1, pkey) >= 0) {
		add_password(linebuf);
	}

	sceIoClose(fd);

	return true;
}

static int write_char(SceUID fd, char c)
{
	u8 prg;

	prg = rc4_prga(&g_key);
	c ^= prg;

	return sceIoWrite(fd, &c, 1);
}

static int write_chars(SceUID fd, char *ch, size_t n)
{
	int ret;
	size_t i;

	for (i = 0; i < n; ++i) {
		ret = write_char(fd, *ch++);

		if (ret != 1) {
			return ret;
		}
	}

	return n;
}

bool save_passwords(void)
{
	password *pwd;
	SceUID fd;
	char path[PATH_MAX];
	u32 magic;

	STRCPY_S(path, scene_appdir());
	STRCAT_S(path, "password.lst");

	rc4_prepare_key((u8 *) CRYPT_KEY, sizeof(CRYPT_KEY) - 1, &g_key);

	fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);

	if (fd < 0) {
		return false;
	}

	magic = CRYPT_MAGIC;
	sceIoWrite(fd, &magic, sizeof(magic));

	for (pwd = g_pwd_head.next; pwd != NULL; pwd = pwd->next) {
		write_chars(fd, pwd->b->ptr, strlen(pwd->b->ptr));
		write_chars(fd, "\r\n", sizeof("\r\n") - 1);
	}

	sceIoClose(fd);

	return true;
}

/*
 * @return next password after deleted password
 */
static password *free_password(password * pwd)
{
	password *next;

	buffer_free(pwd->b);
	pwd->b = NULL;
	next = pwd->next;
	free(pwd);

	return next;
}

void free_passwords(void)
{
	password *pwd;

	pwd = g_pwd_head.next;

	while (pwd != NULL) {
		pwd = free_password(pwd);
	}

	g_pwd_head.next = NULL;
	g_pwd_tail = &g_pwd_head;
}

size_t get_password_count(void)
{
	password *pwd;
	size_t i;

	pwd = g_pwd_head.next;
	i = 0;

	while (pwd != NULL) {
		pwd = pwd->next;
		i++;
	}

	return i;
}

buffer *get_password(int num)
{
	password *pwd;

	pwd = g_pwd_head.next;

	while (num-- > 0 && pwd != NULL) {
		pwd = pwd->next;
	}

	return pwd->b;
}
