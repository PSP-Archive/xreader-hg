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

#ifndef SCENE_IMP_H
#define SCENE_IMP_H
#include "scene.h"

extern char appdir[PATH_MAX], copydir[PATH_MAX], cutdir[PATH_MAX];
extern u32 drperpage, rowsperpage, pixelsperrow;
extern p_bookmark g_bm;
extern p_text fs;
extern p_win_menu copylist, cutlist;

extern p_win_menu g_menu;

#ifdef ENABLE_BG
extern bool repaintbg;
#endif
extern bool imgreading, locreading;
extern int locaval[10];
extern t_fonts fonts[5], bookfonts[21];
extern bool scene_readbook_in_raw_mode;

t_win_menu_op exit_confirm(void);
extern void scene_init(void);
extern void scene_exit(void);
extern void scene_power_save(bool save);
extern const char *scene_appdir(void);
u32 scene_readbook(u32 selidx);
u32 scene_readbook_raw(const char *title, const unsigned char *data, size_t size, t_fs_filetype ft);
extern void scene_mountrbkey(u32 * ctlkey, u32 * ctlkey2, u32 * ku, u32 * kd, u32 * kl, u32 * kr);
extern u32 scene_options(u32 * selidx);
extern void scene_mp3bar(void);
extern int scene_get_infobar_height(void);
extern int default_predraw(const win_menu_predraw_data * pData, const char *str,
						   int max_height, int *left, int *right, int *upper, int *bottom, int width_fixup);
extern int prompt_press_any_key(void);
extern int get_center_pos(int left, int right, const char *str);

#ifdef ENABLE_MUSIC
extern const char *get_sfx_mode_str(int effect_type);
#endif

u32 scene_txtkey(u32 * selidx);

#ifdef ENABLE_IMAGE
u32 scene_readimage(u32 selidx);
u32 scene_imgkey(u32 * selidx);
#endif
extern u32 scene_readbook(u32 selidx);

#ifdef ENABLE_IMAGE
extern u32 scene_readimage(u32 selidx);
#endif

typedef struct _BookViewData
{
	int rowtop;
	bool text_needrf, text_needrp, text_needrb;
	char filename[PATH_MAX], bookmarkname[PATH_MAX], archname[PATH_MAX];
	u32 rrow;
} BookViewData, *PBookViewData;

extern BookViewData cur_book_view, prev_book_view;

extern char prev_path[PATH_MAX], prev_shortpath[PATH_MAX];
extern char prev_lastfile[PATH_MAX];
extern int prev_where;

#endif
