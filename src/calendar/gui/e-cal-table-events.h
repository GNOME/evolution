/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_CAL_TABLE_EVENTS_H
#define E_CAL_TABLE_EVENTS_H

#include <e-util/e-util.h>

#include "e-calendar-view.h"

G_BEGIN_DECLS

#define E_TYPE_CAL_TABLE_EVENTS (e_cal_table_events_get_type ())
G_DECLARE_FINAL_TYPE (ECalTableEvents, e_cal_table_events, E, CAL_TABLE_EVENTS, ECalendarView)

ECalendarView *	e_cal_table_events_new		(ECalModel *model);
EVirtualTree *	e_cal_table_events_get_virtual_tree
						(ECalTableEvents *self);
EPrintable *	e_cal_table_events_get_printable
						(ECalTableEvents *self);
void		e_cal_table_events_set_search_active
						(ECalTableEvents *self,
						 gboolean search_active);
gboolean	e_cal_table_events_get_search_active
						(ECalTableEvents *self);
const gchar * const *
		e_cal_table_events_get_legacy_etable_column_ids
						(guint *out_n_column_ids);

G_END_DECLS

#endif /* E_CAL_TABLE_EVENTS_H */
