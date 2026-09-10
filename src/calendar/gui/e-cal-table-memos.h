/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_CAL_TABLE_MEMOS_H
#define E_CAL_TABLE_MEMOS_H

#include <shell/e-shell-view.h>
#include <e-util/e-util.h>

#include "e-cal-model.h"
#include "e-cal-table-list-base.h"

G_BEGIN_DECLS

#define E_TYPE_CAL_TABLE_MEMOS (e_cal_table_memos_get_type ())
G_DECLARE_FINAL_TYPE (ECalTableMemos, e_cal_table_memos, E, CAL_TABLE_MEMOS, ECalTableListBase)

GtkWidget *	e_cal_table_memos_new		(EShellView *shell_view,
						 ECalModel *model);
const gchar * const *
		e_cal_table_memos_get_legacy_etable_column_ids
						(guint *out_n_column_ids);

G_END_DECLS

#endif /* E_CAL_TABLE_MEMOS_H */
