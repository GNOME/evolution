/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * Author : 
 *  Damon Chaplin <damon@ximian.com>
 *
 * Copyright 2001, Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of version 2 of the GNU General Public 
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */
#ifndef _E_WEEK_VIEW_LAYOUT_H_
#define _E_WEEK_VIEW_LAYOUT_H_

#include "e-week-view.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* I've split these functions away from EWeekView so we can use them for
   printing. */

GArray* e_week_view_layout_events	(GArray		*events,
					 GArray		*old_spans,
					 gboolean	 multi_week_view,
					 gint		 weeks_shown,
					 gboolean	 compress_weekend,
					 gint		 start_weekday,
					 time_t		*day_starts,
					 gint		*rows_per_day);

/* Returns which 'cell' in the table the day appears in. Note that most days
   have a height of 2 rows, but Sat/Sun are sometimes compressed so they have
   a height of only 1 row. */
void e_week_view_layout_get_day_position(gint		 day,
					 gboolean	 multi_week_view,
					 gint		 weeks_shown,
					 gint		 display_start_day,
					 gboolean	 compress_weekend,
					 gint		*cell_x,
					 gint		*cell_y,
					 gint		*rows);

gboolean e_week_view_layout_get_span_position (EWeekViewEvent *event,
					       EWeekViewEventSpan *span,
					       gint	 rows_per_cell,
					       gint	 rows_per_compressed_cell,
					       gint	 display_start_day,
					       gboolean	 multi_week_view,
					       gboolean	 compress_weekend,
					       gint	*span_num_days);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_WEEK_VIEW_LAYOUT_H_ */
