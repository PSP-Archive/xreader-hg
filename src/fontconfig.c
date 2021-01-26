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

#include <pspkernel.h>
#include <psppower.h>
#include <pspsdk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "config.h"
#include "dbg.h"
#include "strsafe.h"
#include "common/utils.h"
#include "fontconfig.h"
#include "scene.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

static char g_font_config_path[PATH_MAX];

int new_font_config(const char *fontname, font_config * p, bool cjkmode)
{
	if (fontname == NULL)
		return -1;

	memset(p, 0, sizeof(*p));
	p->fontname = fontname;
	p->antialias = true;
	p->cleartype = true;
	p->lcdfilter = 1;
	p->cjkmode = cjkmode;
	p->hinting = !p->cjkmode;

	return 0;
}

static int skip_space(const char *buf, size_t size, const char **p)
{
	if (*p < buf || *p >= buf + size) {
		return -1;
	}

	while (isspace(**p) && *p < buf + size) {
		(*p)++;
	}

	return *p - buf;
}

static int skip_token(const char *buf, size_t size, const char **p)
{
	if (*p < buf || *p >= buf + size) {
		return -1;
	}

	while (!isspace(**p) && *p < buf + size) {
		(*p)++;
	}

	return *p - buf;
}

static int get_token(const char *linebuf, size_t size, const char *p, char *buf, size_t bufsize)
{
	size_t pos;
	const char *q = p;
	size_t tsize;

	pos = p - linebuf;

	if (pos >= size)
		return -1;

	tsize = skip_token(linebuf, size, &q) - pos;

	return strncpy_s(buf, bufsize, &linebuf[pos], tsize);
}

static int get_next_quote(const char *linebuf, size_t size, const char *p)
{
	if (p < linebuf || p >= linebuf + size)
		return -1;

	p = strchr(p, '"');

	if (p < linebuf || p >= linebuf + size)
		return -1;

	return (int) (p - linebuf);
}

static int add_token(token ** th, size_t * tsize, token * t)
{
	if (*th == NULL) {
		*th = malloc(sizeof(**th));
		(*tsize) = 1;
	} else {
		*th = safe_realloc(*th, sizeof(**th) * (*tsize + 1));
		(*tsize)++;
	}

	if (*th == NULL) {
		return -1;
	}

	memcpy(&(*th)[(*tsize) - 1], t, sizeof(**th));

	return 0;
}

static int add_token_quote(token ** th, size_t * tsize, const char *tokenstring)
{
	token t;

	memset(&t, 0, sizeof(t));

	t.type = TOKEN_STRING;
	t.value = strdup(tokenstring);
	t.value_size = strlen(tokenstring) + 1;

	return add_token(th, tsize, &t);
}

static bool is_str_digit(const char *str)
{
	size_t len;
	size_t i;
	bool has_dot = false;

	len = strlen(str);

	for (i = 0; i < len; ++i) {
		if (has_dot) {
			if (str[i] == '.') {
				return false;
			}
		} else {
			if (str[i] == '.') {
				has_dot = true;
				continue;
			}
		}

		if (!(isspace(str[i]) || isdigit(str[i]))) {
			return false;
		}
	}

	return true;
}

