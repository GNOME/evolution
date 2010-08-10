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

#ifndef CAL_PREFS_DIALOG_H
#define CAL_PREFS_DIALOG_H

#include <gtk/gtk.h>
#include <gconf/gconf-client.h>
#include <libedataserverui/e-source-selector.h>
#include <widgets/misc/e-preferences-window.h>

/* Standard GObject macros */
#define CALENDAR_TYPE_PREFS_DIALOG \
	(calendar_prefs_dialog_get_type ())
#define CALENDAR_PREFS_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), CALENDAR_TYPE_PREFS_DIALOG, CalendarPrefsDialog))
#define CALENDAR_PREFS_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), CALENDAR_TYPE_PREFS_DIALOG, CalendarPrefsDialogClass))
#define CALENDAR_IS_PREFS_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), CALENDAR_TYPE_PREFS_DIALOG))
#define CALENDAR_IS_PREFS_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), CALENDAR_TYPE_PREFS_DIALOG))
#define CALENDAR_PREFS_DIALOG_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), CALENDAR_TYPE_PREFS_DIALOG, CalendarPrefsDialogClass))

G_BEGIN_DECLS

typedef struct _CalendarPrefsDialog CalendarPrefsDialog;
typedef struct _CalendarPrefsDialogClass CalendarPrefsDialogClass;

struct _CalendarPrefsDialog {
	GtkVBox parent;

	GtkBuilder *builder;

	GConfClient *gconf;

	/* General tab */
	GtkWidget *day_second_zone;
	GtkWidget *working_days[7];
	GtkWidget *week_start_day;
	GtkWidget *start_of_day;
	GtkWidget *end_of_day;
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
	GtkWidget *month_scroll_by_week;
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

GType		calendar_prefs_dialog_get_type	(void);
GtkWidget *	calendar_prefs_dialog_new	(EPreferencesWindow *window);

G_END_DECLS

#endif /* CAL_PREFS_DIALOG_H */
