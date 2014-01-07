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
 *		Bolian Yin <bolian.yin@sun.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ea-gnome-calendar.h"
#include "e-calendar-view.h"
#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

static void ea_gnome_calendar_class_init (EaGnomeCalendarClass *klass);

static gint ea_gnome_calendar_get_n_children (AtkObject * obj);
static AtkObject * ea_gnome_calendar_ref_child (AtkObject *obj, gint i);

static void ea_gcal_dates_change_cb (GnomeCalendar *gcal, gpointer data);

static gpointer parent_class = NULL;

static const gchar *
ea_gnome_calendar_get_name (AtkObject *accessible)
{
	if (accessible->name)
		return accessible->name;
	return _("Gnome Calendar");
}

static const gchar *
ea_gnome_calendar_get_description (AtkObject *accessible)
{
	if (accessible->description)
		return accessible->description;
	return _("Gnome Calendar");
}

GType
ea_gnome_calendar_get_type (void)
{
	static GType type = 0;
	AtkObjectFactory *factory;
	GTypeQuery query;
	GType derived_atk_type;

	if (!type) {
		static GTypeInfo tinfo = {
			sizeof (EaGnomeCalendarClass),
			(GBaseInitFunc) NULL, /* base init */
			(GBaseFinalizeFunc) NULL, /* base finalize */
			(GClassInitFunc) ea_gnome_calendar_class_init, /* class init */
			(GClassFinalizeFunc) NULL, /* class finalize */
			NULL, /* class data */
			sizeof (EaGnomeCalendar), /* instance size */
			0, /* nb preallocs */
			(GInstanceInitFunc) NULL, /* instance init */
			NULL /* value table */
		};

		/*
		 * Figure out the size of the class and instance
		 * we are run-time deriving from (GailWidget, in this case)
		 */

		factory = atk_registry_get_factory (
			atk_get_default_registry (),
			GTK_TYPE_WIDGET);
		derived_atk_type = atk_object_factory_get_accessible_type (factory);
		g_type_query (derived_atk_type, &query);
		tinfo.class_size = query.class_size;
		tinfo.instance_size = query.instance_size;

		type = g_type_register_static (
			derived_atk_type,
			"EaGnomeCalendar", &tinfo, 0);

	}

	return type;
}

static void
ea_gnome_calendar_class_init (EaGnomeCalendarClass *klass)
{
	AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	class->get_name = ea_gnome_calendar_get_name;
	class->get_description = ea_gnome_calendar_get_description;

	class->get_n_children = ea_gnome_calendar_get_n_children;
	class->ref_child = ea_gnome_calendar_ref_child;
}

AtkObject *
ea_gnome_calendar_new (GtkWidget *widget)
{
	GObject *object;
	AtkObject *accessible;

	g_return_val_if_fail (GNOME_IS_CALENDAR (widget), NULL);

	object = g_object_new (EA_TYPE_GNOME_CALENDAR, NULL);

	accessible = ATK_OBJECT (object);
	atk_object_initialize (accessible, widget);

	accessible->role = ATK_ROLE_FILLER;

	/* listen on view type change
	 */
	g_signal_connect (
		widget, "dates_shown_changed",
		G_CALLBACK (ea_gcal_dates_change_cb), accessible);

#ifdef ACC_DEBUG
	printf ("EvoAcc: ea-gnome-calendar created: %p\n", (gpointer) accessible);
#endif

	return accessible;
}