static int add_token_keyword(token ** th, size_t * tsize, const char *tokenstring)
{
	token t;

	memset(&t, 0, sizeof(t));

	if (!stricmp(tokenstring, "if")) {
		t.type = TOKEN_IF;
	} else if (!stricmp(tokenstring, "then")) {
		t.type = TOKEN_THEN;
	} else if (!stricmp(tokenstring, "else")) {
		t.type = TOKEN_ELSE;
	} else if (!stricmp(tokenstring, "end")) {
		t.type = TOKEN_END;
	} else if (!stricmp(tokenstring, "==")) {
		t.type = TOKEN_COMP;
	} else if (!stricmp(tokenstring, "and")) {
		t.type = TOKEN_AND;
	} else if (!stricmp(tokenstring, "or")) {
		t.type = TOKEN_OR;
	} else if (!stricmp(tokenstring, "(")) {
		t.type = TOKEN_BRACKET;
		t.value = (void *) TOKEN_BRACKET_LEFT;
	} else if (!stricmp(tokenstring, ")")) {
		t.type = TOKEN_BRACKET;
		t.value = (void *) TOKEN_BRACKET_RIGHT;
	} else if (!stricmp(tokenstring, "=")) {
		t.type = TOKEN_EQUAL;
	} else if (!stricmp(tokenstring, "!")) {
		t.type = TOKEN_NOT;
	} else if (!stricmp(tokenstring, "!=")) {
		t.type = TOKEN_NOTEQUAL;
	} else if (!stricmp(tokenstring, ">=")) {
		t.type = TOKEN_NOTLT;
	} else if (!stricmp(tokenstring, ">")) {
		t.type = TOKEN_GT;
	} else if (!stricmp(tokenstring, "<")) {
		t.type = TOKEN_LT;
	} else if (!stricmp(tokenstring, "<=")) {
		t.type = TOKEN_NOTGT;
	} else if (!stricmp(tokenstring, "true")) {
		t.type = TOKEN_BOOL;
		t.value = (void *) 1;
	} else if (!stricmp(tokenstring, "false")) {
		t.type = TOKEN_BOOL;
		t.value = (void *) 0;
	} else if (is_str_digit(tokenstring)) {
		t.type = TOKEN_NUM;
		t.value = (void *) strtol(tokenstring, NULL, 10);
	} else {
		t.type = TOKEN_VAR;
		t.value = strdup(tokenstring);
		t.value_size = strlen(tokenstring) + 1;
	}

	return add_token(th, tsize, &t);
}

int scan_tokens(char *buf, token ** t, size_t * tsize)
{
	bool inquote = false;
	bool intoken = false;

	size_t len = strlen(buf);
	size_t i;

	for (i = 0; i < len; ++i) {
		if (buf[i] == '\"') {
			inquote = !inquote;
			continue;
		}

		if (!inquote && buf[i] == '#') {
			// encount comment, abort
			return 0;
		}

		if (inquote) {
			char tokenname[80];
			const char *p = buf + i;
			int next_quote;

			next_quote = get_next_quote(buf, len, p);

			if (next_quote < 0) {
				dbg_printf(d, "%s: Missing end quote", __func__);

				return -1;
			}

			strncpy_s(tokenname, sizeof(tokenname), &buf[i], next_quote - i);
//          dbg_printf(d, "token is %s", tokenname);
			i = next_quote - 1;
			add_token_quote(t, tsize, tokenname);
		} else {
			if (!isspace(buf[i])) {
				if (!intoken) {
					char tokenname[80];
					const char *p = buf + i;

					get_token(buf, len, p, tokenname, sizeof(tokenname));

//                  dbg_printf(d, "token is %s", tokenname);
					add_token_keyword(t, tsize, tokenname);
				}

				intoken = true;
			} else {
				const char *p = buf + i;

				if (skip_space(buf, len, &p) >= 0) {
					if (p >= buf + 1 && p < buf + len) {
						i = p - buf - 1;
					}
				}

				intoken = false;
			}
		}
	}

	return 0;
}

