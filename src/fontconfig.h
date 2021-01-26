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

#ifndef XR_FONTCONFIG_H
#define XR_FONTCONFIG_H

#include "common/datatype.h"

typedef struct _font_config
{
	const char *fontname;
	bool globaladvance;
	bool antialias;
	bool hinting;
	int hintstyle;
	bool autohint;
	int pixelsize;
	bool embolden;
	bool cleartype;
	int lcdfilter;
	bool cjkmode;
} font_config;

enum
{
	TOKEN_IF,					/* if */
	TOKEN_THEN,					/* then */
	TOKEN_ELSE,					/* else */
	TOKEN_END,					/* end */
	TOKEN_EQUAL,				/* = */
	TOKEN_COMP,					/* == */
	TOKEN_NOT,					/* ! */
	TOKEN_NOTEQUAL,				/* != */
	TOKEN_GT,					/* > */
	TOKEN_LT,					/* < */
	TOKEN_NOTLT,				/* >= */
	TOKEN_NOTGT,				/* <= */
	TOKEN_BRACKET,				/* () */
	TOKEN_AND,					/* AND */
	TOKEN_OR,					/* OR */
	TOKEN_VAR,					/* Variable name */
	TOKEN_BOOL,					/* true/false */
	TOKEN_NUM,					/* 0123 */
	TOKEN_STRING,				/* "Hello world" */
};

enum
{
	TOKEN_BRACKET_LEFT = 0,		/* ( */
	TOKEN_BRACKET_RIGHT			/* ) */
};

enum
{
	TOKEN_COMPARE_FALSE = 0,
	TOKEN_COMPARE_TRUE
};

typedef struct _token
{
	int type;

	void *value;
	size_t value_size;
} token;

typedef struct _fontconfig_mgr
{
	font_config *cache;
	size_t cnt;
	size_t caps;
} fontconfig_mgr;

int new_font_config(const char *fontname, font_config * p, bool cjkmode);
int get_font_config(font_config * p);
int report_font_config(font_config * p);

fontconfig_mgr *fontconfigmgr_init(void);
font_config *fontconfigmgr_add_cache(fontconfig_mgr * font_mgr, font_config * p_cfg);
font_config *fontconfigmgr_lookup(fontconfig_mgr * font_mgr, const char *font_name, int pixelsize, bool cjkmode);
void fontconfigmgr_free(fontconfig_mgr * font_mgr);

#endif
