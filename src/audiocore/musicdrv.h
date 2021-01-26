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

#ifndef MUSICDRV_H
#define MUSICDRV_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "conf.h"

#define MD_GET_TITLE       (1 << 0)
#define MD_GET_ALBUM       (1 << 1)
#define MD_GET_ARTIST      (1 << 2)
#define MD_GET_COMMENT     (1 << 3)
#define MD_GET_CURTIME     (1 << 4)
#define MD_GET_DURATION    (1 << 5)
#define MD_GET_CPUFREQ     (1 << 6)
#define MD_GET_FREQ        (1 << 7)
#define MD_GET_DECODERNAME (1 << 8)
#define MD_GET_ENCODEMSG   (1 << 9)
#define MD_GET_AVGKBPS     (1 << 10)
#define MD_GET_INSKBPS     (1 << 11)
#define MD_GET_CHANNELS    (1 << 12)
#define MD_GET_LYRIC	   (1 << 13)

#define INFO_STR_SIZE 80

	struct music_info
	{
		int type;
		t_conf_encode encode;

		char title[INFO_STR_SIZE];
		char album[INFO_STR_SIZE];
		char artist[INFO_STR_SIZE];
		char comment[INFO_STR_SIZE];
		char decoder_name[INFO_STR_SIZE];
		char encode_msg[INFO_STR_SIZE];

		float cur_time;
		float duration;
		int freq;
		int channels;
		int psp_freq[2];
		int avg_kbps;
		int ins_kbps;

		int file_handle;
		int channel_handle;
	};

/* get_status returns: */
	enum
	{
		ST_LOADED = 0,
		ST_PAUSED = 1,
		ST_PLAYING = 2,
		ST_STOPPED = 3,			/* In this status the alloctated resources were freed */
		ST_FFORWARD = 4,
		ST_FBACKWARD = 5,
		ST_SUSPENDED = 6,
		ST_UNKNOWN = 7
	};

	struct music_ops
	{
		const char *name;
		int (*set_opt) (const char *key, const char *value);
		int (*load) (const char *spath, const char *lpath);
		int (*play) (void);
		int (*pause) (void);
		int (*fforward) (int);
		int (*fbackward) (int);
		int (*get_status) (void);
		int (*get_info) (struct music_info *);
		int (*suspend) (void);
		int (*resume) (const char *spath, const char *lpath);
		int (*end) (void);
		int (*probe) (const char *spath);

		struct music_ops *next;
	};

	int register_musicdrv(struct music_ops *drv);
	int unregister_musicdrv(struct music_ops *drv);
	struct music_ops *get_musicdrv(const char *name);
	struct music_ops *set_musicdrv(struct music_ops *musicdrv);
	struct music_ops *set_musicdrv_by_name(const char *name);
	int musicdrv_maxindex(void);
	int musicdrv_set_opt(const char *key, const char *value);
	int musicdrv_load(const char *spath, const char *lpath);
	int musicdrv_play(void);
	int musicdrv_pause(void);
	int musicdrv_end(void);
	int musicdrv_fforward(int);
	int musicdrv_fbackward(int);
	int musicdrv_get_status(void);
	int musicdrv_suspend(void);
	int musicdrv_resume(const char *spath, const char *lpath);
	int musicdrv_get_info(struct music_info *info);
	struct music_ops *musicdrv_chk_file(const char *name);

	bool opt_is_on(const char *str);

	struct instant_bitrate_frame
	{
		size_t framebits;
		float duration;
	};

	struct instant_bitrate
	{
		size_t n, cap;
		struct instant_bitrate_frame *frames;
	};

	int get_inst_bitrate(struct instant_bitrate *inst);
	float get_bitrate_second(struct instant_bitrate *inst);
	void add_bitrate(struct instant_bitrate *inst, int frame_bits, double duration);
	void free_bitrate(struct instant_bitrate *inst);

	extern struct instant_bitrate g_inst_br;
#ifdef __cplusplus
}
#endif

#endif
