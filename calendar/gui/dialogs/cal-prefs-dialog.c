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
 *		Damon Chaplin <damon@ximian.com>
 *		Ettore Perazzoli <ettore@ximian.com>
 *		David Trowbridge <trowbrds cs colorado edu>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "../e-cal-config.h"
#include "../e-timezone-entry.h"
#include "../calendar-config.h"
#include "cal-prefs-dialog.h"
#include <widgets/misc/e-dateedit.h>
#include "e-util/e-util.h"
#include "e-util/e-binding.h"
#include "e-util/e-datetime-format.h"
#include "e-util/e-dialog-widgets.h"
#include "e-util/e-util-private.h"
#include "shell/e-shell-utils.h"
#include <glib/gi18n.h>
#include <string.h>

static const gint time_division_map[] = {
	60, 30, 15, 10, 5, -1
};

/* The following two are kept separate in case we need to re-order each menu individually */
static const gint hide_completed_units_map[] = {
	CAL_MINUTES, CAL_HOURS, CAL_DAYS, -1
};

/* same is used for Birthdays & Anniversaries calendar */
static const gint default_reminder_units_map[] = {
	CAL_MINUTES, CAL_HOURS, CAL_DAYS, -1
};

static GtkVBoxClass *parent_class = NULL;

static void
calendar_prefs_dialog_finalize (GObject *obj)
{
	CalendarPrefsDialog *prefs = (CalendarPrefsDialog *) obj;

	g_object_unref (prefs->builder);

	if (prefs->gconf) {
		g_object_unref (prefs->gconf);
		prefs->gconf = NULL;
	}

	((GObjectClass *)(parent_class))->finalize (obj);
}

static void
calendar_prefs_dialog_class_init (CalendarPrefsDialogClass *klass)
{
	GObjectClass *object_class;

	object_class = (GObjectClass *) klass;
	parent_class = g_type_class_ref (GTK_TYPE_VBOX);

	object_class->finalize = calendar_prefs_dialog_finalize;
}

static void
calendar_prefs_dialog_init (CalendarPrefsDialog *dialog)
{
}

static GtkWidget *
eccp_widget_glade (EConfig *ec, EConfigItem *item, GtkWidget *parent, GtkWidget *old, gpointer data)
{
	CalendarPrefsDialog *prefs = data;

	return e_builder_get_widget (prefs->builder, item->label);
}

static void
update_day_second_zone_caption (CalendarPrefsDialog *prefs)
{
	gchar *location;
	const gchar *caption;
	icaltimezone *zone;

	g_return_if_fail (prefs != NULL);

	/* Translators: "None" indicates no second time zone set for a day view */
	caption = C_("cal-second-zone", "None");

	location = calendar_config_get_day_second_zone ();
	if (location && *location) {
		zone = icaltimezone_get_builtin_timezone (location);
		if (zone && icaltimezone_get_display_name (zone)) {
			caption = icaltimezone_get_display_name (zone);
		}
	}
	g_free (location);

	gtk_button_set_label (GTK_BUTTON (prefs->day_second_zone), caption);
}

static void
on_set_day_second_zone (GtkWidget *item, CalendarPrefsDialog *prefs)
{
	if (!gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (item)))
		return;

	calendar_config_set_day_second_zone (g_object_get_data (G_OBJECT (item), "timezone"));
	update_day_second_zone_caption (prefs);
}

static void
on_select_day_second_zone (GtkWidget *item, CalendarPrefsDialog *prefs)
{
	g_return_if_fail (prefs != NULL);

	calendar_config_select_day_second_zone ();
	update_day_second_zone_caption (prefs);
}

