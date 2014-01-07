/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __E_MINICARD_VIEW_WIDGET_H__
#define __E_MINICARD_VIEW_WIDGET_H__

#include <libebook/libebook.h>

#include <e-util/e-util.h>

#include "e-minicard-view.h"

G_BEGIN_DECLS

#define E_TYPE_MINICARD_VIEW_WIDGET		(e_minicard_view_widget_get_type ())
#define E_MINICARD_VIEW_WIDGET(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_MINICARD_VIEW_WIDGET, EMinicardViewWidget))
#define E_MINICARD_VIEW_WIDGET_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_MINICARD_VIEW_WIDGET, EMinicardViewWidgetClass))
#define E_IS_MINICARD_VIEW_WIDGET(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_MINICARD_VIEW_WIDGET))
#define E_IS_MINICARD_VIEW_WIDGET_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_MINICARD_VIEW_WIDGET))

typedef struct _EMinicardViewWidget       EMinicardViewWidget;
typedef struct _EMinicardViewWidgetClass  EMinicardViewWidgetClass;

struct _EMinicardViewWidget
{
	ECanvas parent;

	GnomeCanvasItem *background;
	GnomeCanvasItem *emv;

	EAddressbookReflowAdapter *adapter;

	EBookClient *book_client;
	gchar *query;
	guint editable : 1;

	gdouble column_width;
};

struct _EMinicardViewWidgetClass
{
	ECanvasClass parent_class;
	void         (*create_contact)       (EMinicardViewWidget *emvw);
	void         (*create_contact_list)  (EMinicardViewWidget *emvw);
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
