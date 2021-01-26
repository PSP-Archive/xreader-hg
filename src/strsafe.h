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

#ifndef STRSAFE_H
#define STRSAFE_H

#ifdef __cplusplus
extern "C"
{
#endif

	size_t strncpy_s(char *strDest, size_t numberOfElements, const char *strSource, size_t count);
	size_t strcpy_s(char *strDestination, size_t numberOfElements, const char *strSource);
	size_t strncat_s(char *strDest, size_t numberOfElements, const char *strSource, size_t count);
	size_t strcat_s(char *strDestination, size_t numberOfElements, const char *strSource);
	int snprintf_s(char *buffer, size_t sizeOfBuffer, const char *format, ...);
	size_t mbcslen(const unsigned char *str);
	size_t mbcsncpy_s(unsigned char *dst, size_t nBytes, const unsigned char *src, size_t n);

#define NELEMS(n) (sizeof(n) / sizeof(n[0]))

#ifdef STR_CHECK_SIZE
#define CPP_PASTE2i(a,b)                 a ## b	/* twofold indirection is required to expand macros like __LINE__ */
#define CPP_PASTE2(a,b)                  CPP_PASTE2i (a,b)
#define STATICASSERT(expr, asname) \
	typedef struct { \
		char asname[(expr) ? 0 : -1]; \
	} CPP_PASTE2(StaticAssertion_LINE, __LINE__)

#define STRCPY_S(d, s) do { STATICASSERT(sizeof(d) > 4, test); strcpy_s((d), (sizeof(d) / sizeof(d[0])), (s));} while ( 0 )
#define STRCAT_S(d, s) do { STATICASSERT(sizeof(d) > 4, test); strcat_s((d), (sizeof(d) / sizeof(d[0])), (s));} while ( 0 )
#define SPRINTF_S(str, fmt...) do { STATICASSERT(sizeof(str) > 4, test); snprintf_s((str), \
							  ((sizeof(str) / sizeof(str[0]))), fmt); } while ( 0 )
#define MBCSCPY_S(dst, src, n) do { STATICASSERT(sizeof(dst) > 4, test); mbcsncpy_s((dst), (sizeof(dst) / sizeof(dst[0])), (src), (n)); } while ( 0 )
#else
#define STRCPY_S(d, s) strcpy_s((d), (sizeof(d) / sizeof(d[0])), (s))
#define STRCAT_S(d, s) strcat_s((d), (sizeof(d) / sizeof(d[0])), (s))
#define SPRINTF_S(str, fmt...) snprintf_s((str), \
							  ((sizeof(str) / sizeof(str[0]))), fmt)
#define MBCSCPY_S(dst, src, n) mbcsncpy_s((dst), (sizeof(dst) / sizeof(dst[0])), (src), (n))
#endif

#ifdef __cplusplus
}
#endif

#endif
