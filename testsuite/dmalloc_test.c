#include <pspdebug.h>
#include <pspkernel.h>
#include <pspsdk.h>
#include <psprtc.h>
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
#include "image_queue.h"
#include "pspscreen.h"
#include "ctrl.h"
#include "xrhal.h"
#include "dmalloc.h"

extern p_win_menuitem filelist;
extern dword filecount;

int dmalloc_test_fs(void)
{
	dmalloc_debug_setup
		("log-stats,log-non-free,check-fence,check-heap,check-funcs,check-blank,print-messages,inter=100");

	unsigned dmark = dmalloc_mark();

	win_item_destroy(&filelist, &filecount);

	// test dir
	filecount = fs_dir_to_menu("ms0:/", "ms0:/", &filelist, 0, 0, 0, 0, 0, 0);

	// test rar
	filecount = fs_rar_to_menu("ms0:/PSPSDK.RAR", &filelist, 0, 0, 0, 0);

	// test zip
	filecount = fs_zip_to_menu("ms0:/lame3.98.2.zip", &filelist, 0, 0, 0, 0);

	// test chm
	filecount = fs_chm_to_menu("ms0:/temp/WinRAR.chm", &filelist, 0, 0, 0, 0);

	// test flash0 dir
	filecount =
		fs_flashdir_to_menu("flash0:/", "flash0:/", &filelist, 0, 0, 0, 0);

	win_item_destroy(&filelist, &filecount);

	dmalloc_log_changed(dmark, 1, 0, 1);
	dmalloc_log_stats();

	return 0;
}