void report_tokens(token * t, size_t size)
{
	size_t i;

	dbg_printf(d, "Dump all tokens input:");

	for (i = 0; i < size; ++i) {
		switch (t[i].type) {
			case TOKEN_IF:
				dbg_printf(d, "IF");
				break;
			case TOKEN_THEN:
				dbg_printf(d, "THEN");
				break;
			case TOKEN_END:
				dbg_printf(d, "END");
				break;
			case TOKEN_NOT:
				dbg_printf(d, "!");
				break;
			case TOKEN_AND:
				dbg_printf(d, "and");
				break;
			case TOKEN_OR:
				dbg_printf(d, "or");
				break;
			case TOKEN_BRACKET:
				dbg_printf(d, "%s", ((int) t[i].value) == TOKEN_BRACKET_LEFT ? "(" : ")");
				break;
			case TOKEN_NOTEQUAL:
				dbg_printf(d, "!=");
				break;
			case TOKEN_COMP:
				dbg_printf(d, "==");
				break;
			case TOKEN_EQUAL:
				dbg_printf(d, "=");
				break;
			case TOKEN_LT:
				dbg_printf(d, "<");
				break;
			case TOKEN_GT:
				dbg_printf(d, ">");
				break;
			case TOKEN_NOTLT:
				dbg_printf(d, ">=");
				break;
			case TOKEN_NOTGT:
				dbg_printf(d, "<=");
				break;
			case TOKEN_BOOL:
				dbg_printf(d, "%s", (int) (t[i].value) ? "true" : "false");
				break;
			case TOKEN_STRING:
				dbg_printf(d, "\"%s\"", (const char *) t[i].value);
				break;
			case TOKEN_VAR:
				dbg_printf(d, "$%s", (const char *) t[i].value);
				break;
			case TOKEN_NUM:
				dbg_printf(d, "%d", (int) t[i].value);
				break;
		}
	}
}

static int get_var_as_int(font_config * cfg, const token * t, int **interge)
{
	const char *p;

	if (t->type != TOKEN_VAR) {
		return -1;
	}

	p = (const char *) t->value;

	if (!stricmp(p, "pixelsize")) {
		*interge = &cfg->pixelsize;
	} else if (!stricmp(p, "hintstyle")) {
		*interge = &cfg->hintstyle;
	} else if (!stricmp(p, "lcdfilter")) {
		*interge = &cfg->lcdfilter;
	} else {
		return -1;
	}

	return 0;
}

static int get_var_as_bool(font_config * cfg, const token * t, bool ** boolean)
{
	const char *p;

	if (t->type != TOKEN_VAR) {
		return -1;
	}

	p = (const char *) t->value;

	if (!stricmp(p, "globaladvance")) {
		*boolean = &cfg->globaladvance;
	} else if (!stricmp(p, "antialias")) {
		*boolean = &cfg->antialias;
	} else if (!stricmp(p, "hinting")) {
		*boolean = &cfg->hinting;
	} else if (!stricmp(p, "autohint")) {
		*boolean = &cfg->autohint;
	} else if (!stricmp(p, "cleartype")) {
		*boolean = &cfg->cleartype;
	} else if (!stricmp(p, "embolden")) {
		*boolean = &cfg->embolden;
	} else if (!stricmp(p, "cjkmode")) {
		*boolean = &cfg->cjkmode;
	} else {
		return -1;
	}

	return 0;
}

static int get_var_as_string(font_config * cfg, const token * t, const char **string)
{
	const char *p;

	if (t->type != TOKEN_VAR) {
		return -1;
	}

	p = (const char *) t->value;

	if (!stricmp(p, "fontname")) {
		*string = cfg->fontname;
	} else {
		return -1;
	}

	return 0;
}

