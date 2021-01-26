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

#ifndef _CONF_H_
#define _CONF_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <psptypes.h>
#include "common/datatype.h"

	typedef enum
	{
		conf_encode_gbk = 0,
		conf_encode_big5 = 1,
		conf_encode_sjis = 2,
		conf_encode_ucs = 3,
		conf_encode_utf8 = 4
	} t_conf_encode;

	typedef enum
	{
		conf_fit_none = 0,
		conf_fit_width = 1,
		conf_fit_dblwidth = 2,
		conf_fit_height = 3,
		conf_fit_dblheight = 4,
		conf_fit_custom = 5
	} t_conf_fit;

	typedef enum
	{
		conf_rotate_0 = 0,
		conf_rotate_90 = 1,
		conf_rotate_180 = 2,
		conf_rotate_270 = 3,
	} t_conf_rotate;

	typedef enum
	{
		conf_cycle_single = 0,
		conf_cycle_repeat = 1,
		conf_cycle_repeat_one = 2,
		conf_cycle_random = 3
	} t_conf_cycle;

	typedef enum
	{
		conf_arrange_ext = 0,
		conf_arrange_name = 1,
		conf_arrange_size = 2,
		conf_arrange_ctime = 3,
		conf_arrange_mtime = 4
	} t_conf_arrange;

	typedef enum
	{
		conf_viewpos_leftup = 0,
		conf_viewpos_midup = 1,
		conf_viewpos_rightup = 2,
		conf_viewpos_leftmid = 3,
		conf_viewpos_midmid = 4,
		conf_viewpos_rightmid = 5,
		conf_viewpos_leftdown = 6,
		conf_viewpos_middown = 7,
		conf_viewpos_rightdown = 8,
	} t_conf_viewpos;

	typedef enum
	{
		conf_imgpaging_direct = 0,
		conf_imgpaging_updown = 1,
		conf_imgpaging_leftright = 2,
		conf_imgpaging_updown_smooth = 3,
		conf_imgpaging_leftright_smooth = 4,
	} t_conf_imgpaging;

	typedef enum
	{
		conf_thumb_none = 0,
		conf_thumb_scroll = 1,
		conf_thumb_always = 2
	} t_conf_thumb;

	typedef enum
	{
		conf_infobar_none = 0,
		conf_infobar_info = 1,
		conf_infobar_lyric = 2
	} t_conf_infobar;

	typedef enum
	{
		conf_vertread_horz = 0,
		conf_vertread_rvert = 1,
		conf_vertread_lvert = 2,
		conf_vertread_reversal = 3
	} t_conf_vertread;

	typedef enum
	{
		conf_align_left = 0,
		conf_align_right = 1,
		conf_align_center = 2
	} t_conf_align;

	typedef struct
	{
		char path[PATH_MAX];
		u32 forecolor;
		u32 giftranscolor;
		u32 bgcolor;
		bool have_bg;
		u32 titlecolor;
		u32 menutextcolor;
		u32 menubcolor;
		u32 selicolor;
		u32 selbcolor;
		u32 msgbcolor;
		u32 rowspace;
		t_conf_infobar infobar;
		int infobar_style;
		u32 infobar_fontsize;
		bool rlastrow;
		bool autobm;
		u32 vertread;
		t_conf_encode encode;
		t_conf_fit fit;
		bool imginfobar;
		bool scrollbar;
		u32 scale;
		t_conf_rotate rotate;
		u32 txtkey[20];
		u32 imgkey[20];
		char shortpath[PATH_MAX];
		u32 confver;
		bool bicubic;
		u32 wordspace;
		u32 borderspace;
		char lastfile[PATH_MAX];
		t_conf_encode mp3encode;
		t_conf_encode lyricencode;
		t_conf_cycle mp3cycle;
		bool isreading;
		char bgarch[PATH_MAX];
		char bgfile[PATH_MAX];
		int bgwhere;
		u32 slideinterval;
		bool hprmctrl;
		int grayscale;
		bool showhidden;
		bool showunknown;
		bool showfinfo;
		bool allowdelete;
		t_conf_arrange arrange;
		bool enableusb;
		t_conf_viewpos viewpos;
		u32 imgmvspd;
		t_conf_imgpaging imgpaging;
		u32 imgpaging_spd;
		u32 imgpaging_interval;
		u32 imgpaging_duration;
		u32 flkey[20];
		int fontsize;
		bool reordertxt;
		bool pagetonext;
		int autopage;
		int prev_autopage;
		int autopagetype;
		int autolinedelay;
		bool thumb;
		int bookfontsize;
		bool enable_analog;
		bool img_enable_analog;
		u32 txtkey2[20];
		u32 imgkey2[20];
		u32 flkey2[20];
		u32 imgpagereserve;
		u32 lyricex;
		bool autoplay;
		bool usettf;
		char cttfarch[PATH_MAX];
		char cttfpath[PATH_MAX];
		char ettfarch[PATH_MAX];
		char ettfpath[PATH_MAX];
		int freqs[3];
		int imgbrightness;
		bool dis_scrsave;
		int autosleep;
		int launchtype;
		bool infobar_use_ttf_mode;
		bool img_no_repeat;
		bool hide_flash;
		u32 tabstop;
		bool apetagorder;
		char language[20];
		int filelistwidth;
		bool ttf_load_to_memory;
		bool save_password;
		int scrollbar_width;
		bool hide_last_row;
		bool infobar_show_timer;
		bool englishtruncate;
		bool image_scroll_chgn_speed;
		bool ttf_haste_up;
		bool linenum_style;
	/**
	 * ×´Ì¬À¸¶ÔÆë
	 *
	 * 0 - ×ó
	 * 1 - ÓÒ
	 * 2 - ÖÐ
	 */
		t_conf_align infobar_align;
		char musicdrv_opts[PATH_MAX];
		bool magnetic_scrolling;
		unsigned max_cache_img;
		bool show_encoder_msg;
		int sfx_mode;
		int alc_mode;
		bool use_vaudio;
		int max_brightness;
	} __attribute__ ((packed)) t_conf, *p_conf;

/* txt key:
	0 - bookmark;
	1 - prevpage;
	2 - nextpage;
	3 - prev100;
	4 - next100;
	5 - prev500;
	6 - next500;
	7 - firstpage;
	8 - lastpage;
	9 - prevbook;
	10 - nextbook;
	11 - exitbook;
	above 11 - reversed for future version

   image key:
	0 - imageprev;
	1 - imagenext;
	2 - imagescaletype;
	3 - imagescalesmaller;
	4 - imagescalelarger;
	5 - imagerotateleft;
	6 - imagerotateright;
	7 - imagebar;
	8 - imageinfo;
	9 - imageexit;
	above 9 - reversed for future version
*/

	extern void conf_set_file(const char *filename);
	extern void conf_get_keyname(u32 key, char *res);
	extern bool conf_load(p_conf conf);
	extern bool conf_save(p_conf conf);
	extern const char *conf_get_encodename(t_conf_encode encode);
	extern const char *conf_get_fitname(t_conf_fit fit);
	extern const char *conf_get_cyclename(t_conf_cycle cycle);
	extern const char *conf_get_arrangename(t_conf_arrange arrange);
	extern const char *conf_get_viewposname(t_conf_viewpos viewpos);
	extern const char *conf_get_imgpagingname(t_conf_imgpaging imgpaging);
	extern const char *conf_get_thumbname(t_conf_thumb thumb);
	extern const char *conf_get_infobarname(t_conf_infobar infobar);

	extern t_conf config;

#ifdef __cplusplus
}
#endif

#endif
