/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-minicard-view-widget.h
 * Copyright (C) 2000  Helix Code, Inc.
 * Author: Chris Lahey <clahey@helixcode.com>
 *
 * This library is free software; you can redistribute it and/or
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __E_MINICARD_VIEW_WIDGET_H__
#define __E_MINICARD_VIEW_WIDGET_H__

#include <gnome.h>
#include "e-minicard-view.h"
#include "e-util/e-canvas.h"
#include <ebook/e-book.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_MINICARD_VIEW_WIDGET_TYPE			(e_minicard_view_widget_get_type ())
#define E_MINICARD_VIEW_WIDGET(obj)			(GTK_CHECK_CAST ((obj), E_MINICARD_VIEW_WIDGET_TYPE, EMinicardViewWidget))
#define E_MINICARD_VIEW_WIDGET_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_MINICARD_VIEW_WIDGET_TYPE, EMinicardViewWidgetClass))
#define E_IS_MINICARD_VIEW_WIDGET(obj) 		(GTK_CHECK_TYPE ((obj), E_MINICARD_VIEW_WIDGET_TYPE))
#define E_IS_MINICARD_VIEW_WIDGET_CLASS(klass) 	(GTK_CHECK_CLASS_TYPE ((obj), E_MINICARD_VIEW_WIDGET_TYPE))


typedef struct _EMinicardViewWidget       EMinicardViewWidget;
typedef struct _EMinicardViewWidgetClass  EMinicardViewWidgetClass;

struct _EMinicardViewWidget
{
	ECanvas parent;

	GnomeCanvasItem *rect;
	GnomeCanvasItem *emv;

	EBook *book;
	char *query;
};

struct _EMinicardViewWidgetClass
{
	ECanvasClass parent_class;
};

GtkType    e_minicard_view_widget_get_type (void);
void       e_minicard_view_widget_remove_selection (EMinicardViewWidget *view,
						    EBookCallback  cb,
						    gpointer       closure);
void       e_minicard_view_widget_jump_to_letter   (EMinicardViewWidget *view,
						    char           letter);

GtkWidget *e_minicard_view_widget_new (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __E_MINICARD_VIEW_WIDGET_H__ */