static int token_compare(font_config * cfg, token * t, size_t tsize, size_t i)
{
	int ret = TOKEN_COMPARE_FALSE;
	size_t left;
	size_t right;

	if (i < 1 || i + 1 > tsize - 1) {
		dbg_printf(d, "%s: Missing operand", __func__);
		return -1;
	}

	left = i - 1;
	right = i + 1;

	switch (t[left].type) {
#if 0
		case TOKEN_NUM:
			if (t[right].type == TOKEN_NUM) {
				ret = ((int) t[left].value == (int) t[right].value) ? TOKEN_COMPARE_TRUE : TOKEN_COMPARE_FALSE;
			} else if (t[right].type == TOKEN_VAR) {
				int *rightvalue;

				ret = get_var_as_int(cfg, &t[right], &rightvalue);

				if (ret == 0) {
					ret = ((int) t[left].value == *rightvalue) ? TOKEN_COMPARE_TRUE : TOKEN_COMPARE_FALSE;
				}
			}
			break;
		case TOKEN_STRING:
			break;
		case TOKEN_BOOL:
			if (t[right].type == TOKEN_BOOL) {
				ret = ((bool) (int) t[left].value == (bool) (int) t[right].value) ? TOKEN_COMPARE_TRUE : TOKEN_COMPARE_FALSE;
			} else if (t[right].type == TOKEN_VAR) {
				bool *rightvalue;

				ret = get_var_as_bool(cfg, &t[right], &rightvalue);

				if (ret == 0) {
					ret = ((bool) (int) t[left].value == *rightvalue) ? TOKEN_COMPARE_TRUE : TOKEN_COMPARE_FALSE;
				}
			}
			break;
#endif
		case TOKEN_VAR:
			{
				switch (t[right].type) {
					case TOKEN_NUM:
						{
							int *leftvalue;

							ret = get_var_as_int(cfg, &t[left], &leftvalue);

							if (ret == 0) {
								ret = ((int) t[right].value == *leftvalue) ? TOKEN_COMPARE_TRUE : TOKEN_COMPARE_FALSE;
							}
						}
						break;
					case TOKEN_BOOL:
						{
							bool *leftvalue;

							ret = get_var_as_bool(cfg, &t[left], &leftvalue);

							if (ret == 0) {
								ret = ((bool) (int) t[right].value == *leftvalue) ? TOKEN_COMPARE_TRUE : TOKEN_COMPARE_FALSE;
							}
						}
						break;
					case TOKEN_STRING:
						{
							const char *leftvalue;

							ret = get_var_as_string(cfg, &t[left], &leftvalue);

							if (ret == 0) {
								if (!strcmp((const char *) t[right].value, leftvalue)) {
									ret = TOKEN_COMPARE_TRUE;
								} else {
									ret = TOKEN_COMPARE_FALSE;
								}
							}
						}
						break;
				}
			}
			break;
	}

	return ret;
}

static int token_notcompare(font_config * cfg, token * t, size_t tsize, size_t i)
{
	return !token_compare(cfg, t, tsize, i);
}

static int token_gt(font_config * cfg, token * t, size_t tsize, size_t i)
{
	int ret = TOKEN_COMPARE_FALSE;
	size_t left;
	size_t right;

	if (i < 1 || i + 1 > tsize - 1) {
		dbg_printf(d, "%s: Missing operand", __func__);
		return -1;
	}

	left = i - 1;
	right = i + 1;

	switch (t[left].type) {
		case TOKEN_VAR:
			{
				switch (t[right].type) {
					case TOKEN_NUM:
						{
							int *leftvalue;

							ret = get_var_as_int(cfg, &t[left], &leftvalue);

							if (ret == 0) {
								ret = (*leftvalue > (int) t[right].value) ? TOKEN_COMPARE_TRUE : TOKEN_COMPARE_FALSE;
							}
						}
						break;
				}
			}
			break;
	}

	return ret;
}

static int token_ngt(font_config * cfg, token * t, size_t tsize, size_t i)
{
	return !token_gt(cfg, t, tsize, i);
}

static int token_lt(font_config * cfg, token * t, size_t tsize, size_t i)
{
	int ret = TOKEN_COMPARE_FALSE;
	size_t left;
	size_t right;

	if (i < 1 || i + 1 > tsize - 1) {
		dbg_printf(d, "%s: Missing operand", __func__);
		return -1;
	}

	left = i - 1;
	right = i + 1;

	switch (t[left].type) {
		case TOKEN_VAR:
			{
				switch (t[right].type) {
					case TOKEN_NUM:
						{
							int *leftvalue;

							ret = get_var_as_int(cfg, &t[left], &leftvalue);

							if (ret == 0) {
								ret = (*leftvalue < (int) t[right].value) ? TOKEN_COMPARE_TRUE : TOKEN_COMPARE_FALSE;
							}
						}
						break;
				}
			}
			break;
	}

	return ret;
}

static int token_nlt(font_config * cfg, token * t, size_t tsize, size_t i)
{
	return !token_lt(cfg, t, tsize, i);
}

