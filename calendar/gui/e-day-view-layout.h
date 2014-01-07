/*
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Damon Chaplin <damon@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_DAY_VIEW_LAYOUT_H_
#define _E_DAY_VIEW_LAYOUT_H_

#include "e-day-view.h"

G_BEGIN_DECLS

/* I've split these functions away from EDayView so we can use them for
 * printing. */

void e_day_view_layout_long_events	(GArray	   *events,
					 gint	    days_shown,
					 time_t	   *day_starts,
					 gint	   *rows_in_top_display);

gint e_day_view_layout_day_events	(GArray	   *events,
					 gint	    rows,
					 gint	    mins_per_row,
					 guint8	   *cols_per_row,
					 gint       max_cols);

gboolean   e_day_view_find_long_event_days	(EDayViewEvent	*event,
						 gint		 days_shown,
						 time_t		*day_starts,
						 gint		*start_day,
						 gint		*end_day);

G_END_DECLS

#endif /* _E_DAY_VIEW_LAYOUT_H_ */