static void
day_second_zone_clicked (GtkWidget *widget, CalendarPrefsDialog *prefs)
{
	GtkWidget *menu, *item;
	GSList *group = NULL, *recent_zones, *s;
	gchar *location;
	icaltimezone *zone, *second_zone = NULL;

	menu = gtk_menu_new ();

	location = calendar_config_get_day_second_zone ();
	if (location && *location)
		second_zone = icaltimezone_get_builtin_timezone (location);
	g_free (location);

	group = NULL;
	item = gtk_radio_menu_item_new_with_label (group, C_("cal-second-zone", "None"));
	group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (item));
	if (!second_zone)
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), TRUE);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	g_signal_connect (item, "toggled", G_CALLBACK (on_set_day_second_zone), prefs);

	recent_zones = calendar_config_get_day_second_zones ();
	for (s = recent_zones; s != NULL; s = s->next) {
		zone = icaltimezone_get_builtin_timezone (s->data);
		if (!zone)
			continue;

		item = gtk_radio_menu_item_new_with_label (group, icaltimezone_get_display_name (zone));
		group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (item));
		/* both comes from builtin, thus no problem to compare pointers */
		if (zone == second_zone)
			gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), TRUE);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
		g_object_set_data_full (G_OBJECT (item), "timezone", g_strdup (s->data), g_free);
		g_signal_connect (item, "toggled", G_CALLBACK (on_set_day_second_zone), prefs);
	}
	calendar_config_free_day_second_zones (recent_zones);

	item = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	item = gtk_menu_item_new_with_label (_("Select..."));
	g_signal_connect (item, "activate", G_CALLBACK (on_select_day_second_zone), prefs);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	gtk_widget_show_all (menu);

	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL,
			0, gtk_get_current_event_time ());
}

static void
start_of_day_changed (GtkWidget *widget, CalendarPrefsDialog *prefs)
{
	gint start_hour, start_minute, end_hour, end_minute;
	EDateEdit *start, *end;

	start = E_DATE_EDIT (prefs->start_of_day);
	end = E_DATE_EDIT (prefs->end_of_day);

	e_date_edit_get_time_of_day (start, &start_hour, &start_minute);
	e_date_edit_get_time_of_day (end, &end_hour, &end_minute);

	if ((start_hour > end_hour) || (start_hour == end_hour && start_minute > end_minute)) {
		if (start_hour < 23)
			e_date_edit_set_time_of_day (end, start_hour + 1, start_minute);
		else
			e_date_edit_set_time_of_day (end, 23, 59);

		return;
	}

	calendar_config_set_day_start_hour (start_hour);
	calendar_config_set_day_start_minute (start_minute);
}

static void
end_of_day_changed (GtkWidget *widget, CalendarPrefsDialog *prefs)
{
	gint start_hour, start_minute, end_hour, end_minute;
	EDateEdit *start, *end;

	start = E_DATE_EDIT (prefs->start_of_day);
	end = E_DATE_EDIT (prefs->end_of_day);

	e_date_edit_get_time_of_day (start, &start_hour, &start_minute);
	e_date_edit_get_time_of_day (end, &end_hour, &end_minute);

	if ((end_hour < start_hour) || (end_hour == start_hour && end_minute < start_minute)) {
		if (end_hour < 1)
			e_date_edit_set_time_of_day (start, 0, 0);
		else
			e_date_edit_set_time_of_day (start, end_hour - 1, end_minute);

		return;
	}

	calendar_config_set_day_end_hour (end_hour);
	calendar_config_set_day_end_minute (end_minute);
}

static void
time_divisions_changed (GtkWidget *widget, CalendarPrefsDialog *prefs)
{
	gint time_divisions;

	time_divisions = e_dialog_combo_box_get (prefs->time_divisions, time_division_map);
	calendar_config_set_time_divisions (time_divisions);
}

static void
month_scroll_by_week_toggled (GtkToggleButton *toggle, CalendarPrefsDialog *prefs)
{
	calendar_config_set_month_scroll_by_week (gtk_toggle_button_get_active (toggle));
}

static void
hide_completed_tasks_toggled (GtkToggleButton *toggle, CalendarPrefsDialog *prefs)
{
	gboolean hide;

	hide = gtk_toggle_button_get_active (toggle);

	gtk_widget_set_sensitive (prefs->tasks_hide_completed_interval, hide);
	gtk_widget_set_sensitive (prefs->tasks_hide_completed_units, hide);

	calendar_config_set_hide_completed_tasks (hide);
}

static void
hide_completed_tasks_changed (GtkWidget *widget, CalendarPrefsDialog *prefs)
{
	calendar_config_set_hide_completed_tasks_value (
		gtk_spin_button_get_value_as_int (
		GTK_SPIN_BUTTON (prefs->tasks_hide_completed_interval)));
}

