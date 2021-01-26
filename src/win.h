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

#ifndef _WIN_H_
#define _WIN_H_

#include "common/datatype.h"
#include "display.h"
#include "buffer.h"

// Menu item structure
typedef struct
{
	buffer *compname;
	buffer *shortname;
	char name[128];				// item name
	u32 width;					// display width in bytes (for align/span)
	pixel icolor;				// font color
	pixel selicolor;			// selected font color
	pixel selrcolor;			// selected rectangle line color
	pixel selbcolor;			// selected background filling color
	bool selected;				// item selected
	void *data;					// custom data for user processing
	u16 data2[4];
	u32 data3;
} t_win_menuitem, *p_win_menuitem;

enum
{
	MENU_REALLOC_INCR = 64,
};

typedef struct _t_win_menu
{
	p_win_menuitem root;
	u32 size;
	u32 cap;
} t_win_menu, *p_win_menu;

typedef struct _win_menu_predraw_data
{
	int max_item_len;
	int item_count;
	int x, y;
	int left, upper;
	int linespace;
} win_menu_predraw_data;

// Menu callback
typedef enum
{
	win_menu_op_continue = 0,
	win_menu_op_redraw,
	win_menu_op_ok,
	win_menu_op_cancel,
	win_menu_op_force_redraw
} t_win_menu_op;
typedef t_win_menu_op(*t_win_menu_callback) (u32 key, p_win_menuitem item, u32 * count, u32 page_count, u32 * topindex, u32 * index);
typedef void (*t_win_menu_draw) (p_win_menuitem item, u32 index, u32 topindex, u32 max_height);

// Default callback for menu
extern t_win_menu_op win_menu_defcb(u32 key, p_win_menuitem item, u32 * count, u32 page_count, u32 * topindex, u32 * index);

// Menu show & wait for input with callback supported
extern u32 win_menu(u32 x, u32 y, u32 max_width, u32 max_height,
					p_win_menuitem item, u32 count, u32 initindex,
					u32 linespace, pixel bgcolor, bool redraw, t_win_menu_draw predraw, t_win_menu_draw postdraw, t_win_menu_callback cb);

// Messagebox with yes/no
extern bool win_msgbox(const char *prompt, const char *yesstr, const char *nostr, pixel fontcolor, pixel bordercolor, pixel bgcolor);

// Message with any key pressed to close
extern void win_msg(const char *prompt, pixel fontcolor, pixel bordercolor, pixel bgcolor);

extern void win_item_destroy(p_win_menuitem * item, u32 * size);
extern p_win_menuitem win_realloc_items(p_win_menuitem item, int orgsize, int newsize);
extern p_win_menuitem win_copy_item(p_win_menuitem dst, const p_win_menuitem src);

extern int win_get_max_length(const p_win_menuitem pItem, int size);
extern int win_get_max_pixel_width(const p_win_menuitem pItem, int size);

p_win_menu win_menu_new(void);
int win_menu_add(p_win_menu menu, p_win_menuitem item);
int win_menu_add_copy(p_win_menu menu, p_win_menuitem item);
void win_menuitem_destory(p_win_menuitem menu);
void win_menu_destroy(p_win_menu menu);
void win_menuitem_new(p_win_menuitem p);
void win_menuitem_free(p_win_menuitem p);

#endif