static int token_equal(font_config * cfg, token * t, size_t tsize, size_t i)
{
	size_t left, right;

	if (i < 1 || i + 1 > tsize - 1) {
		dbg_printf(d, "%s: Missing operand", __func__);
		return -1;
	}

	left = i - 1;
	right = i + 1;

	if (t[left].type != TOKEN_VAR) {
		dbg_printf(d, "Error left value");
		return -1;
	}

	switch (t[right].type) {
		case TOKEN_BOOL:
			{
				bool *boolean = NULL;

				int ret = get_var_as_bool(cfg, &t[left], &boolean);

				if (ret == 0) {
					*boolean = (bool) (int) t[right].value;
				} else {
					return -1;
				}
			}
			break;
		case TOKEN_NUM:
			{
				int *interge = NULL;

				int ret = get_var_as_int(cfg, &t[left], &interge);

				if (ret == 0) {
					*interge = (int) t[right].value;
				} else {
					return -1;
				}
			}
			break;
		case TOKEN_STRING:
			{
				int *interge = NULL;

				int ret = get_var_as_int(cfg, &t[left], &interge);

				if (ret < 0) {
					return -1;
				}

				if (!stricmp((const char *) t[left].value, "hintstyle")) {
					if (!stricmp((const char *) t[right].value, "hintnone")) {
						*interge = 0;
					} else if (!stricmp((const char *) t[right].value, "hintslight")) {
						*interge = 1;
					} else if (!stricmp((const char *) t[right].value, "hintmedium")) {
						*interge = 2;
					} else if (!stricmp((const char *) t[right].value, "hintfull")) {
						*interge = 3;
					}
				} else if (!stricmp((const char *) t[left].value, "lcdfilter")) {
					if (!stricmp((const char *) t[right].value, "lcdnone")) {
						*interge = 0;
					} else if (!stricmp((const char *) t[right].value, "lcddefault")) {
						*interge = 1;
					} else if (!stricmp((const char *) t[right].value, "lcdlight")) {
						*interge = 2;
					} else if (!stricmp((const char *) t[right].value, "lcdlegacy")) {
						*interge = 3;
					}
				} else {
					dbg_printf(d, "Unsupported now");
					return -1;
				}
			}

			return 0;
			break;
	}

	return 0;
}

