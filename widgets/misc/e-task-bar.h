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

#ifndef _E_TASK_BAR_H_
#define _E_TASK_BAR_H_

#include "e-task-widget.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define E_TYPE_TASK_BAR			(e_task_bar_get_type ())
#define E_TASK_BAR(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_TASK_BAR, ETaskBar))
#define E_TASK_BAR_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_TASK_BAR, ETaskBarClass))
#define E_IS_TASK_BAR(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_TASK_BAR))
#define E_IS_TASK_BAR_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_TASK_BAR))


typedef struct _ETaskBar        ETaskBar;
typedef struct _ETaskBarPrivate ETaskBarPrivate;
typedef struct _ETaskBarClass   ETaskBarClass;

struct _ETaskBar {
	GtkHBox          parent;

	ETaskBarPrivate *priv;
};

struct _ETaskBarClass {
	GtkHBoxClass parent_class;
};


GType        e_task_bar_get_type         (void);
void         e_task_bar_construct        (ETaskBar    *task_bar);
GtkWidget   *e_task_bar_new              (void);

void         e_task_bar_set_message      (ETaskBar    *task_bar,
					  const gchar  *message);
void         e_task_bar_unset_message    (ETaskBar    *task_bar);

void         e_task_bar_prepend_task     (ETaskBar    *task_bar,
					  ETaskWidget *task_widget);
void         e_task_bar_remove_task      (ETaskBar    *task_bar,
					  gint          n);
ETaskWidget * e_task_bar_get_task_widget_from_id (ETaskBar *task_bar,
						  guint id);

void	    e_task_bar_remove_task_from_id (ETaskBar *task_bar,
					    guint id);
ETaskWidget *e_task_bar_get_task_widget  (ETaskBar    *task_bar,
					  gint          n);
gint	     e_task_bar_get_num_children (ETaskBar *task_bar);
G_END_DECLS

#endif /* _E_TASK_BAR_H_ */
