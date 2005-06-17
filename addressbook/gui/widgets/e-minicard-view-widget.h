/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-minicard-view-widget.h
 * Copyright (C) 2000  Ximian, Inc.
 * Author: Chris Lahey <clahey@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
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

#include <widgets/misc/e-canvas.h>
#include <libebook/e-book.h>
#include "e-minicard-view.h"

G_BEGIN_DECLS

#define E_TYPE_MINICARD_VIEW_WIDGET		(e_minicard_view_widget_get_type ())
#define E_MINICARD_VIEW_WIDGET(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_MINICARD_VIEW_WIDGET, EMinicardViewWidget))
#define E_MINICARD_VIEW_WIDGET_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_MINICARD_VIEW_WIDGET, EMinicardViewWidgetClass))
#define E_IS_MINICARD_VIEW_WIDGET(obj) 		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_MINICARD_VIEW_WIDGET))
#define E_IS_MINICARD_VIEW_WIDGET_CLASS(klass) 	(G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_MINICARD_VIEW_WIDGET))


typedef struct _EMinicardViewWidget       EMinicardViewWidget;
typedef struct _EMinicardViewWidgetClass  EMinicardViewWidgetClass;

struct _EMinicardViewWidget
{
	ECanvas parent;

	GnomeCanvasItem *background;
	GnomeCanvasItem *emv;

	EAddressbookReflowAdapter *adapter;

	EBook *book;
	char *query;
	guint editable : 1;

	double column_width;
};

struct _EMinicardViewWidgetClass
{
	ECanvasClass parent_class;
	void         (*selection_change)     (EMinicardViewWidget *emvw);
	void         (*column_width_changed) (EMinicardViewWidget *emvw, double width);
	guint        (*right_click)          (EMinicardViewWidget *emvw);
};


GType            e_minicard_view_widget_get_type             (void);
GtkWidget       *e_minicard_view_widget_new                  (EAddressbookReflowAdapter *adapter);

/* Get parts of the view widget. */
ESelectionModel *e_minicard_view_widget_get_selection_model  (EMinicardViewWidget       *view);
EMinicardView   *e_minicard_view_widget_get_view             (EMinicardViewWidget       *view);

G_END_DECLS

#endif /* __E_MINICARD_VIEW_WIDGET_H__ */
