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
#include <pspdisplay.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <psputility.h>
#include <psputility_osk.h>
#include <pspgu.h>
#include "display.h"
#include "strsafe.h"
#include "common/datatype.h"
#include "conf.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

static void ClearScreen(pixel * saveimage)
{
	if (saveimage) {
		disp_newputimage(0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, PSP_SCREEN_WIDTH, 0, 0, 0, 0, saveimage, true);
	}
}

int _get_osk_input(char *buf, int size, unsigned short desc[128])
{
	char tstr[128] = { 0 };
	unsigned short intext[128] = { 0 };	// text already in the edit box on start
	unsigned short outtext[128] = { 0 };	// text after input
	SceUtilityOskData data;
	SceUtilityOskParams osk;
	pixel *saveimage = (pixel *) memalign(16,
										  PSP_SCREEN_WIDTH * PSP_SCREEN_HEIGHT * sizeof(pixel));
	bool done = false;
	int rc;
	void *buffer;

	if (saveimage) {
		pixel *t;

		disp_getimage(0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, saveimage);

		t = disp_swizzle_image(saveimage, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT);
		if (t == NULL) {
			free(saveimage);
			return -1;
		}
		free(saveimage);
		saveimage = t;
	} else
		return -1;

	memset(&data, 0, sizeof(data));

	data.language = 0;
	data.lines = 1;				// just one line
	data.unk_24 = 1;			// set to 1
	data.desc = desc;
	data.inputtype = 0x1 | 0x4 | 0x8;
	data.intext = intext;
	data.outtextlength = 128;	// sizeof(outtext) / sizeof(unsigned short)
	data.outtextlimit = 50;		// just allow 50 chars
	data.outtext = (unsigned short *) outtext;

	memset(&osk, 0, sizeof(osk));
	osk.base.size = sizeof(osk);

	if (!strcmp(config.language, "zh_CN"))
		osk.base.language = 11;
	else
		osk.base.language = 2;

	osk.base.buttonSwap = 0;
	osk.base.graphicsThread = 37;	// gfx thread pri
	osk.base.accessThread = 49;	// unknown thread pri (?)
	osk.base.fontThread = 38;
	osk.base.soundThread = 36;
	osk.datacount = 1;
	osk.data = &data;

	rc = sceUtilityOskInitStart(&osk);

	if (rc) {
		free(saveimage);
		return 0;
	}

	ClearScreen(saveimage);

	while (!done) {
		int i, j = 0;

		switch (sceUtilityOskGetStatus()) {
			case PSP_UTILITY_DIALOG_INIT:
				break;
			case PSP_UTILITY_DIALOG_VISIBLE:
				ClearScreen(saveimage);
				sceUtilityOskUpdate(2);	// 2 is taken from ps2dev.org recommendation
				break;
			case PSP_UTILITY_DIALOG_QUIT:
				sceUtilityOskShutdownStart();
				break;
			case PSP_UTILITY_DIALOG_FINISHED:
				done = true;
				break;
			case PSP_UTILITY_DIALOG_NONE:
			default:
				break;
		}

		for (i = 0; data.outtext[i]; i++) {

			if (data.outtext[i] != '\0' && data.outtext[i] != '\n' && data.outtext[i] != '\r') {
				tstr[j] = data.outtext[i];
				j++;
			}
		}

		strcpy_s(buf, size, tstr);
		sceDisplayWaitVblankStart();
		sceGuSwapBuffers();
	}

	sceDisplayWaitVblankStart();
	buffer = sceGuSwapBuffers();

	free(saveimage);
	disp_fix_osk(buffer);

	return 1;
}

int get_osk_input(char *buf, int size)
{
	unsigned short desc[128] = {
		'E', 'n', 't', 'e', 'r', ' ', 'G', 'I', 0
	};

	return _get_osk_input(buf, size, desc);
}

int get_osk_input_password(char *buf, int size)
{
	unsigned short passwd[128] = {
		'E', 'n', 't', 'e', 'r', ' ', 'P', 'a', 's', 's', 'w', 'o', 'r', 'd', 0
	};

	return _get_osk_input(buf, size, passwd);
}