int parse_tokens(font_config * cfg, token * t, size_t tsize, bool boolean_mode)
{
	size_t i;
	size_t start = 0, end = 0;
	int ret = 0;
	int level = 0;

	if (boolean_mode) {
		// search for AND or OR
		for (i = 0; i < tsize; ++i) {
			if (t[i].type == TOKEN_AND || t[i].type == TOKEN_OR) {
				break;
			}
		}

		if (i == tsize) {
			return parse_tokens(cfg, t, tsize, false);
		}

		if (t[i].type == TOKEN_AND) {
			int left, right;

			left = parse_tokens(cfg, &t[0], i, true);
			right = parse_tokens(cfg, &t[i + 1], tsize - i - 1, true);

			return left && right;
		} else if (t[i].type == TOKEN_OR) {
			int left, right;

			left = parse_tokens(cfg, &t[0], i, true);
			right = parse_tokens(cfg, &t[i + 1], tsize - i - 1, true);

			return left || right;
		}
	} else {
		size_t j;
		int elsepos;

		for (i = 0; i < tsize; ++i) {
			switch (t[i].type) {
				case TOKEN_IF:
					start = i + 1;

					// jump to then
					{
						for (j = start; j < tsize; ++j) {
							if (t[j].type == TOKEN_THEN || t[j].type == TOKEN_THEN || t[j].type == TOKEN_END) {
								break;
							}
						}

						if (j == tsize || t[j].type == TOKEN_IF || t[j].type == TOKEN_END) {
							dbg_printf(d, "Missing then");
							return -1;
						}

						i = j - 1;
						continue;
					}

					break;
				case TOKEN_THEN:
					end = i - 1;
					ret = parse_tokens(cfg, &t[start], end - start + 1, true);

					if (ret == -1) {
						return ret;
					}

					start = i + 1;

					// search for END
					// note stack use

					level = 1;

					for (j = start; j < tsize; ++j) {
						if (t[j].type == TOKEN_IF) {
							level++;
						} else if (t[j].type == TOKEN_END) {
							if (--level <= 0) {
								end = j - 1;
								break;
							}
						}
					}

					if (j == tsize) {
						dbg_printf(d, "Missing end");
						return -1;
					}

					elsepos = -1;
					level = 1;

					// search for ELSE
					for (j = start; j < end; ++j) {
						if (t[j].type == TOKEN_IF) {
							level++;
						} else if (t[j].type == TOKEN_END) {
							if (--level <= 0) {
								break;
							}
						} else if (t[j].type == TOKEN_ELSE) {
							if (level == 1) {
								elsepos = j;
								break;
							}
						}
					}

					if (ret == TOKEN_COMPARE_TRUE) {
						if (elsepos >= 0) {
							ret = parse_tokens(cfg, &t[start], elsepos - start, false);
						} else {
							ret = parse_tokens(cfg, &t[start], end - start + 1, false);
						}

						i = end + 1;
					} else {
						if (elsepos >= 0) {
							ret = parse_tokens(cfg, &t[elsepos + 1], end - elsepos, false);
						}

						i = end + 1;
					}

					break;
				case TOKEN_COMP:
					ret = token_compare(cfg, t, tsize, i);
					i++;
					break;
				case TOKEN_LT:
					ret = token_lt(cfg, t, tsize, i);
					i++;
					break;
				case TOKEN_GT:
					ret = token_gt(cfg, t, tsize, i);
					i++;
					break;
				case TOKEN_NOTLT:
					ret = token_nlt(cfg, t, tsize, i);
					i++;
					break;
				case TOKEN_NOTGT:
					ret = token_ngt(cfg, t, tsize, i);
					i++;
					break;
				case TOKEN_NOTEQUAL:
					ret = token_notcompare(cfg, t, tsize, i);
					i++;
					break;
				case TOKEN_EQUAL:
					ret = token_equal(cfg, t, tsize, i);
					i++;
					break;
				case TOKEN_NUM:
				case TOKEN_BOOL:
					ret = (int) t[i].value;
					break;
				case TOKEN_VAR:
				case TOKEN_NOT:
				case TOKEN_STRING:
				case TOKEN_BRACKET:
					//              report_tokens(&t[i], 1);
					break;
				default:
					dbg_printf(d, "Unexcepted token");
					report_tokens(&t[i], 1);
					return -1;
					break;
			}
		}
	}

	return ret;
}

int destroy_token(token ** t, size_t * tsize)
{
	size_t i;

	for (i = 0; i < *tsize; ++i) {
		if ((*t)[i].type == TOKEN_VAR || (*t)[i].type == TOKEN_STRING) {
			if ((*t)[i].value != NULL) {
				free((*t)[i].value);
				(*t)[i].value = NULL;
			}
		}
	}

	free(*t);
	*t = NULL;
	*tsize = 0;

	return 0;
}

fontconfig_mgr *fontconfigmgr_init(void)
{
	fontconfig_mgr *font_mgr;

	font_mgr = malloc(sizeof(*font_mgr));

	if (font_mgr == NULL) {
		return NULL;
	}

	memset(font_mgr, 0, sizeof(*font_mgr));

	return font_mgr;
}

font_config *fontconfigmgr_add_cache(fontconfig_mgr * font_mgr, font_config * p_cfg)
{
	font_config *p = NULL;

	if (font_mgr->cnt + 1 > font_mgr->caps) {
		font_mgr->caps += font_mgr->cnt + 16;
		font_mgr->cache = safe_realloc(font_mgr->cache, sizeof(font_mgr->cache[0]) * font_mgr->caps);
	}

	if (font_mgr->cache == NULL) {
		return NULL;
	}

	p = &font_mgr->cache[font_mgr->cnt++];
	memcpy(p, p_cfg, sizeof(font_mgr->cache[0]));

	return p;
}