static void
hide_completed_tasks_units_changed (GtkWidget *widget, CalendarPrefsDialog *prefs)
{
	calendar_config_set_hide_completed_tasks_units (
		e_dialog_combo_box_get (prefs->tasks_hide_completed_units, hide_completed_units_map));
}

static void
default_reminder_toggled (GtkToggleButton *toggle, CalendarPrefsDialog *prefs)
{
	calendar_config_set_use_default_reminder (gtk_toggle_button_get_active (toggle));
}

static void
default_reminder_interval_changed (GtkWidget *widget, CalendarPrefsDialog *prefs)
{
	const gchar *str;
	gdouble value;

	str = gtk_entry_get_text (GTK_ENTRY (widget));
	value = g_ascii_strtod (str, NULL);

	calendar_config_set_default_reminder_interval (value);
}

static void
default_reminder_units_changed (GtkWidget *widget, CalendarPrefsDialog *prefs)
{
	calendar_config_set_default_reminder_units (
		e_dialog_combo_box_get (prefs->default_reminder_units, default_reminder_units_map));
}

static void
ba_reminder_toggled (GtkToggleButton *toggle, CalendarPrefsDialog *prefs)
{
	gboolean enabled = gtk_toggle_button_get_active (toggle);

	calendar_config_set_ba_reminder (&enabled, NULL, NULL);
}

static void
ba_reminder_interval_changed (GtkWidget *widget, CalendarPrefsDialog *prefs)
{
	const gchar *str;
	gint value;

	str = gtk_entry_get_text (GTK_ENTRY (widget));
	value = (gint) g_ascii_strtod (str, NULL);

	calendar_config_set_ba_reminder (NULL, &value, NULL);
}

static void
ba_reminder_units_changed (GtkWidget *widget, CalendarPrefsDialog *prefs)
{
	CalUnits units = e_dialog_combo_box_get (prefs->ba_reminder_units, default_reminder_units_map);

	calendar_config_set_ba_reminder (NULL, NULL, &units);
}

static void
notify_with_tray_toggled (GtkToggleButton *toggle, CalendarPrefsDialog *prefs)
{
	GConfClient *gconf;

	g_return_if_fail (toggle != NULL);

	gconf = gconf_client_get_default ();
	gconf_client_set_bool (gconf, "/apps/evolution/calendar/notify/notify_with_tray", gtk_toggle_button_get_active (toggle), NULL);
	g_object_unref (gconf);
}

static void
alarms_selection_changed (ESourceSelector *selector, CalendarPrefsDialog *prefs)
{
	ESourceList *source_list = prefs->alarms_list;
	GSList *selection;
	GSList *l;
	GSList *groups;
	ESource *source;
	const gchar *alarm;

	/* first we clear all the alarm flags from all sources */
	for (groups = e_source_list_peek_groups (source_list); groups; groups = groups->next) {
		ESourceGroup *group = E_SOURCE_GROUP (groups->data);
		GSList *sources;
		for (sources = e_source_group_peek_sources (group); sources; sources = sources->next) {
			source = E_SOURCE (sources->data);

			alarm = e_source_get_property (source, "alarm");
			if (alarm && !g_ascii_strcasecmp (alarm, "never"))
				continue;

			e_source_set_property (source, "alarm", "false");
		}
	}

	/* then we loop over the selector's selection, setting the
	   property on those sources */
	selection = e_source_selector_get_selection (selector);
	for (l = selection; l; l = l->next) {
		source = E_SOURCE (l->data);

		alarm = (gchar *)e_source_get_property (source, "alarm");
		if (alarm && !g_ascii_strcasecmp (alarm, "never"))
			continue;

		e_source_set_property (E_SOURCE (l->data), "alarm", "true");
	}
	e_source_selector_free_selection (selection);

	/* FIXME show an error if this fails? */
	e_source_list_sync (source_list, NULL);
}

