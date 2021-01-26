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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <pspsdk.h>
#include <pspkernel.h>
#include <psprtc.h>
#include <psppower.h>
#include <unistd.h>
#include <fcntl.h>
#include "musicdrv.h"
#include "musicmgr.h"
#include "lyric.h"
#include "fat.h"
#include "strsafe.h"
#include "common/utils.h"
#include "conf.h"
#include "mpcplayer.h"
#include "wavplayer.h"
#include "flacplayer.h"
#include "ttaplayer.h"
#include "apeplayer.h"
#include "mp3player.h"
#include "oggplayer.h"
#include "wmaplayer.h"
#include "wvplayer.h"
#include "at3player.h"
#include "m4aplayer.h"
#include "aacplayer.h"
#include "aa3player.h"
#include "dbg.h"
#include "config.h"
#include "scene.h"
#include "ctrl.h"
#include "fs.h"
#include "buffer.h"
#include "musicinfo.h"
#include "power.h"
#include "image_queue.h"
#include "thread_lock.h"
#include "freq_lock.h"
#include "systemctrl.h"
#include "musiclist.h"
#include "stack.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

struct music_list
{
	unsigned curr_pos;
	int cycle_mode;
	bool is_list_playing;
	int fb_mode;
	bool first_time;
} g_list;

static int g_thread_actived = 0;
static int g_thread_exited = 0;

#ifdef ENABLE_LYRIC
static t_lyric lyric;
#endif

static bool g_music_hprm_enable = false;
static struct psp_mutex_t music_l;

static int music_play(int i);

static Stack g_played;

static inline void swap(int *a, int *b)
{
	int t;

	t = *a;
	*a = *b;
	*b = t;
}

static int shuffle_next(void)
{
	MusicListEntry *entry;

	if (g_list.cycle_mode != conf_cycle_random) {
		return 0;
	}

	stack_push(&g_played, g_list.curr_pos);
	entry = musiclist_get_next_shuffle(&g_music_list);

	if (entry == NULL) {
		musiclist_begin_shuffle(&g_music_list);
		entry = musiclist_get_next_shuffle(&g_music_list);
	}

	if (entry != NULL) {
		g_list.curr_pos = musiclist_get_idx(&g_music_list, entry);
	}

	return 0;
}

static int shuffle_prev(void)
{
	int pos;

	if (stack_pop(&g_played, &pos) < 0) {
		return -1;
	}

	g_list.curr_pos = pos;

	return 0;
}

static void music_lock(void)
{
	xr_lock(&music_l);
}

static void music_unlock(void)
{
	xr_unlock(&music_l);
}

int music_add(const char *spath, const char *lpath)
{
	int pos;

	if (spath == NULL || lpath == NULL)
		return -EINVAL;

	if (!fs_is_music(spath, lpath))
		return -EINVAL;

	pos = music_find(spath, lpath);

	if (pos >= 0) {
		return -1;
	}

	return musiclist_add(&g_music_list, spath, lpath);
}

int music_find(const char *spath, const char *lpath)
{
	MusicListEntry *e;

	e = musiclist_search_by_path(&g_music_list, spath, lpath);

	if (e == NULL) {
		return -1;
	}

	return musiclist_get_idx(&g_music_list, e);
}

int music_maxindex(void)
{
	return musiclist_count(&g_music_list);
}

int music_stop(void)
{
	int ret;

	ret = musicdrv_get_status();
	if (ret < 0)
		return ret;

	if (ret == ST_PLAYING || ret == ST_PAUSED || ret == ST_LOADED || ret == ST_STOPPED || ret == ST_FFORWARD || ret == ST_FBACKWARD) {
		ret = musicdrv_end();
	} else
		ret = 0;

	return ret;
}

int music_fforward(int sec)
{
	int ret;

	ret = musicdrv_get_status();
	if (ret < 0)
		return ret;

	if (ret == ST_PLAYING || ret == ST_PAUSED)
		ret = musicdrv_fforward(sec);
	else
		ret = 0;

	return ret;
}

