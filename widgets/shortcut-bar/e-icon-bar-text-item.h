/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * Author : 
 *  Damon Chaplin <damon@helixcode.com>
 *
 * Copyright 1999, Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

/*
 * Based on gnome-icon-text-item:  an editable text block with word wrapping
 * for the GNOME canvas.
 *
 * Copyright (C) 1998, 1999 The Free Software Foundation
 *
 * Authors: Miguel de Icaza <miguel@gnu.org>
 *          Federico Mena <federico@gimp.org>
 */

/*
 * EIconBarTextItem - An editable canvas text item for the EIconBar.
 */

#ifndef _E_ICON_BAR_TEXT_ITEM_H_
#define _E_ICON_BAR_TEXT_ITEM_H_

#include <gtk/gtkentry.h>
#include <libgnomeui/gnome-canvas.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define E_ICON_BAR_TEXT_ITEM(obj)     (GTK_CHECK_CAST((obj), \
        e_icon_bar_text_item_get_type (), EIconBarTextItem))
#define E_ICON_BAR_TEXT_ITEM_CLASS(k) (GTK_CHECK_CLASS_CAST ((k),\
	e_icon_bar_text_item_get_type ()))
#define E_IS_ICON_BAR_TEXT_ITEM(o)    (GTK_CHECK_TYPE((o), \
	e_icon_bar_text_item_get_type ()))

typedef struct _EIconBarTextItemInfo   EIconBarTextItemInfo;

typedef struct {
	GnomeCanvasItem canvas_item;

	/* Size and maximum allowed width */
	int x, y;
	int width;

	/* Font name */
	char *fontname;

	/* Private data */
	gpointer priv; /* was GtkEntry *entry */

	/* Actual text */
	char *text;

	/* Text layout information */
	EIconBarTextItemInfo *ti;

	/* Whether the text is being edited */
	unsigned int editing : 1;

	/* Whether the text item is selected */
	unsigned int selected : 1;

	/* Whether the user is select-dragging a block of text */
	unsigned int selecting : 1;

	/* Whether the text is editable */
	unsigned int is_editable : 1;

	/* Whether the text is allocated by us (FALSE if allocated by the client) */
	unsigned int is_text_allocated : 1;


	/* The horizontal alignment of the text (default 0.5). */
	gfloat xalign;

	/* The justification of the text (default is centered). */
	GtkJustification justification;

	/* The max number of lines of text shown, or -1 for all (default). */
	gint max_lines;

	/* If '...' is displayed if the text doesn't all fit (default TRUE). */
	gboolean show_ellipsis;

	/* This is TRUE if we couldn't fit all the text in. */
	gboolean is_clipped;
} EIconBarTextItem;

typedef struct {
	GnomeCanvasItemClass parent_class;

	/* Signals we emit */
	int  (* text_changed)      (EIconBarTextItem *iti);
	void (* height_changed)    (EIconBarTextItem *iti);
	void (* width_changed)     (EIconBarTextItem *iti);
	void (* editing_started)   (EIconBarTextItem *iti);
	void (* editing_stopped)   (EIconBarTextItem *iti);
	void (* selection_started) (EIconBarTextItem *iti);
	void (* selection_stopped) (EIconBarTextItem *iti);
} EIconBarTextItemClass;

GtkType  e_icon_bar_text_item_get_type      (void);

void     e_icon_bar_text_item_configure     (EIconBarTextItem  *iti,
					     int                x,
					     int                y,
					     int                width,
					     const char        *fontname,
					     const char        *text,
					     gboolean           is_static);

void     e_icon_bar_text_item_set_width     (EIconBarTextItem  *iti,
					     int		width);

void     e_icon_bar_text_item_setxy         (EIconBarTextItem  *iti,
					     int                x,
					     int                y);

void     e_icon_bar_text_item_select        (EIconBarTextItem  *iti,
					     int                sel);

char*	 e_icon_bar_text_item_get_text      (EIconBarTextItem  *iti);
void	 e_icon_bar_text_item_set_text	    (EIconBarTextItem  *iti,
					     const char	       *text,
					     gboolean		is_static);

void     e_icon_bar_text_item_start_editing (EIconBarTextItem  *iti);
void     e_icon_bar_text_item_stop_editing  (EIconBarTextItem  *iti,
					     gboolean           accept);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_ICON_BAR_TEXT_ITEM_H_ */

