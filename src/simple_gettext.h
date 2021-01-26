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

#include "config.h"

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef ENABLE_NLS

	typedef struct _TextDomainEntry TextDomainEntry;
	typedef struct _TextDomainEntry *PTextDomainEntry;

	struct _TextDomainEntry
	{
		char *domainname;
		char *dirname;
		PTextDomainEntry next;
	};

	const char *simple_gettext(const char *msgid);
	char *simple_bindtextdomain(const char *domainname, const char *dirname);
	char *simple_textdomain(const char *domainname);
	void simple_gettext_destroy(void);

#define _(STRING) simple_gettext(STRING)
#else

#define _(STRING) (STRING)
#define simple_gettext(msgid)
#define simple_bindtextdomain(domainname, dirname)
#define simple_textdomain(domainname) (domainname)
#define simple_gettext_destroy()

#endif

#ifdef __cplusplus
}
#endif