static void
update_system_tz_widgets (EShellSettings *shell_settings,
                          GParamSpec *pspec,
                          CalendarPrefsDialog *prefs)
{
	GtkWidget *widget;
	icaltimezone *zone;
	const gchar *display_name;
	gchar *text;

	widget = e_builder_get_widget (prefs->builder, "system-tz-label");
	g_return_if_fail (GTK_IS_LABEL (widget));

	zone = e_cal_util_get_system_timezone ();
	if (zone != NULL)
		display_name = gettext (icaltimezone_get_display_name (zone));
	else
		display_name = "UTC";

	text = g_strdup_printf ("(%s)", display_name);
	gtk_label_set_text (GTK_LABEL (widget), text);
	g_free (text);
}

static void
setup_changes (CalendarPrefsDialog *prefs)
{
	g_signal_connect (G_OBJECT (prefs->day_second_zone), "clicked", G_CALLBACK (day_second_zone_clicked), prefs);

	g_signal_connect (G_OBJECT (prefs->start_of_day), "changed", G_CALLBACK (start_of_day_changed), prefs);
	g_signal_connect (G_OBJECT (prefs->end_of_day), "changed", G_CALLBACK (end_of_day_changed), prefs);

	g_signal_connect (G_OBJECT (prefs->time_divisions), "changed", G_CALLBACK (time_divisions_changed), prefs);

	g_signal_connect (G_OBJECT (prefs->month_scroll_by_week), "toggled", G_CALLBACK (month_scroll_by_week_toggled), prefs);

	g_signal_connect (G_OBJECT (prefs->tasks_hide_completed), "toggled",
			  G_CALLBACK (hide_completed_tasks_toggled), prefs);
	g_signal_connect (G_OBJECT (prefs->tasks_hide_completed_interval), "value-changed",
			  G_CALLBACK (hide_completed_tasks_changed), prefs);
	g_signal_connect (G_OBJECT (prefs->tasks_hide_completed_units), "changed", G_CALLBACK (hide_completed_tasks_units_changed), prefs);

	g_signal_connect (G_OBJECT (prefs->default_reminder), "toggled", G_CALLBACK (default_reminder_toggled), prefs);
	g_signal_connect (G_OBJECT (prefs->default_reminder_interval), "changed",
			  G_CALLBACK (default_reminder_interval_changed), prefs);
	g_signal_connect (G_OBJECT (prefs->default_reminder_units), "changed", G_CALLBACK (default_reminder_units_changed), prefs);

	g_signal_connect (G_OBJECT (prefs->ba_reminder), "toggled", G_CALLBACK (ba_reminder_toggled), prefs);
	g_signal_connect (G_OBJECT (prefs->ba_reminder_interval), "changed",
			  G_CALLBACK (ba_reminder_interval_changed), prefs);
	g_signal_connect (G_OBJECT (prefs->ba_reminder_units), "changed", G_CALLBACK (ba_reminder_units_changed), prefs);

	g_signal_connect (G_OBJECT (prefs->notify_with_tray), "toggled", G_CALLBACK (notify_with_tray_toggled), prefs);
	g_signal_connect (G_OBJECT (prefs->alarm_list_widget), "selection_changed", G_CALLBACK (alarms_selection_changed), prefs);
}

/* Shows the current task list settings in the dialog */
static void
show_task_list_config (CalendarPrefsDialog *prefs)
{
	CalUnits units;
	gboolean hide_completed_tasks;

	/* Hide Completed Tasks. */
	hide_completed_tasks = calendar_config_get_hide_completed_tasks ();
	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (prefs->tasks_hide_completed),
		hide_completed_tasks);

	/* Hide Completed Tasks Units. */
	units = calendar_config_get_hide_completed_tasks_units ();
	e_dialog_combo_box_set (prefs->tasks_hide_completed_units, units, hide_completed_units_map);

	/* Hide Completed Tasks Value. */
	gtk_spin_button_set_value (
		GTK_SPIN_BUTTON (prefs->tasks_hide_completed_interval),
		calendar_config_get_hide_completed_tasks_value ());

	gtk_widget_set_sensitive (prefs->tasks_hide_completed_interval, hide_completed_tasks);
	gtk_widget_set_sensitive (prefs->tasks_hide_completed_units, hide_completed_tasks);
}

