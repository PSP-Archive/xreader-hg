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

#include <pspsdk.h>
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <psploadexec_kernel.h>
#include <systemctrl.h>
#include <systemctrl_se.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "kubridge.h"
#include "xr_rdriver/xr_rdriver.h"
#include "m33boot.h"
#include "dbg.h"
#include "xrPrx/xrPrx.h"

static struct SceKernelLoadExecVSHParam param;
static int apitype = 0;
static const char *program = NULL;
static char *mode = NULL;

static int lanuch_api(void)
{
	memset(&param, 0, sizeof(param));
	param.size = sizeof(param);
	param.args = strlen(program) + 1;
	param.argp = (char *) program;
	param.key = mode;

	dbg_printf(d, "%s: program %s", __func__, program);

	return sctrlKernelLoadExecVSHWithApitype(apitype, program, &param);
}

extern int run_homebrew(const char *path)
{
	apitype = 0x141;
	program = path;
	mode = "game";

	return lanuch_api();
}

extern int run_umd(void)
{
	apitype = 0x120;
	program = "disc0:/PSP_GAME/SYSDIR/EBOOT.BIN";
	mode = "game";

	return lanuch_api();
}

extern int run_iso(const char *iso)
{
	SEConfig config;
	int r;

	apitype = 0x120;
	program = "disc0:/PSP_GAME/SYSDIR/EBOOT.BIN";
	mode = "game";

	SetUmdFile((char *) iso);
	r = sctrlSEGetConfigEx(&config, sizeof(config));

	dbg_printf(d, "%s: sctrlSEGetConfigEx %08x", __func__, r);

	if (config.umdmode == MODE_MARCH33) {
		SetConfFile(1);
	} else if (config.umdmode == MODE_NP9660) {
		SetConfFile(2);
	} else {
		// Assume this is is normal umd mode, as isofs will be deleted soon
		SetConfFile(0);
	}

	return lanuch_api();
}

int run_psx_game(const char *path)
{
	apitype = 0x143;
	program = path;
	mode = "pops";

	return lanuch_api();
}
