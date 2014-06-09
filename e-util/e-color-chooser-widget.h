/* e-color-chooser-widget.h
 *
 * Copyright (C) 2012 Dan Vrátil <dvratil@redhat.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_COLOR_CHOOSER_WIDGET_H
#define E_COLOR_CHOOSER_WIDGET_H

#include <gtk/gtk.h>

/* Standard GObject macros */
#define E_TYPE_COLOR_CHOOSER_WIDGET \
	(e_color_chooser_widget_get_type ())
#define E_COLOR_CHOOSER_WIDGET(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_COLOR_CHOOSER_WIDGET, EColorChooserWidget))
#define E_COLOR_CHOOSER_WIDGET_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_COLOR_CHOOSER_WIDGET, EColorChooserWidgetClass))
#define E_IS_COLOR_CHOOSER_WIDGET(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_COLOR_CHOOSER_WIDGET))
#define E_IS_COLOR_CHOOSER_WIDGET_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_COLOR_CHOOSER_WIDGET))
#define E_COLOR_CHOOSER_WIDGET_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_COLOR_CHOOSER_WIDGET, EColorChooserWidgetClass))

G_BEGIN_DECLS

typedef struct _EColorChooserWidget EColorChooserWidget;
typedef struct _EColorChooserWidgetClass EColorChooserWidgetClass;
typedef struct _EColorChooserWidgetPrivate EColorChooserWidgetPrivate;

struct _EColorChooserWidget {
	GtkColorChooserWidget parent;
	EColorChooserWidgetPrivate *priv;
};

struct _EColorChooserWidgetClass {
	GtkColorChooserWidgetClass parent_class;

	void		(*editor_activated)	(GtkColorChooserWidget *chooser);
};

GType		e_color_chooser_widget_get_type	(void) G_GNUC_CONST;
GtkWidget *	e_color_chooser_widget_new	(void);

G_END_DECLS

#endif /* E_COLOR_CHOOSER_WIDGET_H */

