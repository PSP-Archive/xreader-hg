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

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

	struct _APETagHeader
	{
		uint64_t preamble;
		uint32_t version;
		uint32_t tag_size;
		uint32_t item_count;
		uint32_t flags;
		uint64_t reserved;
	} __attribute__ ((packed));

	typedef struct _APETagHeader APETagHeader;

	enum
	{
		APE_HEADER,
		APE_FOOTER
	};

	enum
	{
		APETAG_OK = 0,
		APETAG_ERROR_OPEN,
		APETAG_ERROR_FILEFORMAT,
		APETAG_UNSUPPORT_TAGVERSION,
		APETAG_BAD_ARGUMENT,
		APETAG_NOT_IMPLEMENTED,
		APETAG_MEMORY_NOT_ENOUGH,
	};

	extern int apetag_errno;

	struct _APETagItem
	{
		uint32_t item_value;
		uint32_t item_flags;
	} __attribute__ ((packed));

	typedef struct _APETagItem APETagItem;

	struct _APETagItems
	{
		int item_count;
		APETagItem **items;
	};

	typedef struct _APETagItems APETagItems;

	struct _APETag
	{
		int is_header_or_footer;
		APETagHeader footer;
		int item_count;
		APETagItems *items;
	};

	typedef struct _APETag APETag;

	enum
	{
		APE_ITEM_UTF8 = 0,
		APE_ITEM_BINARY = 1,
		APE_ITEM_EXT = 2
	};

#define APE_ISBITSET(v,n) ((((char*)v)[n/8] &   (1<<(n%8))))
#define APE_ITEM_HAS_HEADER(item) APE_ISBITSET((&(item)->item_flags), (31))
#define APE_ITEM_HAS_FOOTER(item) APE_ISBITSET((&(item)->item_flags), (30))
#define APE_ITEM_IS_HEADER(item) APE_ISBITSET((&(item)->item_flags), (29))
#define APE_ITEM_IS_FOOTER(item) (!APE_ISBITSET((&(item)->item_flags), (29)))
#define APE_ITEM_IS_UTF8(item) ((((item)->item_flags >> 1 ) & 7) == APE_ITEM_UTF8)
#define APE_ITEM_IS_BINARY(item) ((((item)->item_flags >> 1 ) & 7) == APE_ITEM_BINARY)
#define APE_ITEM_IS_EXT(item) ((((item)->item_flags >> 1 ) & 7) == APE_ITEM_EXT)
#define APE_ITEM_IS_READONLY(item) APE_ISBITSET((&(item)->item_flags), (0))
#define APE_ITEM_GET_KEY(item) (((char*)(item))+8)
#define APE_ITEM_GET_KEY_LEN(item) (strlen(((char*)(item))+8))
#define APE_ITEM_GET_VALUE(item) (APE_ITEM_GET_KEY(item) + 1 + strlen(APE_ITEM_GET_KEY(item)))
#define APE_ITEM_GET_VALUE_LEN(item) ((item)->item_value)

	APETag *apetag_load(const char *filename);
	int apetag_free(APETag * tag);
	const char *apetag_strerror(int errno);
	char *apetag_get(APETag * tag, const char *key);

#ifdef __cplusplus
}
#endif
