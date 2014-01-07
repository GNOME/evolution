/*
 * Evolution calendar - Main calendar view widget
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
 *		Miguel de Icaza <miguel@ximian.com>
 *      Federico Mena-Quintero <federico@ximian.com>
 *      Seth Alves <alves@hungry.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef GNOME_CALENDAR_H
#define GNOME_CALENDAR_H

#include <time.h>
#include <gtk/gtk.h>
#include <libecal/libecal.h>

#include <e-util/e-util.h>

#include "e-cal-model.h"

/* Standard GObject macros */
#define GNOME_TYPE_CALENDAR \
	(gnome_calendar_get_type ())
#define GNOME_CALENDAR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), GNOME_TYPE_CALENDAR, GnomeCalendar))
#define GNOME_CALENDAR_CLASS(cls) \
	(G_TYPE_CHECK_INSTANCE_CAST_CLASS \
	((cls), GNOME_TYPE_CALENDAR, GnomeCalendarClass))
#define GNOME_IS_CALENDAR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), GNOME_TYPE_CALENDAR))
#define GNOME_IS_CALENDAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), GNOME_TYPE_CALENDAR))
#define GNOME_CALENDAR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), GNOME_TYPE_CALENDAR, GnomeCalendarClass))

G_BEGIN_DECLS

/* Avoid circular inclusion. */
struct _ECalendarView;

typedef struct _GnomeCalendar GnomeCalendar;
typedef struct _GnomeCalendarClass GnomeCalendarClass;
typedef struct _GnomeCalendarPrivate GnomeCalendarPrivate;

/* View types */
typedef enum {
	GNOME_CAL_DAY_VIEW,
	GNOME_CAL_WORK_WEEK_VIEW,
	GNOME_CAL_WEEK_VIEW,
	GNOME_CAL_MONTH_VIEW,
	GNOME_CAL_LIST_VIEW,
	GNOME_CAL_LAST_VIEW
} GnomeCalendarViewType;

typedef enum {
	GNOME_CAL_GOTO_TODAY,
	GNOME_CAL_GOTO_DATE,
	GNOME_CAL_GOTO_FIRST_DAY_OF_MONTH,
	GNOME_CAL_GOTO_LAST_DAY_OF_MONTH,
	GNOME_CAL_GOTO_FIRST_DAY_OF_WEEK,
	GNOME_CAL_GOTO_LAST_DAY_OF_WEEK,
	GNOME_CAL_GOTO_SAME_DAY_OF_PREVIOUS_WEEK,
	GNOME_CAL_GOTO_SAME_DAY_OF_NEXT_WEEK
} GnomeCalendarGotoDateType;

struct _GnomeCalendar {
	GObject parent;
	GnomeCalendarPrivate *priv;
};

struct _GnomeCalendarClass {
	GObjectClass parent_class;

	/* Notification signals */
	void	(*dates_shown_changed)		(GnomeCalendar *gcal);

	void	(*calendar_selection_changed)	(GnomeCalendar *gcal);

	void	(*calendar_focus_change)	(GnomeCalendar *gcal,
						 gboolean in);
	void	(*change_view)			(GnomeCalendar *gcal,
						 GnomeCalendarViewType view_type);

	void	(*source_added)			(GnomeCalendar *gcal,
						 ESource *source);
	void	(*source_removed)		(GnomeCalendar *gcal,
						 ESource *source);

	/* Action signals */
        void	(*goto_date)			(GnomeCalendar *gcal,
						 GnomeCalendarGotoDateType date);
};

GType		gnome_calendar_get_type		(void);
GtkWidget *	gnome_calendar_new		(ESourceRegistry *registry);
ESourceRegistry *
		gnome_calendar_get_registry	(GnomeCalendar *gcal);
ECalendar *	gnome_calendar_get_date_navigator
						(GnomeCalendar *gcal);
void		gnome_calendar_set_date_navigator
						(GnomeCalendar *gcal,
						 ECalendar *date_navigator);
GtkWidget *	gnome_calendar_get_memo_table	(GnomeCalendar *gcal);
void		gnome_calendar_set_memo_table	(GnomeCalendar *gcal,
						 GtkWidget *memo_table);
GtkWidget *	gnome_calendar_get_task_table	(GnomeCalendar *gcal);
void		gnome_calendar_set_task_table	(GnomeCalendar *gcal,
						 GtkWidget *task_table);
ECalModel *	gnome_calendar_get_model	(GnomeCalendar *gcal);
void		gnome_calendar_update_query	(GnomeCalendar *gcal);
void		gnome_calendar_set_search_query	(GnomeCalendar *gcal,
						 const gchar *sexp,
						 gboolean range_search,
						 time_t start_range,
						 time_t end_range);

void		gnome_calendar_next		(GnomeCalendar *gcal);
void		gnome_calendar_previous		(GnomeCalendar *gcal);
void		gnome_calendar_goto		(GnomeCalendar *gcal,
						 time_t new_time);
void		gnome_calendar_update_view_times (GnomeCalendar *gcal,
						 time_t start_time);
void		gnome_calendar_dayjump		(GnomeCalendar *gcal,
						 time_t time);
void		gnome_calendar_goto_today	(GnomeCalendar *gcal);

GnomeCalendarViewType
		gnome_calendar_get_view		(GnomeCalendar *gcal);
void		gnome_calendar_set_view		(GnomeCalendar *gcal,
						 GnomeCalendarViewType view_type);
void		gnome_calendar_display_view	(GnomeCalendar *gcal,
						 GnomeCalendarViewType view_type);

struct _ECalendarView *
		gnome_calendar_get_calendar_view (GnomeCalendar *gcal,
						 GnomeCalendarViewType view_type);

gboolean	gnome_calendar_get_range_selected
						(GnomeCalendar *gcal);
void		gnome_calendar_set_range_selected
						(GnomeCalendar *gcal,
						 gboolean range_selected);
void		gnome_calendar_set_selected_time_range
						(GnomeCalendar *gcal,
						 time_t start_time);
void		gnome_calendar_new_task		(GnomeCalendar *gcal,
						 time_t *dtstart,
						 time_t *dtend);

/* Returns the selected time range for the current view. Note that this may be
 * different from the fields in the GnomeCalendar, since the view may clip
 * this or choose a more appropriate time. */
void		gnome_calendar_get_current_time_range
						(GnomeCalendar *gcal,
						 time_t *start_time,
						 time_t *end_time);

void		gnome_calendar_notify_dates_shown_changed
						(GnomeCalendar *gcal);

/* Returns the number of selected events (0 or 1 at present). */
gint		gnome_calendar_get_num_events_selected
						(GnomeCalendar *gcal);

void		gnome_calendar_purge		(GnomeCalendar  *gcal,
						 time_t older_than);

G_END_DECLS

#endif
