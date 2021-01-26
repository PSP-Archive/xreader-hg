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

#ifdef PSP
#include <ctype.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pspsdk.h>
#include "common/qsort.h"
#include "common/utils.h"
#endif
#include <stdio.h>
#include <string.h>
#include "APETag.h"
#include "dbg.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

int apetag_errno = 0;

static long has_id3(FILE * fp)
{
	long p;
	char bytes[3];

	fseek(fp, -128, SEEK_END);
	p = ftell(fp);

	if (fread(bytes, 3, 1, fp) == 1) {
		if (memcmp(bytes, "TAG", 3) == 0) {
//          dbg_printf(d, "发现ID3v1 标记，位置 %ld", p);
			return p;
		}
	}

	return -1;
}

static APETagItems *new_item(void)
{
	APETagItems *p = (APETagItems *) malloc(sizeof(APETagItems));

	p->item_count = 0;
	p->items = NULL;

	return p;
}

static int free_item(APETagItems * items)
{
	int i;

	if (items == NULL) {
		return -1;
	}

	for (i = 0; i < items->item_count; ++i) {
		free(items->items[i]);
		items->items[i] = NULL;
	}

	items->item_count = 0;
	free(items->items);
	items->items = NULL;
	free(items);

	return 0;
}

static int append_item(APETagItems * items, void *ptr, int size)
{
	if (items == NULL || ptr == NULL || size == 0) {
		apetag_errno = APETAG_BAD_ARGUMENT;
		return -1;
	}

	if (items->item_count == 0) {
		items->item_count++;
		items->items = (APETagItem **) malloc(sizeof(APETagItem *) * 1);
		if (items->items == NULL) {
			apetag_errno = APETAG_MEMORY_NOT_ENOUGH;
			return -1;
		}
		items->items[0] = (APETagItem *) malloc(size);
		if (items->items[0] == NULL) {
			apetag_errno = APETAG_MEMORY_NOT_ENOUGH;
			return -1;
		}
		memcpy(items->items[0], ptr, size);
	} else {
		APETagItem **t;

		items->item_count++;
		t = (APETagItem **) realloc(items->items,
												 sizeof(APETagItem *) *
												 items->item_count);
		if (t != NULL)
			items->items = t;
		else {
			apetag_errno = APETAG_MEMORY_NOT_ENOUGH;
			return -1;
		}
		items->items[items->item_count - 1] = (APETagItem *) malloc(size);
		if (items->items[items->item_count - 1] == NULL) {
			apetag_errno = APETAG_MEMORY_NOT_ENOUGH;
			return -1;
		}
		memcpy(items->items[items->item_count - 1], ptr, size);
	}

	apetag_errno = APETAG_OK;

	return 0;
}

static APETagItem *find_item(const APETagItems * items, const char *searchstr)
{
	int i;

	if (items == NULL)
		return NULL;

	for (i = 0; i < items->item_count; ++i) {
		if (strcmp(APE_ITEM_GET_KEY(items->items[i]), searchstr) == 0) {
			return items->items[i];
		}
	}

	return NULL;
}

