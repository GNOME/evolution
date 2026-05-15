/*
 * SPDX-FileCopyrightText: (C) 2012 Dan Vrátil <dvratil@redhat.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
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

