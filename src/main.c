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

#include "config.h"

#include <pspdebug.h>
#include <pspkernel.h>
#include <pspsdk.h>
#include <psppower.h>
#include <pspdisplay.h>
#include <kubridge.h>
#include "fat.h"
#include "conf.h"
#include "scene.h"
#include "conf.h"
#include "display.h"
#include "ctrl.h"
#include "power.h"
#include "dbg.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

PSP_MODULE_INFO("XREADER", 0x0200, 1, 6);
PSP_MAIN_THREAD_PARAMS(45, 256, PSP_THREAD_ATTR_USER);
PSP_HEAP_SIZE_KB(-512);

static unsigned int lock_count = 0;
static unsigned int intr_flags = 0;

int psp_model;
int psp_fw_version;

/**
 * malloc lock for malloc-2.6.4 in newlib-0.17,
 * in malloc-2.8.4 newlib port we use our own lock,
 * so these code may be not used at all
 */
void __malloc_lock(struct _reent *ptr)
{
	unsigned int flags = pspSdkDisableInterrupts();

	if (lock_count == 0) {
		intr_flags = flags;
	}

	lock_count++;
}

void __malloc_unlock(struct _reent *ptr)
{
	if (--lock_count == 0) {
		pspSdkEnableInterrupts(intr_flags);
	}
}

static int power_callback(int arg1, int powerInfo, void *arg)
{
	dbg_printf(d, "%s: arg1 0x%08x powerInfo 0x%08x", __func__, arg1, powerInfo);

	if ((powerInfo & (PSP_POWER_CB_POWER_SWITCH | PSP_POWER_CB_STANDBY)) > 0) {
		power_down();
	} else if ((powerInfo & PSP_POWER_CB_RESUME_COMPLETE) > 0) {
		sceKernelDelayThread(1500000);
		power_up();
	}

	return 0;
}

static int exit_callback(int arg1, int arg2, void *arg)
{
	extern volatile bool xreader_scene_inited;

	while (xreader_scene_inited == false) {
		sceKernelDelayThread(1);
	}

	scene_exit();
	scePowerUnregisterCallback(0);
	sceKernelExitGame();

	return 0;
}

static int CallbackThread(unsigned int args, void *argp)
{
	int cbid;

	cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
	sceKernelRegisterExitCallback(cbid);
	scePowerUnregisterCallback(0);
	cbid = sceKernelCreateCallback("Power Callback", power_callback, NULL);
	scePowerRegisterCallback(0, cbid);
	sceKernelSleepThreadCB();

	return 0;
}

/* Sets up the callback thread and returns its thread id */
static int SetupCallbacks(void)
{
	int thid = sceKernelCreateThread("Callback Thread", CallbackThread, 0x11,
									 0x10000,
									 PSP_THREAD_ATTR_VFPU, 0);

	if (thid >= 0) {
		sceKernelStartThread(thid, 0, 0);
	}

	return thid;
}

static int main_thread(unsigned int args, void *argp)
{
	scene_init();
	return 0;
}

int main(int argc, char *argv[])
{
	int thid;

	psp_model = kuKernelGetModel();
	psp_fw_version = sceKernelDevkitVersion();

	SetupCallbacks();
	thid = sceKernelCreateThread("User Thread", main_thread, 45, 0x40000, PSP_THREAD_ATTR_VFPU, NULL);

	if (thid < 0)
		sceKernelSleepThread();

	sceKernelStartThread(thid, 0, NULL);
	sceKernelSleepThread();

	return 0;
}

/* call @XREADER:dlmalloc,0xBAD2E938@ */
void debug_malloc(void)
{
#ifdef DMALLOC
	static unsigned mark = -1;

	if (mark == -1) {
		mark = dmalloc_mark();
	} else {
		dmalloc_log_changed(mark, 1, 0, 1);
		dmalloc_log_stats();
		dmalloc_log_unfreed();
	}
#else
	struct mallinfo info;

	info = mallinfo();

	printf("number of free chunks: %d\n", info.ordblks);
	printf("total free space: %d\n", info.fordblks);
	printf("maximum total allocated space: %d\n", info.usmblks);
	printf("total allocated space: %d\n", info.uordblks);
	printf("keep cost: %d\n", info.keepcost);
#endif
}