static int read_apetag(FILE * fp, APETag * tag)
{
	if (fread(&tag->footer, sizeof(APETagHeader), 1, fp) == 1) {
		if (memcmp(&tag->footer.preamble, "APETAGEX", 8) == 0) {
			char *raw_tag;
			int i;
			const char *p;

			/*
			   if (tag->footer.version != 2000) {
			   apetag_errno = APETAG_UNSUPPORT_TAGVERSION;
			   return -1;
			   }
			 */

			tag->item_count = tag->footer.item_count;
			tag->items = new_item();

			if (tag->items == NULL) {
				apetag_errno = APETAG_MEMORY_NOT_ENOUGH;
				return -1;
			}

			fseek(fp, -tag->footer.tag_size, SEEK_CUR);
//          dbg_printf(d, "转到MP3位置 %ld", ftell(fp));

			raw_tag = (char *) malloc(tag->footer.tag_size);

			if (raw_tag == NULL) {
				apetag_errno = APETAG_MEMORY_NOT_ENOUGH;
				return -1;
			}

			if (fread(raw_tag, tag->footer.tag_size, 1, fp) != 1) {
				apetag_errno = APETAG_ERROR_FILEFORMAT;
				return -1;
			}

			p = raw_tag;

			for (i = 0; i < tag->footer.item_count; ++i) {
				int size = APE_ITEM_GET_VALUE_LEN((APETagItem *) p);
				int m = APE_ITEM_GET_KEY_LEN((APETagItem *) p);
//              char str[80];

//              sprintf(str, "项目 %%d 大小 %%d %%.%ds: %%.%ds", m, size);
//              dbg_printf(d, str, i+1, size, p+8, p + 8 + 1 + m);
				append_item(tag->items, (void *) p, size + m + 1 + 8);
				p += size + m + 1 + 8;
			}

			free(raw_tag);

			/*
			   dbg_printf(d,
			   "发现APE标记: 版本(%d) 项目个数: %d 项目+尾字节大小: %u",
			   (int) tag->footer.version, tag->item_count,
			   (size_t) tag->footer.tag_size);
			   for (i = 0; i < tag->items->item_count; ++i) {
			   print_item_type(tag->items->items[i]);
			   }
			 */
			apetag_errno = APETAG_OK;
			tag->is_header_or_footer = APE_FOOTER;

			return 0;
		}
	}

	return -2;
}

static int search_apetag(FILE * fp, APETag * tag)
{
	int ret;

	apetag_errno = APETAG_NOT_IMPLEMENTED;

	// 首先搜索文件尾处，ID3V1标签之前，是否有APE标签
	if (has_id3(fp) != -1) {
		fseek(fp, -160, SEEK_END);
//      dbg_printf(d, "转到MP3位置 %ld", ftell(fp));
	} else {
		fseek(fp, -32, SEEK_END);
//      dbg_printf(d, "转到MP3位置 %ld", ftell(fp));
	}

	if ((ret = read_apetag(fp, tag)) == 0)
		return 0;
	if (ret == -2) {
		fseek(fp, -32, SEEK_END);
		if (read_apetag(fp, tag) == 0) {
			tag->is_header_or_footer = APE_FOOTER;
			return 0;
		}
		// 如果没有找到，搜索文件头是否有APE标签
		fseek(fp, 0, SEEK_SET);
		if (read_apetag(fp, tag) == 0) {
			tag->is_header_or_footer = APE_HEADER;
			return 0;
		}
	}

	return -1;
}

APETag *apetag_load(const char *filename)
{
	FILE *fp;
	APETag *p;

	if (filename == NULL)
		return 0;

	fp = fopen(filename, "rb");

	if (fp == NULL) {
		apetag_errno = APETAG_ERROR_OPEN;
		return 0;
	}

	p = (APETag *) malloc(sizeof(APETag));

	if (p == NULL) {
		apetag_errno = APETAG_MEMORY_NOT_ENOUGH;
		fclose(fp);
		return 0;
	}

	if (search_apetag(fp, p) == -1) {
		apetag_errno = APETAG_ERROR_FILEFORMAT;
		fclose(fp);
		return 0;
	}

	apetag_errno = APETAG_OK;
	fclose(fp);

	return p;
}

int apetag_free(APETag * tag)
{
	if (tag == NULL) {
		apetag_errno = APETAG_BAD_ARGUMENT;
		return 0;
	}

	free_item(tag->items);
	free(tag);

	return 0;
}

char *apetag_get(APETag * tag, const char *key)
{
	int size;
	char *t;
	APETagItem *pItem;

	if (tag == NULL || key == NULL) {
		return NULL;
	}

	pItem = find_item(tag->items, key);

	if (pItem == NULL || !APE_ITEM_IS_UTF8(pItem)) {
		return NULL;
	}

	size = APE_ITEM_GET_VALUE_LEN(pItem);
	t = malloc(size + 1);
	memcpy(t, APE_ITEM_GET_VALUE(pItem), size);
	t[size] = '\0';

	return t;
}
