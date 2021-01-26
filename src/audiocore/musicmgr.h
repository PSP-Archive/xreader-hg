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

#pragma once

#include "lyric.h"

struct _buffer;

int music_add(const char *spath, const char *lpath);
int music_add_dir(const char *spath, const char *lpath);
int music_find(const char *spath, const char *lpath);
int music_directplay(const char *spath, const char *lpath);
int music_maxindex(void);
int music_del(int i);
int music_moveup(int i);
int music_movedown(int i);
int music_list_play(void);
int music_list_stop(void);
int music_list_playorpause(void);
int music_ispaused(void);
int music_init(void);
int music_free(void);
int music_resume(void);
int music_suspend(void);
int music_prev(void);
int music_next(void);
int music_set_cycle_mode(int);
int music_list_save(const char *path);
int music_list_load(const char *path);
p_lyric music_get_lyric(void);
struct music_info;
int music_get_info(struct music_info *info);
int music_load(int i);
bool music_curr_playing();
int music_get_current_pos(void);
int music_set_hprm(bool enable);
int music_list_clear(void);