font_config *fontconfigmgr_lookup(fontconfig_mgr * font_mgr, const char *font_name, int pixelsize, bool cjkmode)
{
	size_t i;
	font_config my_font_config, *p = NULL;

	for (i = 0; i < font_mgr->cnt; ++i) {
		if (!strcmp(font_mgr->cache[i].fontname, font_name)) {
			if (font_mgr->cache[i].pixelsize == pixelsize && font_mgr->cache[i].cjkmode == cjkmode) {
				return &font_mgr->cache[i];
			}
		}
	}

	new_font_config(font_name, &my_font_config, cjkmode);
	my_font_config.pixelsize = pixelsize;

	if (get_font_config(&my_font_config) == 0) {
		p = fontconfigmgr_add_cache(font_mgr, &my_font_config);
	} else {
		return NULL;
	}

	return p;
}

void fontconfigmgr_free(fontconfig_mgr * font_mgr)
{
	if (font_mgr->cache != NULL) {
		free(font_mgr->cache);
		font_mgr->cache = NULL;
	}

	free(font_mgr);
}

int get_font_config(font_config * p)
{
	FILE *fp;
	char linebuf[4096];
	token *t = NULL;
	size_t tsize = 0;

	if (g_font_config_path[0] == '\0') {
		STRCPY_S(g_font_config_path, scene_appdir());
		STRCAT_S(g_font_config_path, "fonts.conf");
	}

	fp = fopen(g_font_config_path, "rb");

	if (fp == NULL) {
		dbg_printf(d, "%s: cannot open file %s", __func__, g_font_config_path);
		return -1;
	}

	while (!feof(fp)) {
		if (fgets(linebuf, sizeof(linebuf), fp) == NULL) {
			break;
		}

		if (linebuf[strlen(linebuf) - 1] == '\n')
			linebuf[strlen(linebuf) - 1] = '\0';

		if (linebuf[0] == '\0')
			continue;

		if (linebuf[0] == '#')
			continue;

//      dbg_printf(d, "linebuf is %s", linebuf);

		scan_tokens(linebuf, &t, &tsize);
	}

	fclose(fp);

//  report_tokens(t, tsize);

	parse_tokens(p, t, tsize, false);

	destroy_token(&t, &tsize);

	return 0;
}

static const char *get_bool_str(bool b)
{
	return b ? "true" : "false";
}

int report_font_config(font_config * p)
{
	dbg_printf(d, "%-20s: %s", "name", p->fontname);
	dbg_printf(d, "%-20s: %d", "pixelsize", p->pixelsize);
	dbg_printf(d, "%-20s: %s", "globaladvance", get_bool_str(p->globaladvance));
	dbg_printf(d, "%-20s: %s", "antialias", get_bool_str(p->antialias));
	dbg_printf(d, "%-20s: %s", "hinting", get_bool_str(p->hinting));
	dbg_printf(d, "%-20s: %d", "hintstyle", p->hintstyle);
	dbg_printf(d, "%-20s: %s", "autohint", get_bool_str(p->autohint));
	dbg_printf(d, "%-20s: %s", "cleartype", get_bool_str(p->cleartype));
	dbg_printf(d, "%-20s: %s", "embolden", get_bool_str(p->embolden));
	dbg_printf(d, "%-20s: %d", "lcdfilter", p->lcdfilter);
	dbg_printf(d, "%-20s: %s", "cjkmode", get_bool_str(p->cjkmode));

	return 0;
}

#ifdef TEST

int main(int argc, char *argv[])
{
	d = dbg_init();
	dbg_open_stream(d, stderr);
	dbg_switch(d, 1);

	font_config my_font_config;

	STRCPY_S(g_font_config_path, "./fonts.conf");

	new_font_config("#WenQuanYi Zen Hei#", &my_font_config);

	if (argc < 2) {
		my_font_config.pixelsize = 16;
	} else {
		my_font_config.pixelsize = atoi(argv[1]);
	}

	if (get_font_config(&my_font_config) == 0) {
		report_font_config(&my_font_config);
	}

	dbg_close(d);

	return 0;
}
#endif
