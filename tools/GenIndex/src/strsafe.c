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

//    xMP3
//    Copyright (C) 2008 Hrimfaxi
//    outmatch@gmail.com
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

//
//    $Id: strsafe.c 349 2008-02-22 12:42:32Z outmatch $
//

/* vim:set ts=4 sw=4 cindent ignorecase enc=gbk: */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "strsafe.h"

size_t strnlen(const char *s, size_t len)
{
	size_t i;

	for (i = 0; i < len && s[i]; i++);
	return i;
}

size_t
strncpy_s(char *strDest,
		  size_t numberOfElements, const char *strSource, size_t count)
{
	if (!strDest || !strSource || numberOfElements == 0)
		return 0;
#ifdef _DEBUG
	if (numberOfElements == 4) {
		dbg_printf(d, "strncpy_s: strDest可能退化为指针: %s", strSource);
	}
#endif
	size_t copied = numberOfElements - 1 < count ? numberOfElements - 1 : count;
	strncpy(strDest, strSource, copied);
	strDest[copied] = '\0';

	return strnlen(strDest, numberOfElements);
}

size_t
strcpy_s(char *strDestination, size_t numberOfElements, const char *strSource)
{
	return strncpy_s(strDestination, numberOfElements, strSource, -1);
}

size_t strncat_s(char *strDest,
				 size_t numberOfElements, const char *strSource, size_t count)
{
	size_t rest;

	if (!strDest || !strSource || numberOfElements == 0)
		return 0;

#ifdef _DEBUG
	if (numberOfElements == 4) {
		dbg_printf(d, "strncat_s: strDest可能退化为指针: %s", strSource);
	}
#endif

	rest = numberOfElements - strnlen(strDest, numberOfElements) - 1;
	strncat(strDest, strSource, rest < count ? rest : count);

	return strnlen(strDest, numberOfElements);
}

size_t
strcat_s(char *strDestination, size_t numberOfElements, const char *strSource)
{
	return strncat_s(strDestination, numberOfElements, strSource, -1);
}

int snprintf_s(char *buffer, size_t sizeOfBuffer, const char *format, ...)
{
	if (!buffer || sizeOfBuffer == 0)
		return -1;

#ifdef _DEBUG
	if (sizeOfBuffer == 4) {
		dbg_printf(d, "snprintf_s: strDest可能退化为指针: %s", format);
	}
#endif

	va_list va;

	va_start(va, format);

	int ret = vsnprintf(buffer, sizeOfBuffer, format, va);

	buffer[sizeOfBuffer - 1] = '\0';

	va_end(va);

	return ret;
}

size_t mbcslen(const unsigned char *str)
{
	size_t s = 0;

	if (!str)
		return 0;

	while (*str != '\0') {
		if (*str > 0x80 && *(str + 1) != '\0') {
			s++;
			str += 2;
		} else {
			s++;
			str++;
		}
	}

	return s;
}

size_t mbcsncpy_s(unsigned char *dst, size_t nBytes, const unsigned char *src,
				  size_t n)
{
	unsigned char *start = dst;

	if (!dst || !src || nBytes == 0 || n == 0)
		return 0;

#ifdef _DEBUG
	if (nBytes == 4) {
		dbg_printf(d, "mbcsncpy_s: dst可能退化为指针: %s", src);
	}
#endif

	while (nBytes > 0 && n > 0 && *src != '\0') {
		if (*src > 0x80 && *(src + 1) != '\0') {
			if (nBytes > 2) {
				nBytes -= 2, n--;
				*dst++ = *src++;
				*dst++ = *src++;
			} else {
				break;
			}
		} else {
			if (nBytes > 1) {
				nBytes--, n--;
				*dst++ = *src++;
			} else {
				break;
			}
		}
	}

	*dst = '\0';

	return mbcslen(start);
}
