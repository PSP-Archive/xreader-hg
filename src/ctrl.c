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

#include <pspkernel.h>
#include <time.h>
#include "display.h"
#include "conf.h"
#ifdef ENABLE_MUSIC
#include "audiocore/musicmgr.h"
#ifdef ENABLE_LYRIC
#include "audiocore/lyric.h"
#endif
#endif
#include "ctrl.h"
#include "thread_lock.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

#define CTRL_REPEAT_TIME 0x40000
static unsigned int last_btn = 0;
static unsigned int last_tick = 0;

#ifdef ENABLE_HPRM
static bool hprmenable = false;
static u32 lasthprmkey = 0;
static u32 lastkhprmkey = 0;
static struct psp_mutex_t hprm_l;
#endif

extern void ctrl_init(void)
{
	sceCtrlSetSamplingCycle(0);
#ifdef ENABLE_ANALOG
	sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
#else
	sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);
#endif
#ifdef ENABLE_HPRM
	xr_lock_init(&hprm_l);
#endif
}

extern void ctrl_destroy(void)
{
#ifdef ENABLE_HPRM
	xr_lock_destroy(&hprm_l);
#endif
}

#ifdef ENABLE_ANALOG
extern bool ctrl_analog(int *x, int *y)
{
	SceCtrlData ctl;

	sceCtrlReadBufferPositive(&ctl, 1);

	if (ctl.Buttons & PSP_CTRL_HOLD) {
		return false;
	}

	*x = ((int) ctl.Lx) - 128;
	*y = ((int) ctl.Ly) - 128;

	if (*x < -63 || *x > 63 || *y < -63 || *y > 63) {
		return true;
	}

	return false;
}
#endif

extern u32 ctrl_read_cont(void)
{
	SceCtrlData ctl;

	sceCtrlReadBufferPositive(&ctl, 1);

#ifdef ENABLE_HPRM
	if (hprmenable && sceHprmIsRemoteExist()) {
		u32 key;

		if (xr_lock(&hprm_l) >= 0) {
			sceHprmPeekCurrentKey(&key);
			xr_unlock(&hprm_l);

			if (key > 0) {
				switch (key) {
					case PSP_HPRM_FORWARD:
						if (key == lastkhprmkey)
							break;
						lastkhprmkey = key;
						return CTRL_FORWARD;
					case PSP_HPRM_BACK:
						if (key == lastkhprmkey)
							break;
						lastkhprmkey = key;
						return CTRL_BACK;
					case PSP_HPRM_PLAYPAUSE:
						if (key == lastkhprmkey)
							break;
						lastkhprmkey = key;
						return CTRL_PLAYPAUSE;
				}
			} else
				lastkhprmkey = 0;
		}
	}
#endif

	last_btn = ctl.Buttons;
	last_tick = ctl.TimeStamp;

	return last_btn;
}

extern u32 ctrl_read(void)
{
	SceCtrlData ctl;

#ifdef ENABLE_HPRM
	if (hprmenable && sceHprmIsRemoteExist()) {
		u32 key;

		sceHprmPeekCurrentKey(&key);

		if (key > 0) {
			switch (key) {
				case PSP_HPRM_FORWARD:
					if (key == lastkhprmkey)
						break;
					lastkhprmkey = key;
					return CTRL_FORWARD;
				case PSP_HPRM_BACK:
					if (key == lastkhprmkey)
						break;
					lastkhprmkey = key;
					return CTRL_BACK;
				case PSP_HPRM_PLAYPAUSE:
					if (key == lastkhprmkey)
						break;
					lastkhprmkey = key;
					return CTRL_PLAYPAUSE;
			}
		} else
			lastkhprmkey = 0;
	}
#endif

	sceCtrlReadBufferPositive(&ctl, 1);

	if (ctl.Buttons == last_btn) {
		if (ctl.TimeStamp - last_tick < CTRL_REPEAT_TIME)
			return 0;
		return last_btn;
	}

	last_btn = ctl.Buttons;
	last_tick = ctl.TimeStamp;
	return last_btn;
}

extern void ctrl_waitreleaseintime(int i)
{
	SceCtrlData ctl;

	do {
		sceCtrlReadBufferPositive(&ctl, 1);
		sceKernelDelayThread(i);
	} while (ctl.Buttons != 0);
}

extern void ctrl_waitrelease(void)
{
	return ctrl_waitreleaseintime(20000);
}

extern int ctrl_waitreleasekey(u32 key)
{
	SceCtrlData pad;

	sceCtrlReadBufferPositive(&pad, 1);
	while (pad.Buttons == key) {
		sceKernelDelayThread(50000);
		sceCtrlReadBufferPositive(&pad, 1);
	}

	return 0;
}

extern u32 ctrl_waitany(void)
{
	u32 key;

	while ((key = ctrl_read()) == 0) {
		sceKernelDelayThread(50000);
	}
	return key;
}

extern u32 ctrl_waitkey(u32 key_wait)
{
	u32 key;

	while ((key = ctrl_read()) != key_wait) {
		sceKernelDelayThread(50000);
	}

	return key;
}

extern u32 ctrl_waitmask(u32 keymask)
{
	u32 key;

	while (((key = ctrl_read()) & keymask) == 0) {
		sceKernelDelayThread(50000);
	}
	return key;
}

#if defined(ENABLE_MUSIC) && defined(ENABLE_LYRIC)
extern u32 ctrl_waitlyric(void)
{
	u32 key;

	while ((key = ctrl_read()) == 0) {
		sceKernelDelayThread(50000);
		if (lyric_check_changed(music_get_lyric()))
			break;
	}
	return key;
}
#endif

#ifdef ENABLE_HPRM
extern u32 ctrl_hprm(void)
{
	u32 key = ctrl_hprm_raw();

	if (key == lasthprmkey)
		return 0;

	lasthprmkey = key;
	return (u32) key;
}

extern u32 ctrl_hprm_raw(void)
{
	u32 key = 0;

/*	if(psp_fw_version >= 0x02000010)
		return 0;*/
	if (!sceHprmIsRemoteExist())
		return 0;

	if (xr_lock(&hprm_l) >= 0) {
		sceHprmPeekCurrentKey(&key);
		xr_unlock(&hprm_l);
	}

	return (u32) key;
}
#endif

extern u32 ctrl_waittime(u32 t)
{
	u32 key;
	time_t t1 = time(NULL);

	while ((key = ctrl_read()) == 0) {
		sceKernelDelayThread(50000);
		if (time(NULL) - t1 >= t)
			return 0;
	}
	return key;
}

#ifdef ENABLE_HPRM
extern void ctrl_enablehprm(bool enable)
{
	hprmenable = /*(psp_fw_version < 0x02000010) && */ enable;
}
#endif
