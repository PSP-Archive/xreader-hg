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

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pspkernel.h>
#include "musicdrv.h"
#include "genericplayer.h"
#include "freq_lock.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

static struct music_ops *music_drivers = NULL;
static struct music_ops *cur_musicdrv = NULL;
static bool need_stop = false;

/**
 * MP3动态比特率
 */
struct instant_bitrate g_inst_br;

int register_musicdrv(struct music_ops *drv)
{
	struct music_ops **tmp;

	if (drv == NULL) {
		return -EINVAL;
	}
	if (drv->next != NULL) {
		return -EBUSY;
	}
	tmp = &music_drivers;
	while (*tmp) {
		if (strcmp((*tmp)->name, drv->name) == 0)
			return -EBUSY;
		tmp = &(*tmp)->next;
	}
	*tmp = drv;
	return 0;
}

int unregister_musicdrv(struct music_ops *drv)
{
	struct music_ops **tmp;

	tmp = &music_drivers;
	while (*tmp) {
		if (drv == *tmp) {
			*tmp = drv->next;
			drv->next = NULL;
			return 0;
		}
		tmp = &(*tmp)->next;
	}
	return -EINVAL;
}

int musicdrv_maxindex(void)
{
	struct music_ops *tmp;
	int count;

	count = 0;
	for (tmp = music_drivers; tmp; tmp = tmp->next)
		count++;

	return count;
}

struct music_ops *musicdrv_chk_file(const char *name)
{
	struct music_ops *tmp;

	if (name == NULL)
		return NULL;

	for (tmp = music_drivers; tmp; tmp = tmp->next) {
		if (tmp->probe(name)) {
			return tmp;
		}
	}

	return NULL;
}

struct music_ops *get_musicdrv(const char *name)
{
	struct music_ops *tmp;

	if (name == NULL)
		return cur_musicdrv;

	for (tmp = music_drivers; tmp && strcmp(name, tmp->name); tmp = tmp->next);

	return tmp;
}

struct music_ops *set_musicdrv(struct music_ops *musicdrv)
{
	cur_musicdrv = musicdrv;

	return cur_musicdrv;
}

struct music_ops *set_musicdrv_by_name(const char *name)
{
	cur_musicdrv = get_musicdrv(name);

	return cur_musicdrv;
}

int musicdrv_set_opt(const char *key, const char *value)
{
	if (key == NULL || value == NULL)
		return -EINVAL;
	if (cur_musicdrv == NULL || cur_musicdrv->set_opt == NULL)
		return generic_set_opt(key, value);

	generic_set_opt(key, value);

	return cur_musicdrv->set_opt(key, value);
}

int musicdrv_load(const char *spath, const char *lpath)
{
	if (spath == NULL || lpath == NULL)
		return -EINVAL;
	if (cur_musicdrv == NULL)
		return -EBUSY;
	if (cur_musicdrv->load) {
		int ret, fid;

		fid = freq_enter_hotzone();
		ret = cur_musicdrv->load(spath, lpath);
		freq_leave(fid);

		need_stop = true;

		return ret;
	} else
		return -ENOSYS;
}

int musicdrv_play(void)
{
	if (cur_musicdrv == NULL || cur_musicdrv->play == NULL)
		return generic_play();

	return cur_musicdrv->play();
}

int musicdrv_pause(void)
{
	if (cur_musicdrv == NULL || cur_musicdrv->pause == NULL)
		return generic_pause();

	return cur_musicdrv->pause();
}

int musicdrv_end(void)
{
	if (!need_stop)
		return 0;
	if (cur_musicdrv == NULL || cur_musicdrv->end == NULL)
		return 0;

	return cur_musicdrv->end();
}

int musicdrv_fforward(int second)
{
	if (cur_musicdrv == NULL || cur_musicdrv->fforward == NULL)
		return generic_fforward(second);

	return cur_musicdrv->fforward(second);
}

int musicdrv_fbackward(int second)
{
	if (cur_musicdrv == NULL || cur_musicdrv->fbackward == NULL)
		return generic_fbackward(second);

	return cur_musicdrv->fbackward(second);
}

int musicdrv_get_status(void)
{
	if (cur_musicdrv == NULL || cur_musicdrv->get_status == NULL)
		return generic_get_status();

	return cur_musicdrv->get_status();
}

int musicdrv_suspend(void)
{
	if (cur_musicdrv == NULL)
		return 0;

	if (cur_musicdrv->suspend)
		return cur_musicdrv->suspend();
	else
		return -ENOSYS;
}

int musicdrv_resume(const char *spath, const char *lpath)
{
	if (cur_musicdrv == NULL)
		return -EBUSY;
	if (cur_musicdrv->resume)
		return cur_musicdrv->resume(spath, lpath);
	else
		return -ENOSYS;
}

int musicdrv_get_info(struct music_info *info)
{
	if (info == NULL)
		return -EINVAL;
	if (cur_musicdrv == NULL)
		return -EBUSY;
	if (cur_musicdrv->get_info)
		return cur_musicdrv->get_info(info);
	else
		return -ENOSYS;
}

int free_musicinfo(struct music_info *info)
{
	return 0;
}

bool opt_is_on(const char *str)
{
	size_t slen;

	if (str == NULL)
		return false;

	slen = strlen(str);

#define CHECK_TAIL(tailstr) \
	do { \
		if (slen > (sizeof(tailstr) - 1) \
				&& !strncmp(&str[slen- (sizeof(tailstr) - 1)], tailstr, (sizeof(tailstr) - 1))) \
		return true; }\
	while ( 0 )

	CHECK_TAIL("=y");
	CHECK_TAIL("=yes");
	CHECK_TAIL("=1");
	CHECK_TAIL("=on");

#undef CHECK_TAIL

	return false;
}

int get_inst_bitrate(struct instant_bitrate *inst)
{
	size_t i;
	int r = 0;

	if (inst == NULL)
		return 0;

	if (inst->n == 0)
		return 0;

	for (i = 0; i < inst->n; ++i) {
		r += inst->frames[i].framebits;
	}

	r /= inst->n;

	return r;
}

float get_bitrate_second(struct instant_bitrate *inst)
{
	size_t i;
	float r = 0;

	if (inst == NULL)
		return 0;

	for (i = 0; i < inst->n; ++i) {
		r += inst->frames[i].duration;
	}

	return r;
}

void add_bitrate(struct instant_bitrate *inst, int frame_bits, double duration)
{
	if (inst == NULL)
		return;

	if (get_bitrate_second(inst) <= 1.000) {
		if (inst->frames == NULL || inst->n >= inst->cap) {
			struct instant_bitrate_frame *p = realloc(inst->frames, (inst->cap + 10) * sizeof(*inst->frames));

			if (p == NULL)
				return;

			inst->frames = p;
			inst->cap += 10;
		}

		inst->frames[inst->n].framebits = frame_bits;
		inst->frames[inst->n].duration = duration;

		inst->n++;
	} else {
		memmove(&inst->frames[0], &inst->frames[1], sizeof(*inst->frames) * (inst->n - 1));
		inst->frames[inst->n - 1].framebits = frame_bits;
		inst->frames[inst->n - 1].duration = duration;
	}
}

void free_bitrate(struct instant_bitrate *inst)
{
	if (inst == NULL)
		return;

	if (inst->frames != NULL) {
		free(inst->frames);
		inst->frames = NULL;
	}

	memset(inst, 0, sizeof(*inst));
}
