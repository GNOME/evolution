/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Damon Chaplin <damon@ximian.com>
 */

#ifndef E_TASK_TABLE_H
#define E_TASK_TABLE_H

#include <shell/e-shell-view.h>

#include "e-cal-model.h"

/*
 * ETaskTable - displays the iCalendar objects in a table (an ETable).
 *
 * XXX ETaskTable and EMemoTable have lots of duplicate code.  We should
 *     look at merging them, or at least bringing back ECalendarTable as
 *     a common base class.
 */

/* Standard GObject macros */
#define E_TYPE_TASK_TABLE \
	(e_task_table_get_type ())
#define E_TASK_TABLE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_TASK_TABLE, ETaskTable))
#define E_TASK_TABLE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_TASK_TABLE, ETaskTableClass))
#define E_IS_TASK_TABLE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_TASK_TABLE))
#define E_IS_TASK_TABLE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_TASK_TABLE))
#define E_TASK_TABLE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_TASK_TABLE, ETaskTableClass))

G_BEGIN_DECLS

typedef struct _ETaskTable ETaskTable;
typedef struct _ETaskTableClass ETaskTableClass;
typedef struct _ETaskTablePrivate ETaskTablePrivate;

struct _ETaskTable {
	ETable parent;

	ETaskTablePrivate *priv;
};

struct _ETaskTableClass {
	ETableClass parent_class;

	/* Signals */
	void	(*open_component)		(ETaskTable *task_table,
						 ECalModelComponent *comp_data);
	void	(*popup_event)			(ETaskTable *task_table,
						 GdkEvent *event);
};

GType		e_task_table_get_type		(void);
GtkWidget *	e_task_table_new		(EShellView *shell_view,
						 ECalModel *model);
ECalModel *	e_task_table_get_model		(ETaskTable *task_table);
EShellView *	e_task_table_get_shell_view	(ETaskTable *task_table);
GSList *	e_task_table_get_selected	(ETaskTable *task_table);
GtkTargetList *	e_task_table_get_copy_target_list
						(ETaskTable *task_table);
GtkTargetList *	e_task_table_get_paste_target_list
						(ETaskTable *task_table);
ECalModelComponent *
		e_task_table_get_selected_comp
						(ETaskTable *task_table);
void		e_task_table_hide_completed_tasks
						(ETaskTable *table,
						 GList *clients_list,
						 gboolean config_changed);
void		e_task_table_process_completed_tasks
						(ETaskTable *table,
						 gboolean config_changed);

G_END_DECLS

#endif /* E_TASK_TABLE_H */
