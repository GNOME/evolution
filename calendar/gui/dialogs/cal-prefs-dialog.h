/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		David Trowbridge <trowbrds cs colorado edu>
 *		Damon Chaplin <damon@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _CAL_PREFS_DIALOG_H_
#define _CAL_PREFS_DIALOG_H_

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <gconf/gconf-client.h>
#include <libedataserverui/e-source-selector.h>
#include "evolution-config-control.h"

G_BEGIN_DECLS

typedef struct _CalendarPrefsDialog CalendarPrefsDialog;
typedef struct _CalendarPrefsDialogClass CalendarPrefsDialogClass;

struct _CalendarPrefsDialog {
	GtkVBox parent;

	GladeXML *gui;

	GConfClient *gconf;

	/* General tab */
	GtkWidget *use_system_tz_check;
	GtkWidget *system_tz_label;
	GtkWidget *timezone;
	GtkWidget *day_second_zone;
	GtkWidget *working_days[7];
	GtkWidget *week_start_day;
	GtkWidget *start_of_day;
	GtkWidget *end_of_day;
	GtkWidget *use_12_hour;
	GtkWidget *use_24_hour;
	GtkWidget *confirm_delete;
	GtkWidget *default_reminder;
	GtkWidget *default_reminder_interval;
	GtkWidget *default_reminder_units;
	GtkWidget *ba_reminder;
	GtkWidget *ba_reminder_interval;
	GtkWidget *ba_reminder_units;

	/* Display tab */
	GtkWidget *time_divisions;
	GtkWidget *show_end_times;
	GtkWidget *compress_weekend;
	GtkWidget *dnav_show_week_no;
	GtkWidget *dview_show_week_no;
	GtkWidget *month_scroll_by_week;
	GtkWidget *tasks_due_today_color;
	GtkWidget *tasks_overdue_color;
	GtkWidget *tasks_hide_completed;
	GtkWidget *tasks_hide_completed_interval;
	GtkWidget *tasks_hide_completed_units;

	/* Alarms tab */
	GtkWidget *notify_with_tray;
	GtkWidget *scrolled_window;
	ESourceList *alarms_list;
	GtkWidget *alarm_list_widget;

	/* Free/Busy tab */
	GtkWidget *url_add;
	GtkWidget *url_edit;
	GtkWidget *url_remove;
	GtkWidget *url_enable;
	GtkWidget *url_enable_label;
	GtkWidget *url_enable_image;
	GtkWidget *url_list;
	GtkWidget *template_url;
	guint destroyed : 1;
};

struct _CalendarPrefsDialogClass {
	GtkVBoxClass parent;
};

GType      calendar_prefs_dialog_get_type (void);
GtkWidget *calendar_prefs_dialog_new (void);

G_END_DECLS

#endif /* _CAL_PREFS_DIALOG_H_ */