static void
initialize_selection (ESourceSelector *selector, ESourceList *source_list)
{
	GSList *groups;

	for (groups = e_source_list_peek_groups (source_list); groups; groups = groups->next) {
		ESourceGroup *group = E_SOURCE_GROUP (groups->data);
		GSList *sources;
		for (sources = e_source_group_peek_sources (group); sources; sources = sources->next) {
			ESource *source = E_SOURCE (sources->data);
			const gchar *completion = e_source_get_property (source, "alarm");
			if (!completion  || !g_ascii_strcasecmp (completion, "true")) {
				if (!completion)
					e_source_set_property (E_SOURCE (source), "alarm", "true");
				e_source_selector_select_source (selector, source);
			}
		}
	}
}

static void
show_alarms_config (CalendarPrefsDialog *prefs)
{
	GConfClient *gconf;

	if (e_cal_get_sources (&prefs->alarms_list, E_CAL_SOURCE_TYPE_EVENT, NULL)) {
		prefs->alarm_list_widget = e_source_selector_new (prefs->alarms_list);
		atk_object_set_name (gtk_widget_get_accessible (prefs->alarm_list_widget), _("Selected Calendars for Alarms"));
		gtk_container_add (GTK_CONTAINER (prefs->scrolled_window), prefs->alarm_list_widget);
		gtk_widget_show (prefs->alarm_list_widget);
		initialize_selection (E_SOURCE_SELECTOR (prefs->alarm_list_widget), prefs->alarms_list);
	}

	gconf = gconf_client_get_default ();
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefs->notify_with_tray), gconf_client_get_bool (gconf, "/apps/evolution/calendar/notify/notify_with_tray", NULL));
	g_object_unref (gconf);
}

/* Shows the current config settings in the dialog. */
static void
show_config (CalendarPrefsDialog *prefs)
{
	gint time_divisions;
	gboolean set = FALSE;
	CalUnits units;
	gint interval;

	/* Day's second zone */
	update_day_second_zone_caption (prefs);

	/* Day's second zone */
	update_day_second_zone_caption (prefs);

	/* Start of Day. */
	e_date_edit_set_time_of_day (E_DATE_EDIT (prefs->start_of_day), calendar_config_get_day_start_hour (), calendar_config_get_day_start_minute ());

	/* End of Day. */
	e_date_edit_set_time_of_day (E_DATE_EDIT (prefs->end_of_day), calendar_config_get_day_end_hour (), calendar_config_get_day_end_minute ());

	/* Time Divisions. */
	time_divisions = calendar_config_get_time_divisions ();
	e_dialog_combo_box_set (prefs->time_divisions, time_divisions, time_division_map);

	/* Month View - Scroll by a week */
	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (prefs->month_scroll_by_week),
		calendar_config_get_month_scroll_by_week ());

	/* Task list */
	show_task_list_config (prefs);

	/* Alarms list*/
	show_alarms_config (prefs);

	/* Other page */
	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (prefs->default_reminder),
		calendar_config_get_use_default_reminder ());
	gtk_spin_button_set_value (
		GTK_SPIN_BUTTON (prefs->default_reminder_interval),
		calendar_config_get_default_reminder_interval ());
	e_dialog_combo_box_set (prefs->default_reminder_units, calendar_config_get_default_reminder_units (), default_reminder_units_map);

	/* Birthdays & Anniversaries reminder */
	set = calendar_config_get_ba_reminder (&interval, &units);

	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (prefs->ba_reminder), set);
	gtk_spin_button_set_value (
		GTK_SPIN_BUTTON (prefs->ba_reminder_interval), interval);
	e_dialog_combo_box_set (prefs->ba_reminder_units, units, default_reminder_units_map);
}

