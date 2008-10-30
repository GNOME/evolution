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
 *		Federico Mena Quintero <federico@ximian.com>
 *	    Damon Chaplin <damon@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_TASKS_H_
#define _E_TASKS_H_

#include <bonobo/bonobo-ui-component.h>
#include <gtk/gtk.h>
#include <libedataserver/e-source.h>
#include <libecal/e-cal.h>
#include "e-calendar-table.h"

#define E_TYPE_TASKS            (e_tasks_get_type ())
#define E_TASKS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_TASKS, ETasks))
#define E_TASKS_CLASS(klass)    (G_TYPE_CHECK_INSTANCE_CAST_CLASS ((klass), E_TYPE_TASKS, \
				 ETasksClass))
#define E_IS_TASKS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_TASKS))
#define E_IS_TASKS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), E_TYPE_TASKS))

typedef struct _ETasks ETasks;
typedef struct _ETasksClass ETasksClass;
typedef struct _ETasksPrivate ETasksPrivate;

struct _ETasks {
	GtkTable table;

	/* Private data */
	ETasksPrivate *priv;
};

struct _ETasksClass {
	GtkTableClass parent_class;

	/* Notification signals */
	void (* selection_changed) (ETasks *tasks, int n_selected);
        void (* source_added)      (ETasks *tasks, ESource *source);
        void (* source_removed)    (ETasks *tasks, ESource *source);
};


GType      e_tasks_get_type        (void);
GtkWidget *e_tasks_construct       (ETasks *tasks);

GtkWidget *e_tasks_new             (void);

gboolean   e_tasks_add_todo_source (ETasks *tasks, ESource *source);
gboolean   e_tasks_remove_todo_source (ETasks *tasks, ESource *source);
gboolean   e_tasks_set_default_source (ETasks *tasks, ESource *source);
void       e_tasks_open_task         (ETasks		*tasks);
void       e_tasks_open_task_id      (ETasks		*tasks,
				      const char *src_uid,
				      const char *comp_uid,
				      const char *comp_rid);
void       e_tasks_new_task          (ETasks            *tasks);
void       e_tasks_delete_completed  (ETasks            *tasks);

#endif /* _E_TASKS_H_ */
