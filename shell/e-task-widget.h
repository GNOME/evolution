/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-task-widget.h
 *
 * Copyright (C) 2001  Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
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
 *
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#ifndef _E_TASK_WIDGET_H_
#define _E_TASK_WIDGET_H_

#include <gtk/gtkeventbox.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_TYPE_TASK_WIDGET			(e_task_widget_get_type ())
#define E_TASK_WIDGET(obj)			(GTK_CHECK_CAST ((obj), E_TYPE_TASK_WIDGET, ETaskWidget))
#define E_TASK_WIDGET_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_TYPE_TASK_WIDGET, ETaskWidgetClass))
#define E_IS_TASK_WIDGET(obj)			(GTK_CHECK_TYPE ((obj), E_TYPE_TASK_WIDGET))
#define E_IS_TASK_WIDGET_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_TASK_WIDGET))


typedef struct _ETaskWidget        ETaskWidget;
typedef struct _ETaskWidgetPrivate ETaskWidgetPrivate;
typedef struct _ETaskWidgetClass   ETaskWidgetClass;

struct _ETaskWidget {
	GtkEventBox parent;

	ETaskWidgetPrivate *priv;
};

struct _ETaskWidgetClass {
	GtkEventBoxClass parent_class;
};


GtkType    e_task_widget_get_type   (void);
void       e_task_widget_construct  (ETaskWidget *task_widget,
				     GdkPixbuf   *icon_pixbuf,
				     const char  *component_id,
				     const char  *information);
GtkWidget *e_task_widget_new        (GdkPixbuf   *icon_pixbuf,
				     const char  *component_id,
				     const char  *information);

void  e_task_widget_update  (ETaskWidget *task_widget,
			     const char  *information,
			     double       completion);

void  e_task_wiget_alert    (ETaskWidget *task_widget);
void  e_task_wiget_unalert  (ETaskWidget *task_widget);

const char *e_task_widget_get_component_id  (ETaskWidget *task_widget);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_TASK_WIDGET_H_ */