/* plugin meta-data */
static ECalConfigItem eccp_items[] = {
	{ E_CONFIG_BOOK,          (gchar *) "",                             (gchar *) "toplevel-notebook", eccp_widget_glade },
	{ E_CONFIG_PAGE,          (gchar *) "00.general",                   (gchar *) "general",           eccp_widget_glade },
	{ E_CONFIG_SECTION_TABLE, (gchar *) "00.general/00.time",           (gchar *) "time",              eccp_widget_glade },
	{ E_CONFIG_SECTION_TABLE, (gchar *) "00.general/10.workWeek",       (gchar *) "workWeek",          eccp_widget_glade },
	{ E_CONFIG_SECTION,       (gchar *) "00.general/20.alerts",         (gchar *) "alerts",            eccp_widget_glade },
	{ E_CONFIG_PAGE,          (gchar *) "10.display",                   (gchar *) "display",           eccp_widget_glade },
	{ E_CONFIG_SECTION,       (gchar *) "10.display/00.general",        (gchar *) "displayGeneral",    eccp_widget_glade },
	{ E_CONFIG_SECTION,       (gchar *) "10.display/10.taskList",       (gchar *) "taskList",          eccp_widget_glade },
	{ E_CONFIG_PAGE,          (gchar *) "15.alarms",                    (gchar *) "alarms",            eccp_widget_glade },
	{ E_CONFIG_PAGE,          (gchar *) "20.freeBusy",                  (gchar *) "freebusy",          eccp_widget_glade },
	{ E_CONFIG_SECTION,       (gchar *) "20.freeBusy/00.defaultServer", (gchar *) "defaultFBServer",   eccp_widget_glade },
};

static void
eccp_free (EConfig *ec, GSList *items, gpointer data)
{
	g_slist_free (items);
}

