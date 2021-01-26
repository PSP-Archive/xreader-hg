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
#include <pspdebug.h>
#include <pspsdk.h>
#include <psploadcore.h>
#include <pspsysmem_kernel.h>
#include <psputilsforkernel.h>
#include <pspdisplay_kernel.h>
#include <psppower.h>
#include <pspimpose_driver.h>
#include <pspinit.h>
#include <string.h>
#include "config.h"
#include "systemctrl.h"
#include "xrPrx.h"

typedef unsigned short bool;

extern int _sceDisplaySetBacklightSel(int backlight_level, int mode);

PSP_MODULE_INFO("xrPrx", 0x1007, 1, 1);
PSP_MAIN_THREAD_ATTR(0);

struct PspModuleImport
{
	const char *name;
	unsigned short version;
	unsigned short attribute;
	unsigned char entLen;
	unsigned char varCount;
	unsigned short funcCount;
	unsigned int *fnids;
	unsigned int *funcs;
	unsigned int *vnids;
	unsigned int *vars;
} __attribute__ ((packed));

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAKE_JUMP(f) (0x08000000 | (((u32)(f) >> 2) & 0x03ffffff))

PspDebugErrorHandler curr_handler;
PspDebugRegBlock *exception_regs;

void _pspDebugExceptionHandler(void);
int sceKernelRegisterDefaultExceptionHandler(void *func);

int xrKernelInitApitype(void)
{
	u32 k1;
	int apitype;

	k1 = pspSdkSetK1(0);
	apitype = sceKernelInitApitype();
	pspSdkSetK1(k1);

	return apitype;
}

SceUID xrIoOpen(const char *file, int flags, SceMode mode)
{
	SceUID fd;
	u32 k1, level;

	k1 = pspSdkSetK1(0);
	level = sctrlKernelSetUserLevel(8);
	fd = sceIoOpen(file, flags, mode);
	sctrlKernelSetUserLevel(level);
	pspSdkSetK1(k1);

	return fd;
}

SceOff xrIoLseek(SceUID fd, SceOff offset, int whence)
{
	SceOff ret;
	u32 k1, level;

	k1 = pspSdkSetK1(0);
	level = sctrlKernelSetUserLevel(8);
	ret = sceIoLseek(fd, offset, whence);
	sctrlKernelSetUserLevel(level);
	pspSdkSetK1(k1);

	return ret;
}

int xrIoRead(SceUID fd, void *data, SceSize size)
{
	int ret;
	u32 k1, level;

	k1 = pspSdkSetK1(0);
	level = sctrlKernelSetUserLevel(8);
	ret = sceIoRead(fd, data, size);
	sctrlKernelSetUserLevel(level);
	pspSdkSetK1(k1);

	return ret;
}

int xrIoClose(SceUID fd)
{
	SceUID ret;
	u32 k1, level;

	k1 = pspSdkSetK1(0);
	level = sctrlKernelSetUserLevel(8);
	ret = sceIoClose(fd);
	sctrlKernelSetUserLevel(level);
	pspSdkSetK1(k1);

	return ret;
}

void sync_cache(void)
{
	sceKernelIcacheInvalidateAll();
	sceKernelDcacheWritebackInvalidateAll();
}

static int max_brightness = 100;

int xrDisplaySetMaxBrightness(int newlevel)
{
	max_brightness = newlevel;

	return max_brightness;
}

static int get_backlight_level(int brightness)
{
	int max_backlight_level = 0;

	if (max_brightness < 24) {
		max_backlight_level = 0;
	} else if (max_brightness < 40) {
		max_backlight_level = 1;
	} else if (max_brightness < 72) {
		max_backlight_level = 2;
	} else if (max_brightness < 100) {
		max_backlight_level = 3;
	} else {
		max_backlight_level = 4;
	}

	return max_backlight_level;
}

