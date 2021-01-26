#include <pspdebug.h>
#include <pspkernel.h>
#include <pspsdk.h>
#include <psprtc.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "win.h"
#include "common/datatype.h"
#include "hprm_test.h"
#include "dbg.h"
#include "buffer.h"
#include "power.h"
#include "commons.h"
#include "ctrl.h"

extern dword filecount;

#if 0
int hprm_test(void)
{
	dword key;

	ctrl_init();

	while (1) {
		key = ctrl_hprm_raw();
		dbg_printf(d, "ctrl_hprm_raw() returns %d", (int) key);
		sceKernelDelayThread(1000000);
	}

	return 0;
}
#endif

int hprm_test(void)
{
	dword key = 0;
	dword oldkey = 0;
	u64 start, end;
	double interval;

	ctrl_init();

	sceRtcGetCurrentTick(&start);
	sceRtcGetCurrentTick(&end);

	while (1) {
		key = ctrl_hprm_raw();
		sceRtcGetCurrentTick(&end);
		interval = pspDiffTime(&end, &start);

		dbg_printf(d, "key = %d, oldkey = %d, interval %.3f", (int) key,
				   (int) oldkey, interval);

		if (key == PSP_HPRM_FORWARD || key == PSP_HPRM_BACK
			|| key == PSP_HPRM_PLAYPAUSE) {
			if (interval >= 5.0) {
				if (key == PSP_HPRM_FORWARD) {
					dbg_printf(d, "should fforward");
				} else if (key == PSP_HPRM_BACK) {
					dbg_printf(d, "should fbackward");
				}
			}

			if (key != oldkey) {
				sceRtcGetCurrentTick(&start);
			}

			oldkey = key;

			if (key == PSP_HPRM_PLAYPAUSE && interval >= 10.0) {
				dbg_printf(d, "should power down");
			}
			sceKernelDelayThread(1000000);
		} else {
			if ((oldkey == PSP_HPRM_FORWARD || oldkey == PSP_HPRM_BACK
				 || oldkey == PSP_HPRM_PLAYPAUSE)) {
				if (interval <= 5.0) {
					if (oldkey == PSP_HPRM_FORWARD)
						dbg_printf(d, "should next");
					else if (oldkey == PSP_HPRM_BACK)
						dbg_printf(d, "should prev");
					else if (oldkey == PSP_HPRM_PLAYPAUSE)
						dbg_printf(d, "should play/pause");
				}
			}
			oldkey = key;
			sceKernelDelayThread(1000000);
			sceRtcGetCurrentTick(&start);
		}
	}

	return 0;
}
