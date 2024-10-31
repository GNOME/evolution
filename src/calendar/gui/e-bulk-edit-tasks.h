/*
 * SPDX-FileCopyrightText: (C) 2023 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_BULK_EDIT_TASKS_H
#define E_BULK_EDIT_TASKS_H

#include <e-util/e-util.h>

/* Standard GObject macros */
#define E_TYPE_BULK_EDIT_TASKS \
	(e_bulk_edit_tasks_get_type ())
#define E_BULK_EDIT_TASKS(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_BULK_EDIT_TASKS, EBulkEditTasks))
#define E_BULK_EDIT_TASKS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_BULK_EDIT_TASKS, EBulkEditTasksClass))
#define E_IS_BULK_EDIT_TASKS(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_BULK_EDIT_TASKS))
#define E_IS_BULK_EDIT_TASKS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_BULK_EDIT_TASKS))
#define E_BULK_EDIT_TASKS_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_BULK_EDIT_TASKS, EBulkEditTasksClass))

G_BEGIN_DECLS

typedef struct _EBulkEditTasks EBulkEditTasks;
typedef struct _EBulkEditTasksClass EBulkEditTasksClass;
typedef struct _EBulkEditTasksPrivate EBulkEditTasksPrivate;

struct _EBulkEditTasks {
	GtkDialog parent;
	EBulkEditTasksPrivate *priv;
};

struct _EBulkEditTasksClass {
	GtkDialogClass parent_class;
};

GType		e_bulk_edit_tasks_get_type	(void) G_GNUC_CONST;
GtkWidget *	e_bulk_edit_tasks_new		(GtkWindow *parent,
						 GSList *components); /* ECalModelComponent * */

G_END_DECLS

#endif /* E_BULK_EDIT_TASKS_H */
