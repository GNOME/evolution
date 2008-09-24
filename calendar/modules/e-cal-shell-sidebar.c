/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-cal-shell-sidebar.c
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "e-cal-shell-sidebar.h"

#include <string.h>
#include <glib/gi18n.h>
#include <libecal/e-cal-time-util.h>
#include <libedataserver/e-data-server-util.h>
#include <libedataserverui/e-source-selector.h>

#include <e-util/e-util.h>

#include <gnome-cal.h>
#include <e-cal-shell-view.h>
#include <e-calendar-selector.h>

#define E_CAL_SHELL_SIDEBAR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_CAL_SHELL_SIDEBAR, ECalShellSidebarPrivate))

struct _ECalShellSidebarPrivate {
	GtkWidget *selector;
};

enum {
	PROP_0,
	PROP_SELECTOR
};

static gpointer parent_class;

static void
cal_shell_sidebar_update (EShellSidebar *shell_sidebar)
{
	EShellView *shell_view;
	GnomeCalendar *calendar;
	GnomeCalendarViewType view;
	time_t start_time, end_time;
	struct tm start_tm, end_tm;
	struct icaltimetype start_tt, end_tt;
	icaltimezone *timezone;
	gchar buffer[512];
	gchar end_buffer[512];

	shell_view = e_shell_sidebar_get_shell_view (shell_sidebar);
	calendar = e_cal_shell_view_get_calendar (
		E_CAL_SHELL_VIEW (shell_view));

	gnome_calendar_get_visible_time_range (
		calendar, &start_time, &end_time);
	timezone = gnome_calendar_get_timezone (calendar);
	view = gnome_calendar_get_view (calendar);

	start_tt = icaltime_from_timet_with_zone (start_time, FALSE, timezone);
	start_tm.tm_year = start_tt.year - 1900;
	start_tm.tm_mon = start_tt.month - 1;
	start_tm.tm_mday = start_tt.day;
	start_tm.tm_hour = start_tt.hour;
	start_tm.tm_min = start_tt.minute;
	start_tm.tm_sec = start_tt.second;
	start_tm.tm_isdst = -1;
	start_tm.tm_wday = time_day_of_week (
		start_tt.day, start_tt.month - 1, start_tt.year);

	/* Subtract one from end_time so we don't get an extra day. */
	end_tt = icaltime_from_timet_with_zone (end_time - 1, FALSE, timezone);
	end_tm.tm_year = end_tt.year - 1900;
	end_tm.tm_mon = end_tt.month - 1;
	end_tm.tm_mday = end_tt.day;
	end_tm.tm_hour = end_tt.hour;
	end_tm.tm_min = end_tt.minute;
	end_tm.tm_sec = end_tt.second;
	end_tm.tm_isdst = -1;
	end_tm.tm_wday = time_day_of_week (
		end_tt.day, end_tt.month - 1, end_tt.year);

	switch (view) {
		case GNOME_CAL_DAY_VIEW:
		case GNOME_CAL_WORK_WEEK_VIEW:
		case GNOME_CAL_WEEK_VIEW:
			if (start_tm.tm_year == end_tm.tm_year &&
				start_tm.tm_mon == end_tm.tm_mon &&
				start_tm.tm_mday == end_tm.tm_mday) {
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
					e_utf8_strftime (
						buffer,
						sizeof (buffer),
						"%d", &start_tm);
					e_utf8_strftime (
						end_buffer,
						sizeof (end_buffer),
						_("%d %b %Y"), &end_tm);
					strcat (buffer, " - ");
					strcat (buffer, end_buffer);
				} else {
					e_utf8_strftime (
						buffer,
						sizeof (buffer),
						_("%d %b"), &start_tm);
					e_utf8_strftime (
						end_buffer,
						sizeof (end_buffer),
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
			g_return_if_reached ();
	}

	e_shell_sidebar_set_secondary_text (shell_sidebar, buffer);
}

static void
cal_shell_sidebar_get_property (GObject *object,
                                 guint property_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SELECTOR:
			g_value_set_object (
				value, e_cal_shell_sidebar_get_selector (
				E_CAL_SHELL_SIDEBAR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
cal_shell_sidebar_dispose (GObject *object)
{
	ECalShellSidebarPrivate *priv;

	priv = E_CAL_SHELL_SIDEBAR_GET_PRIVATE (object);

	if (priv->selector != NULL) {
		g_object_unref (priv->selector);
		priv->selector = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
cal_shell_sidebar_constructed (GObject *object)
{
	ECalShellSidebarPrivate *priv;
	EShellView *shell_view;
	EShellSidebar *shell_sidebar;
	ECalShellView *cal_shell_view;
	ESourceList *source_list;
	GtkContainer *container;
	GtkWidget *widget;
	GnomeCalendar *calendar;

	priv = E_CAL_SHELL_SIDEBAR_GET_PRIVATE (object);

	shell_sidebar = E_SHELL_SIDEBAR (object);
	shell_view = e_shell_sidebar_get_shell_view (shell_sidebar);
	cal_shell_view = E_CAL_SHELL_VIEW (shell_view);
	source_list = e_cal_shell_view_get_source_list (cal_shell_view);

	container = GTK_CONTAINER (shell_sidebar);

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_container_add (container, widget);
	gtk_widget_show (widget);

	container = GTK_CONTAINER (widget);

	widget = e_calendar_selector_new (source_list);
	e_source_selector_set_select_new (E_SOURCE_SELECTOR (widget), TRUE);
	gtk_container_add (container, widget);
	priv->selector = g_object_ref (widget);
	gtk_widget_show (widget);

	/* Setup signal handlers. */

	calendar = e_cal_shell_view_get_calendar (cal_shell_view);

	g_signal_connect_swapped (
		calendar, "dates-shown-changed",
		G_CALLBACK (cal_shell_sidebar_update),
		shell_sidebar);

	cal_shell_sidebar_update (shell_sidebar);
}

static void
cal_shell_sidebar_class_init (ECalShellSidebarClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (ECalShellSidebarPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = cal_shell_sidebar_get_property;
	object_class->dispose = cal_shell_sidebar_dispose;
	object_class->constructed = cal_shell_sidebar_constructed;

	g_object_class_install_property (
		object_class,
		PROP_SELECTOR,
		g_param_spec_object (
			"selector",
			_("Source Selector Widget"),
			_("This widget displays groups of calendars"),
			E_TYPE_SOURCE_SELECTOR,
			G_PARAM_READABLE));
}

static void
cal_shell_sidebar_init (ECalShellSidebar *cal_shell_sidebar)
{
	cal_shell_sidebar->priv =
		E_CAL_SHELL_SIDEBAR_GET_PRIVATE (cal_shell_sidebar);

	/* Postpone widget construction until we have a shell view. */
}

GType
e_cal_shell_sidebar_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (ECalShellSidebarClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) cal_shell_sidebar_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (ECalShellSidebar),
			0,     /* n_preallocs */
			(GInstanceInitFunc) cal_shell_sidebar_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			E_TYPE_SHELL_SIDEBAR, "ECalShellSidebar",
			&type_info, 0);
	}

	return type;
}

GtkWidget *
e_cal_shell_sidebar_new (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return g_object_new (
		E_TYPE_CAL_SHELL_SIDEBAR,
		"shell-view", shell_view, NULL);
}

GtkWidget *
e_cal_shell_sidebar_get_selector (ECalShellSidebar *cal_shell_sidebar)
{
	g_return_val_if_fail (
		E_IS_CAL_SHELL_SIDEBAR (cal_shell_sidebar), NULL);

	return cal_shell_sidebar->priv->selector;
}
