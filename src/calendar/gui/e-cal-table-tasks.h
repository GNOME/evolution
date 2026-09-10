/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_CAL_TABLE_TASKS_H
#define E_CAL_TABLE_TASKS_H

#include <shell/e-shell-view.h>
#include <e-util/e-util.h>

#include "e-cal-model.h"
#include "e-cal-table-list-base.h"

G_BEGIN_DECLS

#define E_TYPE_CAL_TABLE_TASKS (e_cal_table_tasks_get_type ())
G_DECLARE_FINAL_TYPE (ECalTableTasks, e_cal_table_tasks, E, CAL_TABLE_TASKS, ECalTableListBase)

GtkWidget *	e_cal_table_tasks_new		(EShellView *shell_view,
						 ECalModel *model);
const gchar * const *
		e_cal_table_tasks_get_legacy_etable_column_ids
						(guint *out_n_column_ids);
void		e_cal_table_tasks_process_completed_tasks
						(ECalTableTasks *self,
						 gboolean config_changed);

G_END_DECLS

#endif /* E_CAL_TABLE_TASKS_H */