static void
calendar_prefs_dialog_construct (CalendarPrefsDialog *prefs,
                                 EShell *shell)
{
	ECalConfig *ec;
	ECalConfigTargetPrefs *target;
	EShellSettings *shell_settings;
	gboolean locale_supports_12_hour_format;
	gint i;
	GtkWidget *toplevel;
	GtkWidget *widget;
	GtkWidget *table;
	GSList *l;

	shell_settings = e_shell_get_shell_settings (shell);

	locale_supports_12_hour_format =
		calendar_config_locale_supports_12_hour_format ();

	/* Force 24 hour format for locales which don't support 12 hour format */
	if (!locale_supports_12_hour_format
	    && !e_shell_settings_get_boolean (shell_settings, "cal-use-24-hour-format"))
		e_shell_settings_set_boolean (shell_settings, "cal-use-24-hour-format", TRUE);

	/* Make sure our custom widget classes are registered with
	 * GType before we load the GtkBuilder definition file. */
	E_TYPE_DATE_EDIT;
	E_TYPE_TIMEZONE_ENTRY;

	prefs->builder = gtk_builder_new ();
	e_load_ui_builder_definition (prefs->builder, "cal-prefs-dialog.ui");

	prefs->gconf = gconf_client_get_default ();

	/** @HookPoint-ECalConfig: Calendar Preferences Page
	 * @Id: org.gnome.evolution.calendar.prefs
	 * @Type: E_CONFIG_BOOK
	 * @Class: org.gnome.evolution.calendar.config:1.0
	 * @Target: ECalConfigTargetPrefs
	 *
	 * The mail calendar preferences page
	 */
	ec = e_cal_config_new (E_CONFIG_BOOK, "org.gnome.evolution.calendar.prefs");
	l = NULL;
	for (i = 0; i < G_N_ELEMENTS (eccp_items); i++)
		l = g_slist_prepend (l, &eccp_items[i]);
	e_config_add_items ((EConfig *) ec, l, NULL, NULL, eccp_free, prefs);

	widget = e_builder_get_widget (prefs->builder, "use-system-tz-check");
	e_mutual_binding_new (
		shell_settings, "cal-use-system-timezone",
		widget, "active");
	g_signal_connect (
		shell_settings, "notify::cal-use-system-timezone",
		G_CALLBACK (update_system_tz_widgets), prefs);
	g_object_notify (G_OBJECT (shell_settings), "cal-use-system-timezone");

	widget = e_builder_get_widget (prefs->builder, "timezone");
	e_mutual_binding_new (
		shell_settings, "cal-timezone",
		widget, "timezone");
	e_mutual_binding_new_with_negation (
		shell_settings, "cal-use-system-timezone",
		widget, "sensitive");

	/* General tab */
	prefs->day_second_zone = e_builder_get_widget (prefs->builder, "day_second_zone");

	widget = e_builder_get_widget (prefs->builder, "sun_button");
	e_mutual_binding_new (
		shell_settings, "cal-working-days-sunday",
		widget, "active");

	widget = e_builder_get_widget (prefs->builder, "mon_button");
	e_mutual_binding_new (
		shell_settings, "cal-working-days-monday",
		widget, "active");

	widget = e_builder_get_widget (prefs->builder, "tue_button");
	e_mutual_binding_new (
		shell_settings, "cal-working-days-tuesday",
		widget, "active");

	widget = e_builder_get_widget (prefs->builder, "wed_button");
	e_mutual_binding_new (
		shell_settings, "cal-working-days-wednesday",
		widget, "active");

	widget = e_builder_get_widget (prefs->builder, "thu_button");
	e_mutual_binding_new (
		shell_settings, "cal-working-days-thursday",
		widget, "active");

	widget = e_builder_get_widget (prefs->builder, "fri_button");
	e_mutual_binding_new (
		shell_settings, "cal-working-days-friday",
		widget, "active");

	widget = e_builder_get_widget (prefs->builder, "sat_button");
	e_mutual_binding_new (
		shell_settings, "cal-working-days-saturday",
		widget, "active");

	widget = e_builder_get_widget (prefs->builder, "week_start_day");
	e_mutual_binding_new (
		shell_settings, "cal-week-start-day",
		widget, "active");

	widget = e_builder_get_widget (prefs->builder, "start_of_day");
	prefs->start_of_day = widget;  /* XXX delete this */
	if (locale_supports_12_hour_format)
		e_binding_new (
			shell_settings, "cal-use-24-hour-format",
			widget, "use-24-hour-format");

	widget = e_builder_get_widget (prefs->builder, "end_of_day");
	prefs->end_of_day = widget;  /* XXX delete this */
	if (locale_supports_12_hour_format)
		e_binding_new (
			shell_settings, "cal-use-24-hour-format",
			widget, "use-24-hour-format");

	widget = e_builder_get_widget (prefs->builder, "use_12_hour");
	gtk_widget_set_sensitive (widget, locale_supports_12_hour_format);
	e_mutual_binding_new_with_negation (
		shell_settings, "cal-use-24-hour-format",
		widget, "active");

	widget = e_builder_get_widget (prefs->builder, "use_24_hour");
	gtk_widget_set_sensitive (widget, locale_supports_12_hour_format);
	e_mutual_binding_new (
		shell_settings, "cal-use-24-hour-format",
		widget, "active");

	widget = e_builder_get_widget (prefs->builder, "confirm_delete");
	e_mutual_binding_new (
		shell_settings, "cal-confirm-delete",
		widget, "active");

	prefs->default_reminder = e_builder_get_widget (prefs->builder, "default_reminder");
	prefs->default_reminder_interval = e_builder_get_widget (prefs->builder, "default_reminder_interval");
	prefs->default_reminder_units = e_builder_get_widget (prefs->builder, "default_reminder_units");
	prefs->ba_reminder = e_builder_get_widget (prefs->builder, "ba_reminder");
	prefs->ba_reminder_interval = e_builder_get_widget (prefs->builder, "ba_reminder_interval");
	prefs->ba_reminder_units = e_builder_get_widget (prefs->builder, "ba_reminder_units");

	/* Display tab */
	prefs->time_divisions = e_builder_get_widget (prefs->builder, "time_divisions");

	widget = e_builder_get_widget (prefs->builder, "show_end_times");
	e_mutual_binding_new (
		shell_settings, "cal-show-event-end-times",
		widget, "active");

	widget = e_builder_get_widget (prefs->builder, "compress_weekend");
	e_mutual_binding_new (
		shell_settings, "cal-compress-weekend",
		widget, "active");

	widget = e_builder_get_widget (prefs->builder, "show_week_numbers");
	e_mutual_binding_new (
		shell_settings, "cal-show-week-numbers",
		widget, "active");

	prefs->month_scroll_by_week = e_builder_get_widget (prefs->builder, "month_scroll_by_week");

	widget = e_builder_get_widget (prefs->builder, "tasks_due_today_color");
	e_mutual_binding_new_full (
		shell_settings, "cal-tasks-color-due-today",
		widget, "color",
		e_binding_transform_string_to_color,
		e_binding_transform_color_to_string,
		(GDestroyNotify) NULL, NULL);

	widget = e_builder_get_widget (prefs->builder, "tasks_overdue_color");
	e_mutual_binding_new_full (
		shell_settings, "cal-tasks-color-overdue",
		widget, "color",
		e_binding_transform_string_to_color,
		e_binding_transform_color_to_string,
		(GDestroyNotify) NULL, NULL);

	prefs->tasks_hide_completed = e_builder_get_widget (prefs->builder, "tasks_hide_completed");
	prefs->tasks_hide_completed_interval = e_builder_get_widget (prefs->builder, "tasks_hide_completed_interval");
	prefs->tasks_hide_completed_units = e_builder_get_widget (prefs->builder, "tasks_hide_completed_units");

	/* Alarms tab */
	prefs->notify_with_tray = e_builder_get_widget (prefs->builder, "notify_with_tray");
	prefs->scrolled_window = e_builder_get_widget (prefs->builder, "calendar-source-scrolled-window");

	/* Free/Busy tab */
	widget = e_builder_get_widget (prefs->builder, "template_url");
	e_mutual_binding_new (
		shell_settings, "cal-free-busy-template",
		widget, "text");

	/* date/time format */
	table = e_builder_get_widget (prefs->builder, "datetime_format_table");
	e_datetime_format_add_setup_widget (table, 0, "calendar", "table",  DTFormatKindDateTime, _("Ti_me and date:"));
	e_datetime_format_add_setup_widget (table, 1, "calendar", "table",  DTFormatKindDate, _("_Date only:"));

	/* Hide senseless preferences when running in Express mode */
	e_shell_hide_widgets_for_express_mode (shell, prefs->builder,
					       "label_second_zone",
					       "hbox_second_zone",
					       "timezone",
					       "timezone_label",
					       "hbox_use_system_timezone",
					       "hbox_time_divisions",
					       "show_end_times",
					       "month_scroll_by_week",
					       NULL);

	/* HACK:  GTK+ 2.18 and 2.20 has a GtkTable which includes row/column spacing even for empty rows/columns.
	 * When Evo runs in Express mode, we hide all the rows in the Time section of the calendar's General
	 * preferences page.  However, due to that behavior in GTK+, we get a lot of extra spacing in that
	 * section.  Since we know that in Express mode we only leave a single row visible, we'll make the
	 * table's row spacing equal to 0 in that case.
	 */
	if (e_shell_get_express_mode (shell)) {
		widget = e_builder_get_widget (prefs->builder, "time");
		gtk_table_set_row_spacings (GTK_TABLE (widget), 0);
	}

	/* Hook up and add the toplevel widget */

	target = e_cal_config_target_new_prefs (ec, prefs->gconf);
	e_config_set_target ((EConfig *)ec, (EConfigTarget *) target);
	toplevel = e_config_create_widget ((EConfig *)ec);
	gtk_container_add (GTK_CONTAINER (prefs), toplevel);

	show_config (prefs);
	/* FIXME: weakref? */
	setup_changes (prefs);
}

GType
calendar_prefs_dialog_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static GTypeInfo type_info = {
			sizeof (CalendarPrefsDialogClass),
			NULL, NULL,
			(GClassInitFunc) calendar_prefs_dialog_class_init,
			NULL, NULL,
			sizeof (CalendarPrefsDialog),
			0,
			(GInstanceInitFunc) calendar_prefs_dialog_init,
		};

		type = g_type_register_static (GTK_TYPE_VBOX, "CalendarPrefsDialog", &type_info, 0);
	}

	return type;
}

GtkWidget *
calendar_prefs_dialog_new (EPreferencesWindow *window)
{
	EShell *shell;
	CalendarPrefsDialog *dialog;

	shell = e_preferences_window_get_shell (window);

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	dialog = g_object_new (CALENDAR_TYPE_PREFS_DIALOG, NULL);

	/* FIXME Kill this function. */
	calendar_prefs_dialog_construct (dialog, shell);

	return GTK_WIDGET (dialog);
}