int music_fbackward(int sec)
{
	int ret;

	ret = musicdrv_get_status();
	if (ret < 0)
		return ret;

	if (ret == ST_PLAYING || ret == ST_PAUSED)
		ret = musicdrv_fbackward(sec);
	else
		ret = 0;

	return ret;
}

static int music_setupdriver(const char *spath, const char *lpath)
{
	struct music_ops *ops = musicdrv_chk_file(spath);
	struct music_ops *dev = NULL;

	if (ops) {
		dev = set_musicdrv_by_name(ops->name);
	} else {
		ops = musicdrv_chk_file(lpath);

		if (ops) {
			dev = set_musicdrv_by_name(ops->name);
		}
	}

	if (dev != ops) {
		dbg_printf(d, "%s: set musicdrv dev (0x%08x) != ops(0x%08x), but they have same name.", __func__, (unsigned int) dev, (unsigned int) ops);
	}

	return dev ? 0 : -ENODEV;
}

static int music_load_config(void)
{
	musicdrv_set_opt("", config.musicdrv_opts);

	return 0;
}

static int music_play(int i)
{
	int ret;

	ret = music_load(i);

	if (ret < 0) {
		return ret;
	}

	ret = musicdrv_play();

	return ret;
}

int music_directplay(const char *spath, const char *lpath)
{
	int pos;
	int ret;

	music_lock();
	music_add(spath, lpath);
	pos = music_find(spath, lpath);

	if (pos < 0) {
		music_unlock();
		return -1;
	}

	g_list.curr_pos = pos;
	ret = music_play(pos);

	if (ret == 0) {
		g_list.is_list_playing = true;
		g_list.first_time = false;
	}

	music_unlock();
	return 0;
}

static int music_list_refresh(void)
{
	music_lock();
	g_list.curr_pos = 0;

	if (music_maxindex() > 0) {
		if (g_list.is_list_playing) {
			if (music_play(g_list.curr_pos) < 0) {
				g_list.is_list_playing = false;
			}
		} else {
			// only load information
			if (music_load(g_list.curr_pos) >= 0) {
				musicdrv_end();
			}
		}
	} else {
		musicdrv_end();
		set_musicdrv(NULL);
	}

	g_list.first_time = true;
	music_unlock();

	return 0;
}

int music_del(int i)
{
	music_lock();
	musiclist_remove(&g_music_list, musiclist_get(&g_music_list, i));
	music_unlock();

	return 0;
}

int music_moveup(int i)
{
	music_lock();
	musiclist_moveup(&g_music_list, i);
	music_unlock();

	return 0;
}

int music_movedown(int i)
{
	music_lock();
	musiclist_movedown(&g_music_list, i);
	music_unlock();

	return 0;
}

static int get_next_music(void)
{
	switch (g_list.cycle_mode) {
		case conf_cycle_single:
			{
				if (g_list.curr_pos == music_maxindex() - 1) {
					g_list.curr_pos = 0;
					g_list.is_list_playing = false;
					g_list.first_time = true;
				} else {
					g_list.curr_pos++;
				}
				break;
			}
		case conf_cycle_repeat:
			{
				if (g_list.curr_pos == music_maxindex() - 1) {
					g_list.curr_pos = 0;
				} else {
					g_list.curr_pos++;
				}
				break;
			}
		case conf_cycle_repeat_one:
			{
				break;
			}
		case conf_cycle_random:
			shuffle_next();
			break;
		default:
			break;
	}

	return 0;
}

int music_list_play(void)
{
	music_lock();

	if (!g_list.is_list_playing)
		g_list.is_list_playing = true;

	music_unlock();
	return 0;
}

int music_list_stop(void)
{
	int ret;

	music_lock();
	ret = music_stop();

	if (ret < 0) {
		music_unlock();
		return ret;
	}

	g_list.is_list_playing = false;
	g_list.first_time = true;
	music_unlock();
	return ret;
}

