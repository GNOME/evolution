/*
 *
 * Evolution calendar - Utilities for tagging ECalendar widgets
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
 *      Federico Mena-Quintero <federico@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef TAG_CALENDAR_H
#define TAG_CALENDAR_H

#include <libecal/libecal.h>
#include <e-util/e-util.h>
#include <calendar/gui/e-cal-data-model.h>

/* Standard GObject macros */
#define E_TYPE_TAG_CALENDAR \
	(e_tag_calendar_get_type ())
#define E_TAG_CALENDAR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_TAG_CALENDAR, ETagCalendar))
#define E_TAG_CALENDAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_TAG_CALENDAR, ETagCalendarClass))
#define E_IS_TAG_CALENDAR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_TAG_CALENDAR))
#define E_IS_TAG_CALENDAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_TAG_CALENDAR))
#define E_TAG_CALENDAR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_TAG_CALENDAR, ETagCalendarClass))

G_BEGIN_DECLS

typedef struct _ETagCalendar ETagCalendar;
typedef struct _ETagCalendarClass ETagCalendarClass;
typedef struct _ETagCalendarPrivate ETagCalendarPrivate;

struct _ETagCalendar {
	GObject parent;
	ETagCalendarPrivate *priv;
};

struct _ETagCalendarClass {
	GObjectClass parent_class;
};

GType		e_tag_calendar_get_type		(void);

ETagCalendar *	e_tag_calendar_new		(ECalendar *calendar);
ECalendar *	e_tag_calendar_get_calendar	(ETagCalendar *tag_calendar);
gboolean	e_tag_calendar_get_recur_events_italic
						(ETagCalendar *tag_calendar);
void		e_tag_calendar_set_recur_events_italic
						(ETagCalendar *tag_calendar,
						 gboolean recur_events_italic);
void		e_tag_calendar_subscribe	(ETagCalendar *tag_calendar,
						 ECalDataModel *data_model);
void		e_tag_calendar_unsubscribe	(ETagCalendar *tag_calendar,
						 ECalDataModel *data_model);


void		tag_calendar_by_comp		(ECalendar *ecal,
						 ECalComponent *comp,
						 ECalClient *client,
						 ICalTimezone *display_zone,
						 gboolean clear_first,
						 gboolean comp_is_on_server,
						 gboolean can_recur_events_italic,
						 GCancellable *cancellable);

G_END_DECLS

#endif
