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

#ifndef FREQ_LOCK_H
#define FREQ_LOCK_H

int freq_init();
int freq_free();
int freq_update(void);
int freq_enter(int cpu, int bus);

/**
 * 离开频率热区
 *
 * @param freq区ID
 *
 * @return 0
 */
int freq_leave(int freq_id);

/**
 * 进入高频率热区
 *
 * @note 如果正在播放音乐,将进入最高频率,否则将进入普通工作频率
 *
 * @return freq区ID
 */
int freq_enter_hotzone(void);

int dbg_freq();

#endif
