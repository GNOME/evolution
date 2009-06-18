/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Ettore Perazzoli <ettore@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_TASK_WIDGET_H_
#define _E_TASK_WIDGET_H_

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define E_TYPE_TASK_WIDGET			(e_task_widget_get_type ())
#define E_TASK_WIDGET(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_TASK_WIDGET, ETaskWidget))
#define E_TASK_WIDGET_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_TASK_WIDGET, ETaskWidgetClass))
#define E_IS_TASK_WIDGET(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_TASK_WIDGET))
#define E_IS_TASK_WIDGET_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_TASK_WIDGET))


typedef struct _ETaskWidget        ETaskWidget;
typedef struct _ETaskWidgetPrivate ETaskWidgetPrivate;
typedef struct _ETaskWidgetClass   ETaskWidgetClass;

struct _ETaskWidget {
	GtkEventBox parent;

	ETaskWidgetPrivate *priv;
	guint id;
};

struct _ETaskWidgetClass {
	GtkEventBoxClass parent_class;
};


GType		e_task_widget_get_type		(void);
void		e_task_widget_construct		(ETaskWidget *task_widget,
						 const gchar *component_id,
						 const gchar *information,
						 void (*cancel_func) (gpointer data),
						 gpointer data);
GtkWidget *	e_task_widget_new		(const gchar *component_id,
						 const gchar *information);
GtkWidget *	e_task_widget_new_with_cancel	(const gchar *component_id,
						 const gchar *information,
						 void (*cancel_func) (gpointer data),
						 gpointer data);
void		e_task_widget_update		(ETaskWidget *task_widget,
						 const gchar *information,
						 double completion);
GtkWidget *	e_task_widget_update_image	(ETaskWidget *task_widget,
						 const gchar *stock,
						 const gchar *text);
void		e_task_wiget_alert		(ETaskWidget *task_widget);
void		e_task_wiget_unalert		(ETaskWidget *task_widget);
const gchar *	e_task_widget_get_component_id	(ETaskWidget *task_widget);

G_END_DECLS

#endif /* _E_TASK_WIDGET_H_ */
