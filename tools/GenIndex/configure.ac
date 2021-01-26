AC_INIT([genindex], [1.0], [outmatch@gmail.com])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_PROG_CC
AC_CONFIG_HEADERS([config.h])

AH_TOP([/*
 * config.h
 *
 * Copyright (c) outmatch 2007
 *
 * You may distribute and modify this program under the terms of either
 * the GNU General Public License Version 3.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <assert.h>

#define TODO assert("TODO" && 0)

#define MAGICTOKEN	">>>>"
#define SPLITTOKEN	"======================"
#define MAX_PATH 1024
])

AH_BOTTOM([#endif])

AC_CONFIG_FILES([
		Makefile
		src/Makefile
		])
AC_OUTPUT
