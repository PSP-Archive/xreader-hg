#include <pspdebug.h>
#include <pspkernel.h>
#include <pspsdk.h>
#include <psprtc.h>
#include <stdio.h>
#include <string.h>
#include "win.h"
#include "common/datatype.h"
#include "fs.h"
#include "rar_speed_test.h"
#include "dbg.h"
#include "archive.h"
#include "buffer.h"
#include "freq_lock.h"
#include "power.h"
#include "commons.h"

extern dword filecount;

int rar_speed_test(void)
{
	p_win_menuitem filelist = NULL;
	dword i, bytes = 0;
	int fid;
	u64 start, now;

	sceRtcGetCurrentTick(&start);

	fid = freq_enter_hotzone();

	filecount = fs_rar_to_menu("ms0:/test.rar", &filelist, 0, 0, 0, 0);

	for (i = 0; i < filecount; ++i) {
		buffer *pb;

		extract_archive_file_into_buffer(&pb, "ms0:/test.rar",
										 filelist[i].compname->ptr,
										 fs_filetype_rar);

		sceRtcGetCurrentTick(&now);

		if (pb != NULL) {
			bytes += pb->used;
		}
#if 0
		if (pb != NULL) {
			dbg_printf(d, "%u: %s %s %d bytes in %f seconds", (unsigned) i,
					   "ms0:/test.rar", filelist[i].compname->ptr, pb->used,
					   pspDiffTime(&now, &start));
		} else {
			dbg_printf(d, "%u: %s %s failed in %f seconds", (unsigned) i,
					   "ms0:/test.rar", filelist[i].compname->ptr,
					   pspDiffTime(&now, &start));
		}
#endif

		if (pb != NULL) {
			buffer_free(pb);
		}
	}

	sceRtcGetCurrentTick(&now);
	dbg_printf(d, "Benchmark: %u files (%u bytes) extracted in %f seconds",
			   filecount, bytes, pspDiffTime(&now, &start));

	if (filelist != NULL) {
		win_item_destroy(&filelist, &filecount);
	}

	freq_leave(fid);

	return 0;
}
