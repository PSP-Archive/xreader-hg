#include <pspdebug.h>
#include <pspkernel.h>
#include <pspsdk.h>
#include <psprtc.h>
#include <pspsdk.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "win.h"
#include "common/datatype.h"
#include "common/utils.h"
#include "fs.h"
#include "rar_speed_test.h"
#include "dbg.h"
#include "archive.h"
#include "buffer.h"
#include "freq_lock.h"
#include "power.h"
#include "image.h"
#include "scene.h"
#include "commons.h"

extern dword filecount;

int jpeg_speed_test(void)
{
	p_win_menuitem filelist = NULL;
	dword i;
	dword pixels_size = 0;
	int fid;
	u64 start, now;

	dbg_printf(d, "Start jpeg benchmark");

	sceRtcGetCurrentTick(&start);

	fid = freq_enter_hotzone();

	filecount = fs_rar_to_menu("ms0:/test.rar", &filelist, 0, 0, 0, 0);

	for (i = 0; i < filecount; ++i) {
		const char *p;
		int ret;

		p = utils_fileext(filelist[i].compname->ptr);

		if (!p || stricmp(p, "jpg")) {
			dbg_printf(d, "skip %s", filelist[i].compname->ptr);
			continue;
		}

		where = scene_in_rar;
		dword w, h;
		pixel *img_dat = NULL;
		pixel bgcolor;

		dbg_switch(d, 0);
		ret =
			image_open_archive(filelist[i].compname->ptr, "ms0:/test.rar",
							   fs_filetype_jpg, &w, &h, &img_dat, &bgcolor,
							   scene_in_rar);
		dbg_switch(d, 1);

		if (ret != 0) {
			dbg_printf(d, "open %s failed", filelist[i].compname->ptr);
			continue;
		}

		if (img_dat != NULL)
			free(img_dat);

		pixels_size += w * h;
	}

	sceRtcGetCurrentTick(&now);
	dbg_printf(d, "Benchmark: %u files (%u pixels) extracted in %f seconds",
			   (unsigned) filecount, (unsigned) pixels_size, pspDiffTime(&now,
																		 &start));

	if (filelist != NULL) {
		win_item_destroy(&filelist, &filecount);
	}

	freq_leave(fid);

	return 0;
}