void xrDisplaySetBrightness(int brightness, int unk1)
{
	u32 k1;

	k1 = pspSdkSetK1(0);
	brightness = MIN(max_brightness, brightness);
	_sceDisplaySetBacklightSel(get_backlight_level(brightness), unk1);
	sceDisplaySetBrightness(brightness, unk1);
	pspSdkSetK1(k1);
}

void xrDisplayGetBrightness(int *brightness, int *unk1)
{
	u32 k1;

	k1 = pspSdkSetK1(0);
	sceDisplayGetBrightness(brightness, unk1);
	pspSdkSetK1(k1);
}

static int (*sceDisplaySetBacklightSel) (int brightness, int mode) = NULL;
static int (*sceDisplayGetBacklightSel) (int *brightness, int *mode) = NULL;

int _sceDisplaySetBacklightSel(int backlight_level, int mode)
{
	int cur_backlight_level, unk;

	backlight_level = MIN(backlight_level, get_backlight_level(max_brightness));
	sceDisplayGetBacklightSel(&cur_backlight_level, &unk);

	if (backlight_level == cur_backlight_level) {
		return 0;
	}

	return (*sceDisplaySetBacklightSel) (backlight_level, mode);
}

static struct PspModuleImport *_libsFindImport(SceUID uid, const char *library)
{
	SceModule *pMod;
	void *stubTab;
	int stubLen;

	pMod = (SceModule *) sceKernelFindModuleByUID(uid);

	if (pMod != NULL) {
		int i = 0;

		stubTab = pMod->stub_top;
		stubLen = pMod->stub_size;

		while (i < stubLen) {
			struct PspModuleImport *pImp = (struct PspModuleImport *) (stubTab + i);

			if ((pImp->name) && (strcmp(pImp->name, library) == 0)) {
				return pImp;
			}

			i += (pImp->entLen * 4);
		}
	}

	return NULL;
}

unsigned int libsFindImportAddrByNid(SceUID uid, const char *library, unsigned int nid)
{
	struct PspModuleImport *pImp;

	pImp = _libsFindImport(uid, library);

	if (pImp) {
		int i;

		for (i = 0; i < pImp->funcCount; i++) {
			if (pImp->fnids[i] == nid) {
				return (unsigned int) &pImp->funcs[i * 2];
			}
		}
	}

	return 0;
}

void patch_brightness(void)
{
	SceModule *pMod;
	u32 *sceDisplaySetBacklightSelStub;

	pMod = (SceModule *) sceKernelFindModuleByName("sceImpose_Driver");

	if (pMod == NULL) {
		return;
	}

	sceDisplaySetBacklightSel = (void *) sctrlHENFindFunction("sceDisplay_Service", "sceDisplay_driver", 0xE55F0D50);

	if (sceDisplaySetBacklightSel == NULL) {
		return;
	}

	sceDisplayGetBacklightSel = (void *) sctrlHENFindFunction("sceDisplay_Service", "sceDisplay_driver", 0x96CFAC38);

	if (sceDisplayGetBacklightSel == NULL) {
		return;
	}

	sceDisplaySetBacklightSelStub = (u32 *) libsFindImportAddrByNid(pMod->modid, "sceDisplay_driver", 0xE55F0D50);

	if (sceDisplaySetBacklightSelStub == NULL) {
		return;
	}

	_sw(MAKE_JUMP(&_sceDisplaySetBacklightSel), (u32) sceDisplaySetBacklightSelStub);

	sync_cache();
}

/* Entry point */
int module_start(SceSize args, void *argp)
{
#ifndef _DEBUG
	int ret = 0;

	if (args != 8)
		return 0;
	curr_handler = (PspDebugErrorHandler) ((int *) argp)[0];
	exception_regs = (PspDebugRegBlock *) ((int *) argp)[1];
	if (!curr_handler || !exception_regs)
		return -1;

	ret = sceKernelRegisterDefaultExceptionHandler((void *) _pspDebugExceptionHandler);
#endif

	patch_brightness();

	return 0;
}

int module_stop(SceSize args, void *argp)
{
	return 0;
}
