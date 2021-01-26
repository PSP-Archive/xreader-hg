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
#include "music_test.h"
#include "commons.h"
#include "common/utils.h"
#include "text.h"
#include "fs.h"
#include "bookmark.h"
#include "scene.h"
#include "scene_impl.h"

int bookmark_test()
{
	const char *fn = "ms0:/电子书/全球通史.txt";
	p_bookmark pBookmark;
	t_bookmark tmpbm;
	int i, c;

	bookmark_init("ms0:/bookmark.conf");

	for (c = 0; c < 3; ++c) {
		pBookmark = bookmark_open(fn);

		if (pBookmark == NULL) {
			dbg_printf(d, "Cannot open bookmark %s", fn);
			return -1;
		}

		for (i = 0; i < 10; ++i) {
			pBookmark->row[i] = rand();
		}

		memcpy(&tmpbm, pBookmark, sizeof(tmpbm));

		bookmark_save(pBookmark);
		bookmark_close(pBookmark);
		pBookmark = NULL;
	}

	dword row;

	dbg_switch(d, 0);
#if 0
	bookmark_autosave(fn, tmpbm.row[0]);

	if ((row = bookmark_autoload(fn)) != tmpbm.row[0]) {
		dbg_switch(d, 1);
		dbg_printf(d, "bookmark autosave / load MISMATCHED %08lx %08lx", row,
				   tmpbm.row[0]);
	}
#endif

	dbg_switch(d, 1);

	pBookmark = bookmark_open(fn);

	if (pBookmark == NULL) {
		dbg_printf(d, "Cannot open bookmark %s again", fn);
	}

	for (i = 0; i < 10; ++i) {
		if (pBookmark->row[i] != tmpbm.row[i]) {
			dbg_printf(d, "Item %02d: %08lx %08lx MISMATCHED", i,
					   pBookmark->row[i], tmpbm.row[i]);
		}
	}

	bookmark_save(pBookmark);
	bookmark_close(pBookmark);
	pBookmark = NULL;

	dbg_printf(d, "bookmark completed");

	sceKernelDelayThread(1000000L);

	return 0;
}
