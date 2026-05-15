/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Damon Chaplin <damon@ximian.com>
 */

#ifndef _E_WEEK_VIEW_LAYOUT_H_
#define _E_WEEK_VIEW_LAYOUT_H_

#include "e-week-view.h"

G_BEGIN_DECLS

/* I've split these functions away from EWeekView so we can use them for
 * printing. */

GArray *	e_week_view_layout_events	(GArray *events,
						 GArray *old_spans,
						 gboolean multi_week_view,
						 gint weeks_shown,
						 gboolean compress_weekend,
						 gint start_weekday,
						 time_t *day_starts,
						 gint *rows_per_day);

/* Returns which 'cell' in the table the day appears in. Note that most days
 * have a height of 2 rows, but Sat/Sun are sometimes compressed so they have
 * a height of only 1 row. */
void		e_week_view_layout_get_day_position
						(gint day,
						 gboolean multi_week_view,
						 gint weeks_shown,
						 GDateWeekday display_start_day,
						 gboolean compress_weekend,
						 gint *cell_x,
						 gint *cell_y,
						 gint *rows);

gboolean	e_week_view_layout_get_span_position
						(EWeekViewEvent *event,
						 EWeekViewEventSpan *span,
						 gint rows_per_cell,
						 gint rows_per_compressed_cell,
						 GDateWeekday display_start_day,
						 gboolean multi_week_view,
						 gboolean compress_weekend,
						 gint *span_num_days);

G_END_DECLS

#endif /* _E_WEEK_VIEW_LAYOUT_H_ */