int music_list_playorpause(void)
{
	music_lock();
	if (!g_list.is_list_playing)
		g_list.is_list_playing = true;
	else {
		int ret = musicdrv_get_status();

		if (ret < 0) {
			music_unlock();
			return ret;
		}
		if (ret == ST_PLAYING)
			musicdrv_pause();
		else if (ret == ST_PAUSED)
			musicdrv_play();
	}
	music_unlock();

	return 0;
}

int music_ispaused(void)
{
	int ret = musicdrv_get_status();

	if (ret == ST_PAUSED)
		return 1;
	else
		return 0;
}

static SceUID g_music_thread = -1;

static int musicdrv_has_stop(void)
{
	int ret;

	ret = musicdrv_get_status();
	if (ret == ST_STOPPED || ret == ST_UNKNOWN || ret == ST_LOADED) {
		return true;
	}

	return false;
}

static int music_thread(SceSize arg, void *argp)
{
	u32 key = 0;
	u32 oldkey = 0;
	u64 start, end;
	double interval = 0;

	g_thread_actived = 1;
	g_thread_exited = 0;

	sceRtcGetCurrentTick(&start);
	sceRtcGetCurrentTick(&end);

	while (g_thread_actived) {
		music_lock();
		if (g_list.is_list_playing) {
			if (musicdrv_has_stop()) {
				if (g_list.first_time) {
					int ret;

					ret = music_play(g_list.curr_pos);
					if (ret == 0)
						g_list.first_time = false;
				} else {
					get_next_music();

					if (!g_list.is_list_playing) {
						music_unlock();
						music_load(g_list.curr_pos);
						music_stop();
						continue;
					}

					music_play(g_list.curr_pos);
				}
			}

			music_unlock();
			sceKernelDelayThread(100000);
		} else {
			music_unlock();
			sceKernelDelayThread(500000);
		}

		if (g_music_hprm_enable) {
			key = ctrl_hprm_raw();
			sceRtcGetCurrentTick(&end);
			interval = pspDiffTime(&end, &start);

			if (key == PSP_HPRM_FORWARD || key == PSP_HPRM_BACK || key == PSP_HPRM_PLAYPAUSE) {
				if (key != oldkey) {
					sceRtcGetCurrentTick(&start);
					sceRtcGetCurrentTick(&end);
					interval = pspDiffTime(&end, &start);
				}

				if (interval >= 0.5) {
					if (key == PSP_HPRM_FORWARD) {
						musicdrv_fforward(5);
						sceKernelDelayThread(200000);
					} else if (key == PSP_HPRM_BACK) {
						musicdrv_fbackward(5);
						sceKernelDelayThread(200000);
					}
				}

				oldkey = key;

				if (key == PSP_HPRM_PLAYPAUSE && interval >= 4.0) {
					power_down();
					scePowerRequestSuspend();
				}
			} else {
				if ((oldkey == PSP_HPRM_FORWARD || oldkey == PSP_HPRM_BACK || oldkey == PSP_HPRM_PLAYPAUSE)) {
					if (interval < 0.5) {
						if (oldkey == PSP_HPRM_FORWARD)
							music_next();
						else if (oldkey == PSP_HPRM_BACK)
							music_prev();
					}

					if (interval < 4.0) {
						if (oldkey == PSP_HPRM_PLAYPAUSE)
							music_list_playorpause();
					}
				}
				oldkey = key;
				sceRtcGetCurrentTick(&start);
			}
		}

		{
			int thid = sceKernelGetThreadId();
			int oldpri = sceKernelGetThreadCurrentPriority();

			sceKernelChangeThreadPriority(thid, 90);
			cache_routine();
			sceKernelChangeThreadPriority(thid, oldpri);
		}
	}

	g_thread_actived = 0;
	g_thread_exited = 1;

	return 0;
}

