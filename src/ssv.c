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

#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <ctype.h>
#include "common/datatype.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

int clean_args(int argc, char **argv)
{
	int i;

	if (argv != NULL) {
		for (i = 0; i < argc; ++i) {
			if (argv[i] != NULL)
				free(argv[i]);
		}

		free(argv);
	}

	return 0;
}

static int append_str(char ch, char **str)
{
	size_t slen;
	char *p;

	if (*str == NULL) {
		slen = 0;
	} else {
		slen = strlen(*str);
	}

	p = realloc(*str, slen + 1 + 1);

	if (p == NULL) {
		return -1;
	}

	*str = p;
	(*str)[slen] = ch;
	(*str)[slen + 1] = '\0';

	return 0;
}

static int new_arg(int *cnt, char **argv[])
{
	char **p;

	p = realloc(*argv, sizeof(char *) * (*cnt + 1));

	if (p == NULL)
		return -1;

	*argv = p;
	(*argv)[*cnt] = NULL;
	(*cnt)++;

	return 0;
}

int build_args(const char *str, int *argc, char ***argv)
{
	bool quote, backslash, endword;
	const char *p;
	bool empty;

	quote = false;
	empty = true;
	backslash = false;
	endword = true;
	*argc = 0;
	*argv = NULL;

	for (p = str; *p != '\0'; ++p) {
		if (*p == '\\') {
			if (backslash) {
				if (endword) {
					new_arg(argc, argv);
					endword = false;
				}
				append_str('\\', &(*argv)[*argc - 1]);
				empty = false;
				backslash = false;
			} else {
				backslash = true;
			}
		} else if (*p == '\"') {
			if (backslash) {
				if (endword) {
					new_arg(argc, argv);
					endword = false;
				}
				append_str('\"', &(*argv)[*argc - 1]);
				empty = false;
				backslash = false;
			} else {
				endword = true;

				if (quote) {
					if (empty) {
						new_arg(argc, argv);
						append_str('\0', &(*argv)[*argc - 1]);
					}

					quote = false;
				} else {
					quote = true;
					empty = true;
				}
			}
		} else if (isspace(*p)) {
			if (backslash) {
				if (endword) {
					new_arg(argc, argv);
					endword = false;
				}

				append_str(*p, &(*argv)[*argc - 1]);
				empty = false;
				backslash = false;
			} else {
				if (quote) {
					if (endword) {
						new_arg(argc, argv);
						endword = false;
					}
					append_str(*p, &(*argv)[*argc - 1]);
					empty = false;
				} else {
					endword = true;
				}
			}
		} else {
			if (endword) {
				new_arg(argc, argv);
				endword = false;
			}
			append_str(*p, &(*argv)[*argc - 1]);
			empty = false;
		}
	}

	return 0;
}
