#include <pspdebug.h>
#include <pspkernel.h>
#include <pspsdk.h>
#include <stdio.h>
#include <string.h>
#include "dbg.h"
#include "conf.h"
#include "freq_lock.h"
#include "rar_speed_test.h"
#include "jpeg_speed_test.h"
#include "hprm_test.h"
#include "display.h"
#include "image_queue.h"
#include "music_test.h"
#include "bookmark_test.h"
#include "commons.h"
#include "common/utils.h"

PSP_MODULE_INFO("xTest", 0x0200, 1, 6);
PSP_MAIN_THREAD_PARAMS(45, 256, PSP_THREAD_ATTR_USER);
PSP_HEAP_SIZE_MAX();

static int exit_callback(int arg1, int arg2, void *arg)
{
	cache_free();

	dbg_close(d);
	d = NULL;
	sceKernelExitGame();

	return 0;
}

static int callback_thr(unsigned int args, void *argp)
{
	int cbid;

	cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
	sceKernelRegisterExitCallback(cbid);
	sceKernelSleepThreadCB();

	return 0;
}

/* Sets up the callback thread and returns its thread id */
static int setup_callbacks(void)
{
	int thid = sceKernelCreateThread("Callback Thread", &callback_thr, 0x11,
									 0x3F40,
									 0, 0);

	if (thid >= 0) {
		sceKernelStartThread(thid, 0, 0);
	}

	return thid;
}

int main_thr(unsigned int args, void *argp)
{
	pspDebugScreenInit();

	pspDebugScreenPrintf("Welcome To xTest: xReader Testing Framework.\n");

	conf_load(&config);
	freq_init();
	fat_init();

	d = dbg_init();
	dbg_open_file(d, "ms0:/xTest.log");
	dbg_open_psp(d);
	dbg_open_stream(d, stderr);
	dbg_switch(d, 1);

	dbg_printf(d, "Start xTest testing...");

	utils_del_file("ms0:/xTest.log");

	while (1) {
		extern int dmalloc_test_fs(void);

		dmalloc_test_fs();
//      image_queue_test();
//      jpeg_speed_test();
//      rar_speed_test();
//      hprm_test();
//      music_test();
		sceKernelDelayThread(100000);
	}

	return 0;
}

int main(int argc, char *argv[])
{
	setup_callbacks();

	int thid = sceKernelCreateThread("User Thread", &main_thr, 45, 0x40000,
									 PSP_THREAD_ATTR_USER, NULL);

	if (thid < 0)
		sceKernelSleepThread();

	sceKernelStartThread(thid, 0, NULL);
	sceKernelSleepThread();

	return 0;
}