int music_init(void)
{
	u32 seed;

	cache_init();

	seed = sctrlKernelRand();

	if (seed == 0x8002013A) {
		pspTime tm;

		sceRtcGetCurrentClockLocalTime(&tm);
		seed = tm.microseconds;
	}

	srand(seed);
	xr_lock_init(&music_l);

#ifdef ENABLE_MPC
	mpc_init();
#endif

#ifdef ENABLE_WAV
	wav_init();
#endif

#ifdef ENABLE_TTA
	tta_init();
#endif

#ifdef ENABLE_APE
	ape_init();
#endif

#ifdef ENABLE_MP3
	mp3_init();
#endif

#ifdef ENABLE_FLAC
	flac_init();
#endif

#ifdef ENABLE_OGG
	ogg_init();
#endif

#ifdef ENABLE_WMA
	wmadrv_init();
#endif

#ifdef ENABLE_WAVPACK
	wv_init();
#endif

#ifdef ENABLE_AT3
	at3_init();
#endif

#ifdef ENABLE_M4A
	m4a_init();
#endif

#ifdef ENABLE_AAC
	aac_init();
#endif

#ifdef ENABLE_AA3
	aa3_init();
#endif

	musiclist_init(&g_music_list);
	memset(&g_list, 0, sizeof(g_list));
	g_list.first_time = true;
	stack_init(&g_played);
	g_music_thread = sceKernelCreateThread("Music Thread", music_thread, 0x12, 0x10000, 0, NULL);

	if (g_music_thread < 0) {
		return -EBUSY;
	}

	sceKernelStartThread(g_music_thread, 0, 0);

	return 0;
}

int music_free(void)
{
	int ret;
	unsigned to = 500000;

	cache_on(false);

	music_lock();

	ret = -1;

	while (ret != 0) {
		ret = music_stop();
		if (ret != 0)
			sceKernelDelayThread(100000);
	}

	g_list.is_list_playing = 0;
	g_thread_actived = 0;
	music_unlock();

	if (sceKernelWaitThreadEnd(g_music_thread, &to) != 0) {
		sceKernelTerminateDeleteThread(g_music_thread);
	} else {
		sceKernelDeleteThread(g_music_thread);
	}

	xr_lock_destroy(&music_l);

	return 0;
}

int music_prev(void)
{
	int ret = 0;

	music_lock();

	switch (g_list.cycle_mode) {
		case conf_cycle_single:
			{
				if (g_list.curr_pos == 0)
					g_list.curr_pos = music_maxindex() - 1;
				else
					g_list.curr_pos--;
				break;
			}
		case conf_cycle_repeat:
			{
				if (g_list.curr_pos == 0)
					g_list.curr_pos = music_maxindex() - 1;
				else
					g_list.curr_pos--;
				break;
			}
			break;
		case conf_cycle_repeat_one:
			{
				if (g_list.curr_pos == 0)
					g_list.curr_pos = music_maxindex() - 1;
				else
					g_list.curr_pos--;
				break;
			}
			break;
		case conf_cycle_random:
			{
				if (shuffle_prev() != 0) {
					ret = music_stop();
					g_list.is_list_playing = false;
					music_unlock();
					return ret;
				}

				break;
			}
			break;
	}

	if (!g_list.is_list_playing)
		g_list.is_list_playing = true;

	if (g_list.is_list_playing)
		ret = music_play(g_list.curr_pos);

	music_unlock();

	return ret;
}

