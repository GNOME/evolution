/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-summary-title-button.h
 *
 * Authors: Iain Holmes <iain@helixcode.com>
 *
 * Copyright (C) 2000  Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _E_SUMMARY_TITLE_BUTTON_H__
#define _E_SUMMARY_TITLE_BUTTON_H__

#include <gtk/gtksignal.h>
#include <libgnomeui/gnome-canvas.h>

#define E_SUMMARY_TITLE_BUTTON_TYPE (e_summary_title_button_get_type ())
#define E_SUMMARY_TITLE_BUTTON(obj) (GTK_CHECK_CAST ((obj), E_SUMMARY_TITLE_BUTTON_TYPE, ESummaryTitleButton))
#define E_SUMMARY_TITLE_BUTTON_CLASS(klass) (GTK_CHECK_CLASS_CAST ((klass), E_SUMMARY_TITLE_BUTTON_TYPE, ESummaryTitleButtonClass))
#define IS_E_SUMMARY_TITLE_BUTTON(obj) (GTK_CHECK_TYPE ((obj), E_SUMMARY_TITLE_BUTTON_TYPE))
#define IS_E_SUMMARY_TITLE_BUTTON_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((obj), E_SUMMARY_TITLE_BUTTON_TYPE))

typedef struct _ESummaryTitleButtonPrivate ESummaryTitleButtonPrivate;
typedef struct _ESummaryTitleButton ESummaryTitleButton;
typedef struct _ESummaryTitleButtonClass ESummaryTitleButtonClass;

struct _ESummaryTitleButton {
	GnomeCanvasRect parent;
	
	ESummaryTitleButtonPrivate *private;
};

struct _ESummaryTitleButtonClass {
	GnomeCanvasRectClass parent_class;
	
	void (*clicked) (ESummaryTitleButton *estb);
};

GtkType e_summary_title_button_get_type (void);

#endif