const gchar *
ea_gnome_calendar_get_label_description (GnomeCalendar *gcal)
{
	GnomeCalendarViewType view_type;
	ECalendarView *calendar_view;
	ECalModel *model;
	icaltimezone *zone;
	struct icaltimetype start_tt, end_tt;
	time_t start_time, end_time;
	struct tm start_tm, end_tm;
	static gchar buffer[512];
	gchar end_buffer[256];
	GnomeCalendarViewType view;

	model = gnome_calendar_get_model (gcal);
	zone = e_cal_model_get_timezone (model);

	view_type = gnome_calendar_get_view (gcal);
	calendar_view = gnome_calendar_get_calendar_view (gcal, view_type);

	if (!e_calendar_view_get_visible_time_range (
		calendar_view, &start_time, &end_time)) {
		g_warn_if_reached ();
		return NULL;
	}

	start_tt = icaltime_from_timet_with_zone (start_time, FALSE, zone);
	start_tm.tm_year = start_tt.year - 1900;
	start_tm.tm_mon = start_tt.month - 1;
	start_tm.tm_mday = start_tt.day;
	start_tm.tm_hour = start_tt.hour;
	start_tm.tm_min = start_tt.minute;
	start_tm.tm_sec = start_tt.second;
	start_tm.tm_isdst = -1;
	start_tm.tm_wday = time_day_of_week (
		start_tt.day, start_tt.month - 1,
		start_tt.year);

	/* Take one off end_time so we don't get an extra day. */
	end_tt = icaltime_from_timet_with_zone (end_time - 1, FALSE, zone);
	end_tm.tm_year = end_tt.year - 1900;
	end_tm.tm_mon = end_tt.month - 1;
	end_tm.tm_mday = end_tt.day;
	end_tm.tm_hour = end_tt.hour;
	end_tm.tm_min = end_tt.minute;
	end_tm.tm_sec = end_tt.second;
	end_tm.tm_isdst = -1;
	end_tm.tm_wday = time_day_of_week (
		end_tt.day, end_tt.month - 1,
		end_tt.year);

	view = gnome_calendar_get_view (gcal);

	switch (view) {
	case GNOME_CAL_DAY_VIEW:
	case GNOME_CAL_WORK_WEEK_VIEW:
	case GNOME_CAL_WEEK_VIEW:
		if (start_tm.tm_year == end_tm.tm_year
		    && start_tm.tm_mon == end_tm.tm_mon
		    && start_tm.tm_mday == end_tm.tm_mday) {
			e_utf8_strftime (
				buffer, sizeof (buffer),
				_("%A %d %b %Y"), &start_tm);
		} else if (start_tm.tm_year == end_tm.tm_year) {
			e_utf8_strftime (
				buffer, sizeof (buffer),
				_("%a %d %b"), &start_tm);
			e_utf8_strftime (
				end_buffer, sizeof (end_buffer),
				_("%a %d %b %Y"), &end_tm);
			strcat (buffer, " - ");
			strcat (buffer, end_buffer);
		} else {
			e_utf8_strftime (
				buffer, sizeof (buffer),
				_("%a %d %b %Y"), &start_tm);
			e_utf8_strftime (
				end_buffer, sizeof (end_buffer),
				_("%a %d %b %Y"), &end_tm);
			strcat (buffer, " - ");
			strcat (buffer, end_buffer);
		}
		break;
	case GNOME_CAL_MONTH_VIEW:
	case GNOME_CAL_LIST_VIEW:
		if (start_tm.tm_year == end_tm.tm_year) {
			if (start_tm.tm_mon == end_tm.tm_mon) {
				if (start_tm.tm_mday == end_tm.tm_mday) {
					buffer[0] = '\0';
				} else {
					e_utf8_strftime (
						buffer, sizeof (buffer),
						"%d", &start_tm);
					strcat (buffer, " - ");
				}
				e_utf8_strftime (
					end_buffer, sizeof (end_buffer),
					_("%d %b %Y"), &end_tm);
				strcat (buffer, end_buffer);
			} else {
				e_utf8_strftime (
					buffer, sizeof (buffer),
					_("%d %b"), &start_tm);
				e_utf8_strftime (
					end_buffer, sizeof (end_buffer),
					_("%d %b %Y"), &end_tm);
				strcat (buffer, " - ");
				strcat (buffer, end_buffer);
			}
		} else {
			e_utf8_strftime (
				buffer, sizeof (buffer),
				_("%d %b %Y"), &start_tm);
			e_utf8_strftime (
				end_buffer, sizeof (end_buffer),
				_("%d %b %Y"), &end_tm);
			strcat (buffer, " - ");
			strcat (buffer, end_buffer);
		}
		break;
	default:
		g_return_val_if_reached (NULL);
	}
	return buffer;
}

static gint
ea_gnome_calendar_get_n_children (AtkObject *obj)
{
	g_return_val_if_fail (EA_IS_GNOME_CALENDAR (obj), 0);

	if (gtk_accessible_get_widget (GTK_ACCESSIBLE (obj)) == NULL)
		return -1;

	return 2;
}

static AtkObject *
ea_gnome_calendar_ref_child (AtkObject *obj,
                             gint i)
{
	AtkObject * child = NULL;
	GnomeCalendar * calendarWidget;
	GnomeCalendarViewType view_type;
	ECalendarView *view;
	ECalendar *date_navigator;
	GtkWidget *childWidget;
	GtkWidget *widget;

	g_return_val_if_fail (EA_IS_GNOME_CALENDAR (obj), NULL);
	/* valid child index range is [0-3] */
	if (i < 0 || i >3)
		return NULL;

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
	if (widget == NULL)
		return NULL;

	calendarWidget = GNOME_CALENDAR (widget);

	switch (i) {
	case 0:
		/* for the day/week view */
		view_type = gnome_calendar_get_view (calendarWidget);
		view = gnome_calendar_get_calendar_view (calendarWidget, view_type);
		childWidget = GTK_WIDGET (view);
		child = gtk_widget_get_accessible (childWidget);
		atk_object_set_parent (child, obj);
		break;
	case 1:
		/* for calendar */
		date_navigator = gnome_calendar_get_date_navigator (calendarWidget);
		childWidget = GTK_WIDGET (date_navigator);
		child = gtk_widget_get_accessible (childWidget);
		break;
	default:
		break;
	}
	if (child)
		g_object_ref (child);
	return child;
}

static void
ea_gcal_dates_change_cb (GnomeCalendar *gcal,
                         gpointer data)
{
	const gchar *new_name;

	g_return_if_fail (GNOME_IS_CALENDAR (gcal));
	g_return_if_fail (data);
	g_return_if_fail (EA_IS_GNOME_CALENDAR (data));

	new_name = ea_gnome_calendar_get_label_description (gcal);
	atk_object_set_name (ATK_OBJECT (data), new_name);
	g_signal_emit_by_name (data, "visible_data_changed");

#ifdef ACC_DEBUG
	printf ("AccDebug: calendar dates changed, label=%s\n", new_name);
#endif
}