int music_next(void)
{
	int ret;

	music_lock();

	switch (g_list.cycle_mode) {
		case conf_cycle_single:
			{
				if (g_list.curr_pos == music_maxindex() - 1)
					g_list.curr_pos = 0;
				else
					g_list.curr_pos++;
				break;
			}
		case conf_cycle_repeat:
			{
				if (g_list.curr_pos == music_maxindex() - 1)
					g_list.curr_pos = 0;
				else
					g_list.curr_pos++;
				break;
			}
		case conf_cycle_repeat_one:
			{
				if (g_list.curr_pos == music_maxindex() - 1)
					g_list.curr_pos = 0;
				else
					g_list.curr_pos++;
				break;
			}
		case conf_cycle_random:
			{
				shuffle_next();
				break;
			}
	}

	if (!g_list.is_list_playing)
		g_list.is_list_playing = true;

	ret = music_play(g_list.curr_pos);

	music_unlock();
	return ret;
}

int music_add_dir(const char *spath, const char *lpath)
{
	p_fat_info info;
	u32 i, count;

	if (spath == NULL || lpath == NULL)
		return -EINVAL;

	count = fat_readdir(lpath, (char *) spath, &info);

	if (count == INVALID)
		return -EBADF;
	for (i = 0; i < count; i++) {
		char sfn[PATH_MAX];
		char lfn[PATH_MAX];

		if ((info[i].attr & FAT_FILEATTR_DIRECTORY) > 0) {
			char lpath2[PATH_MAX], spath2[PATH_MAX];

			if (info[i].filename[0] == '.')
				continue;

			SPRINTF_S(lpath2, "%s%s/", lpath, info[i].longname);
			SPRINTF_S(spath2, "%s%s/", spath, info[i].filename);
			music_add_dir(spath2, lpath2);
			continue;
		}

		if (fs_is_music(info[i].filename, info[i].longname) == false)
			continue;

		SPRINTF_S(sfn, "%s%s", spath, info[i].filename);
		SPRINTF_S(lfn, "%s%s", lpath, info[i].longname);
		music_add(sfn, lfn);
	}
	free((void *) info);

	return 0;
}

#ifdef ENABLE_LYRIC
p_lyric music_get_lyric(void)
{
	struct music_info info;

	info.type = MD_GET_CURTIME;
	if (music_get_info(&info) == 0) {
		xr_timer_t timer;

		timer.seconds = (int) info.cur_time;
		timer.fraction = (unsigned long) ((info.cur_time - (long) info.cur_time) * XR_TIMER_RESOLUTION);
		lyric_update_pos(&lyric, &timer);
	}

	return &lyric;
}
#endif

int music_list_save(const char *path)
{
	FILE *fp;
	int i;
	int fid;

	if (path == NULL)
		return -EINVAL;

	fp = fopen(path, "wt");
	if (fp == NULL)
		return -EBADFD;

	fid = freq_enter_hotzone();
	for (i = 0; i < music_maxindex(); i++) {
		MusicListEntry *file;

		file = musiclist_get(&g_music_list, i);
		if (file == NULL)
			continue;
		fprintf(fp, "%s\n", file->spath);
		fprintf(fp, "%s\n", file->lpath);
	}
	fclose(fp);
	freq_leave(fid);

	return true;
}

static bool music_is_file_exist(const char *filename)
{
	SceIoStat sta;

	if (sceIoGetstat(filename, &sta) < 0) {
		return false;
	}

	return true;
}

int music_list_clear(void)
{
	music_lock();
	musiclist_clear(&g_music_list);
	music_unlock();

	return 0;
}

int music_list_load(const char *path)
{
	int ret = 0;
	FILE *fp;
	char fname[PATH_MAX];
	char lfname[PATH_MAX];
	int fid;

	if (path == NULL) {
		ret = -EINVAL;
		goto end;
	}

	fp = fopen(path, "rt");

	if (fp == NULL) {
		ret = -EBADFD;
		goto end;
	}

	fid = freq_enter_hotzone();

	while (fgets(fname, PATH_MAX, fp) != NULL) {
		u32 len1, len2;

		if (fgets(lfname, PATH_MAX, fp) == NULL)
			break;

		len1 = strlen(fname);
		len2 = strlen(lfname);

		if (len1 > 1 && len2 > 1) {
			if (fname[len1 - 1] == '\n')
				fname[len1 - 1] = 0;
			if (lfname[len2 - 1] == '\n')
				lfname[len2 - 1] = 0;
		} else
			continue;
		if (!music_is_file_exist(fname))
			continue;
		music_add(fname, lfname);
	}

	fclose(fp);
	freq_leave(fid);

  end:
	music_list_refresh();

	return ret;
}

int music_set_cycle_mode(int mode)
{
	int prev;

	music_lock();
	prev = g_list.cycle_mode;

	if (mode >= 0 && mode <= conf_cycle_random)
		g_list.cycle_mode = mode;

	if (prev != g_list.cycle_mode && (g_list.cycle_mode == conf_cycle_random || prev == conf_cycle_random)) {
		stack_clear(&g_played);
	}

	music_unlock();
	return g_list.cycle_mode;
}

int music_get_info(struct music_info *info)
{
	int ret;

	if (info == NULL)
		return -EINVAL;

	ret = musicdrv_get_status();

	if (ret != ST_UNKNOWN) {
		return musicdrv_get_info(info);
	}

	return -EBUSY;
}

int music_load(int i)
{
	MusicListEntry *file = musiclist_get(&g_music_list, i);
	int ret;
	char lyricname[PATH_MAX];
	char lyricshortname[PATH_MAX];

	if (file == NULL)
		return -EINVAL;
	ret = music_stop();
	if (ret < 0)
		return ret;
	ret = music_setupdriver(file->spath, file->lpath);
	if (ret < 0)
		return ret;
	ret = music_load_config();
	if (ret < 0)
		return ret;
	ret = musicdrv_load(file->spath, file->lpath);
	if (ret < 0)
		return ret;
#ifdef ENABLE_LYRIC
	lyric_close(&lyric);

	if (tag_lyric && tag_lyric->used != 0) {
		lyric_open_raw(&lyric, tag_lyric->ptr, tag_lyric->used);
		buffer_free(tag_lyric);
		tag_lyric = NULL;
	} else {
		int lsize;

		strncpy_s(lyricname, NELEMS(lyricname), file->lpath, PATH_MAX);
		lsize = strlen(lyricname);

		lyricname[lsize - 3] = 'l';
		lyricname[lsize - 2] = 'r';
		lyricname[lsize - 1] = 'c';
		if (fat_longnametoshortname(lyricshortname, lyricname, PATH_MAX))
			lyric_open(&lyric, lyricshortname);
	}
#endif

	return ret;
}

bool music_curr_playing()
{
	int ret = musicdrv_get_status();

	if (ret == ST_PLAYING)
		return true;
	return false;
}

int music_get_current_pos(void)
{
	return g_list.curr_pos;
}

int music_set_hprm(bool enable)
{
	g_music_hprm_enable = enable;

	return 0;
}

static int prev_is_playing = 0;

int music_suspend(void)
{
	int ret;

	dbg_printf(d, "%s", __func__);

	music_lock();
	ret = musicdrv_suspend();

	if (ret < 0) {
		dbg_printf(d, "%s: Suspend failed!", __func__);
		musicdrv_end();
		music_unlock();
		return ret;
	}

	prev_is_playing = g_list.is_list_playing;
	g_list.is_list_playing = 0;

	// now music module is locked
	return 0;
}

int music_resume(void)
{
	MusicListEntry *fl = musiclist_get(&g_music_list, g_list.curr_pos);
	int ret;

	dbg_printf(d, "%s", __func__);

	if (fl == NULL) {
		dbg_printf(d, "%s: Resume failed!", __func__);
		music_unlock();

		return -1;
	}

	ret = musicdrv_resume(fl->spath, fl->lpath);

	if (ret < 0) {
		dbg_printf(d, "%s %d: Resume failed!", __func__, __LINE__);
		music_unlock();

		return ret;
	}

	g_list.is_list_playing = prev_is_playing;
	music_unlock();

	return 0;
}
