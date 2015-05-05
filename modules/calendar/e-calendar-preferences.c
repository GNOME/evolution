/*
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
 *		Ettore Perazzoli <ettore@ximian.com>
 *		David Trowbridge <trowbrds cs colorado edu>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-calendar-preferences.h"

#include <string.h>
#include <glib/gi18n.h>

#include "calendar/gui/e-cal-config.h"
#include "calendar/gui/e-timezone-entry.h"
#include "calendar/gui/calendar-config.h"
#include "shell/e-shell-utils.h"

/* same is used for Birthdays & Anniversaries calendar */
static const gint default_reminder_units_map[] = {
	E_DURATION_MINUTES, E_DURATION_HOURS, E_DURATION_DAYS, -1
};

G_DEFINE_DYNAMIC_TYPE (
	ECalendarPreferences,
	e_calendar_preferences,
	GTK_TYPE_BOX)

static gboolean
calendar_preferences_map_string_to_integer (GValue *value,
                                            GVariant *variant,
                                            gpointer user_data)
{
	GEnumClass *enum_class = G_ENUM_CLASS (user_data);
	GEnumValue *enum_value;
	const gchar *nick;

	/* XXX GSettings should know how to bind enum settings to
	 *     integer properties.  I filed a bug asking for this:
	 *     https://bugzilla.gnome.org/show_bug.cgi?id=695217 */

	nick = g_variant_get_string (variant, NULL);
	enum_value = g_enum_get_value_by_nick (enum_class, nick);
	g_return_val_if_fail (enum_value != NULL, FALSE);
	g_value_set_int (value, enum_value->value);

	return TRUE;
}

static GVariant *
calendar_preferences_map_integer_to_string (const GValue *value,
                                            const GVariantType *expected_type,
                                            gpointer user_data)
{
	GEnumClass *enum_class = G_ENUM_CLASS (user_data);
	GEnumValue *enum_value;

	/* XXX GSettings should know how to bind enum settings to
	 *     integer properties.  I filed a bug asking for this:
	 *     https://bugzilla.gnome.org/show_bug.cgi?id=695217 */

	enum_value = g_enum_get_value (enum_class, g_value_get_int (value));
	g_return_val_if_fail (enum_value != NULL, NULL);

	return g_variant_new_string (enum_value->value_nick);
}

static gboolean
calendar_preferences_map_string_to_icaltimezone (GValue *value,
                                                 GVariant *variant,
                                                 gpointer user_data)
{
	GSettings *settings;
	const gchar *location = NULL;
	icaltimezone *timezone = NULL;

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");

	if (g_settings_get_boolean (settings, "use-system-timezone"))
		timezone = e_cal_util_get_system_timezone ();
	else
		location = g_variant_get_string (variant, NULL);

	if (location != NULL && *location != '\0')
		timezone = icaltimezone_get_builtin_timezone (location);

	if (timezone == NULL)
		timezone = icaltimezone_get_utc_timezone ();

	g_value_set_pointer (value, timezone);

	g_object_unref (settings);

	return TRUE;
}

static GVariant *
calendar_preferences_map_icaltimezone_to_string (const GValue *value,
                                                 const GVariantType *expected_type,
                                                 gpointer user_data)
{
	GVariant *variant;
	GSettings *settings;
	const gchar *location = NULL;
	gchar *location_str = NULL;
	icaltimezone *timezone;

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");

	if (g_settings_get_boolean (settings, "use-system-timezone")) {
		location_str = g_settings_get_string (settings, "timezone");
		location = location_str;
	} else {
		timezone = g_value_get_pointer (value);

		if (timezone != NULL)
			location = icaltimezone_get_location (timezone);
	}

	if (location == NULL)
		location = "UTC";

	variant = g_variant_new_string (location);

	g_free (location_str);

	g_object_unref (settings);

	return variant;
}

static gboolean
calendar_preferences_map_time_divisions_to_index (GValue *value,
                                                  GVariant *variant,
                                                  gpointer user_data)
{
	gboolean success = TRUE;

	switch (g_variant_get_int32 (variant)) {
		case 60:
			g_value_set_int (value, 0);
			break;
		case 30:
			g_value_set_int (value, 1);
			break;
		case 15:
			g_value_set_int (value, 2);
			break;
		case 10:
			g_value_set_int (value, 3);
			break;
		case 5:
			g_value_set_int (value, 4);
			break;
		default:
			success = FALSE;
	}

	return success;
}

static GVariant *
calendar_preferences_map_index_to_time_divisions (const GValue *value,
                                                  const GVariantType *expected_type,
                                                  gpointer user_data)
{
	switch (g_value_get_int (value)) {
		case 0:
			return g_variant_new_int32 (60);
		case 1:
			return g_variant_new_int32 (30);
		case 2:
			return g_variant_new_int32 (15);
		case 3:
			return g_variant_new_int32 (10);
		case 4:
			return g_variant_new_int32 (5);
		default:
			break;
	}

	return NULL;
}

static gboolean
calendar_preferences_map_string_to_gdk_color (GValue *value,
                                              GVariant *variant,
                                              gpointer user_data)
{
	GdkColor color;
	const gchar *string;
	gboolean success = FALSE;

	string = g_variant_get_string (variant, NULL);
	if (gdk_color_parse (string, &color)) {
		g_value_set_boxed (value, &color);
		success = TRUE;
	}

	return success;
}

static GVariant *
calendar_preferences_map_gdk_color_to_string (const GValue *value,
                                              const GVariantType *expected_type,
                                              gpointer user_data)
{
	GVariant *variant;
	const GdkColor *color;

	color = g_value_get_boxed (value);
	if (color == NULL) {
		variant = g_variant_new_string ("");
	} else {
		gchar *string;

		string = gdk_color_to_string (color);
		variant = g_variant_new_string (string);
		g_free (string);
	}

	return variant;
}

static void
calendar_preferences_dispose (GObject *object)
{
	ECalendarPreferences *prefs = (ECalendarPreferences *) object;

	if (prefs->builder != NULL) {
		g_object_unref (prefs->builder);
		prefs->builder = NULL;
	}

	if (prefs->registry != NULL) {
		g_object_unref (prefs->registry);
		prefs->registry = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_calendar_preferences_parent_class)->dispose (object);
}

static void
e_calendar_preferences_class_init (ECalendarPreferencesClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = calendar_preferences_dispose;
}

static void
e_calendar_preferences_class_finalize (ECalendarPreferencesClass *class)
{
}

static void
e_calendar_preferences_init (ECalendarPreferences *preferences)
{
	gtk_orientable_set_orientation (GTK_ORIENTABLE (preferences), GTK_ORIENTATION_VERTICAL);
}

static GtkWidget *
calendar_preferences_get_config_widget (EConfig *ec,
                                        EConfigItem *item,
                                        GtkWidget *parent,
                                        GtkWidget *old,
                                        gint position,
                                        gpointer data)
{
	ECalendarPreferences *preferences = data;

	return e_builder_get_widget (preferences->builder, item->label);
}

static void
update_day_second_zone_caption (ECalendarPreferences *prefs)
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
on_set_day_second_zone (GtkWidget *item,
                        ECalendarPreferences *prefs)
{
	if (!gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (item)))
		return;

	calendar_config_set_day_second_zone (g_object_get_data (G_OBJECT (item), "timezone"));
	update_day_second_zone_caption (prefs);
}

static void
on_select_day_second_zone (GtkWidget *item,
                           ECalendarPreferences *prefs)
{
	g_return_if_fail (prefs != NULL);

	calendar_config_select_day_second_zone ();
	update_day_second_zone_caption (prefs);
}

static void
day_second_zone_clicked (GtkWidget *widget,
                         ECalendarPreferences *prefs)
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
	g_signal_connect (
		item, "toggled",
		G_CALLBACK (on_set_day_second_zone), prefs);

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
		g_signal_connect (
			item, "toggled",
			G_CALLBACK (on_set_day_second_zone), prefs);
	}
	calendar_config_free_day_second_zones (recent_zones);

	item = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	item = gtk_menu_item_new_with_label (_("Select..."));
	g_signal_connect (
		item, "activate",
		G_CALLBACK (on_select_day_second_zone), prefs);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	gtk_widget_show_all (menu);

	gtk_menu_popup (
		GTK_MENU (menu), NULL, NULL, NULL, NULL,
		0, gtk_get_current_event_time ());
}

static void
start_of_day_changed (GtkWidget *widget,
                      ECalendarPreferences *prefs)
{
	EDateEdit *start, *end;
	GSettings *settings;
	gint start_hour;
	gint start_minute;
	gint end_hour;
	gint end_minute;

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

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");

	g_settings_set_int (settings, "day-start-hour", start_hour);
	g_settings_set_int (settings, "day-start-minute", start_minute);

	g_object_unref (settings);
}

static void
end_of_day_changed (GtkWidget *widget,
                    ECalendarPreferences *prefs)
{
	EDateEdit *start, *end;
	GSettings *settings;
	gint start_hour;
	gint start_minute;
	gint end_hour;
	gint end_minute;

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

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");

	g_settings_set_int (settings, "day-end-hour", end_hour);
	g_settings_set_int (settings, "day-end-minute", end_minute);

	g_object_unref (settings);
}

static void
update_system_tz_widgets (GtkCheckButton *button,
                          ECalendarPreferences *prefs)
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
setup_changes (ECalendarPreferences *prefs)
{
	g_signal_connect (
		prefs->day_second_zone, "clicked",
		G_CALLBACK (day_second_zone_clicked), prefs);

	g_signal_connect (
		prefs->start_of_day, "changed",
		G_CALLBACK (start_of_day_changed), prefs);

	g_signal_connect (
		prefs->end_of_day, "changed",
		G_CALLBACK (end_of_day_changed), prefs);
}

static void
show_alarms_config (ECalendarPreferences *prefs)
{
	GtkWidget *widget;

	widget = e_alarm_selector_new (prefs->registry);
	atk_object_set_name (
		gtk_widget_get_accessible (widget),
		_("Selected Calendars for Alarms"));
	gtk_container_add (GTK_CONTAINER (prefs->scrolled_window), widget);
	gtk_widget_show (widget);
}

/* Shows the current config settings in the dialog. */
static void
show_config (ECalendarPreferences *prefs)
{
	GSettings *settings;

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");

	/* Day's second zone */
	update_day_second_zone_caption (prefs);

	/* Start of Day. */
	e_date_edit_set_time_of_day (
		E_DATE_EDIT (prefs->start_of_day),
		g_settings_get_int (settings, "day-start-hour"),
		g_settings_get_int (settings, "day-start-minute"));

	/* End of Day. */
	e_date_edit_set_time_of_day (
		E_DATE_EDIT (prefs->end_of_day),
		g_settings_get_int (settings, "day-end-hour"),
		g_settings_get_int (settings, "day-end-minute"));

	/* Alarms list */
	show_alarms_config (prefs);

	g_object_unref (settings);
}

/* plugin meta-data */
static ECalConfigItem eccp_items[] = {
	{ E_CONFIG_BOOK,          (gchar *) "",                             (gchar *) "toplevel-notebook", calendar_preferences_get_config_widget },
	{ E_CONFIG_PAGE,          (gchar *) "00.general",                   (gchar *) "general",           calendar_preferences_get_config_widget },
	{ E_CONFIG_SECTION_TABLE, (gchar *) "00.general/00.time",           (gchar *) "time",              calendar_preferences_get_config_widget },
	{ E_CONFIG_SECTION_TABLE, (gchar *) "00.general/10.workWeek",       (gchar *) "workWeek",          calendar_preferences_get_config_widget },
	{ E_CONFIG_SECTION,       (gchar *) "00.general/20.alerts",         (gchar *) "alerts",            calendar_preferences_get_config_widget },
	{ E_CONFIG_PAGE,          (gchar *) "10.display",                   (gchar *) "display",           calendar_preferences_get_config_widget },
	{ E_CONFIG_SECTION,       (gchar *) "10.display/00.general",        (gchar *) "displayGeneral",    calendar_preferences_get_config_widget },
	{ E_CONFIG_SECTION,       (gchar *) "10.display/10.taskList",       (gchar *) "taskList",          calendar_preferences_get_config_widget },
	{ E_CONFIG_PAGE,          (gchar *) "12.tasks",                     (gchar *) "tasks-vbox",        calendar_preferences_get_config_widget },
	{ E_CONFIG_PAGE,          (gchar *) "15.alarms",                    (gchar *) "alarms",            calendar_preferences_get_config_widget },
	{ E_CONFIG_PAGE,          (gchar *) "20.freeBusy",                  (gchar *) "freebusy",          calendar_preferences_get_config_widget },
	{ E_CONFIG_SECTION,       (gchar *) "20.freeBusy/00.defaultServer", (gchar *) "default-freebusy-vbox",   calendar_preferences_get_config_widget },
};

static void
eccp_free (EConfig *ec,
           GSList *items,
           gpointer data)
{
	g_slist_free (items);
}

static void
calendar_preferences_construct (ECalendarPreferences *prefs,
                                EShell *shell)
{
	ECalConfig *ec;
	ECalConfigTargetPrefs *target;
	GSettings *settings;
	GSettings *eds_settings;
	gboolean locale_supports_12_hour_format;
	gint i;
	GtkWidget *toplevel;
	GtkWidget *widget;
	GtkWidget *table;
	GSList *l;

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");

	locale_supports_12_hour_format =
		calendar_config_locale_supports_12_hour_format ();

	/* Force 24 hour format for locales which don't support 12 hour format */
	if (!locale_supports_12_hour_format)
		g_settings_set_boolean (settings, "use-24hour-format", TRUE);

	/* Make sure our custom widget classes are registered with
	 * GType before we load the GtkBuilder definition file. */
	g_type_ensure (E_TYPE_DATE_EDIT);
	g_type_ensure (E_TYPE_TIMEZONE_ENTRY);

	prefs->builder = gtk_builder_new ();
	e_load_ui_builder_definition (prefs->builder, "e-calendar-preferences.ui");

	/** @HookPoint-ECalConfig: Calendar Preferences Page
	 * @Id: org.gnome.evolution.calendar.prefs
	 * @Class: org.gnome.evolution.calendar.config:1.0
	 * @Target: ECalConfigTargetPrefs
	 *
	 * The mail calendar preferences page
	 */
	ec = e_cal_config_new ("org.gnome.evolution.calendar.prefs");
	l = NULL;
	for (i = 0; i < G_N_ELEMENTS (eccp_items); i++)
		l = g_slist_prepend (l, &eccp_items[i]);
	e_config_add_items ((EConfig *) ec, l, eccp_free, prefs);

	widget = e_builder_get_widget (prefs->builder, "use-system-tz-check");
	g_settings_bind (
		settings, "use-system-timezone",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);
	g_signal_connect (
		widget, "toggled",
		G_CALLBACK (update_system_tz_widgets), prefs);
	update_system_tz_widgets (GTK_CHECK_BUTTON (widget), prefs);

	widget = e_builder_get_widget (prefs->builder, "timezone");
	g_settings_bind_with_mapping (
		settings, "timezone",
		widget, "timezone",
		G_SETTINGS_BIND_DEFAULT,
		calendar_preferences_map_string_to_icaltimezone,
		calendar_preferences_map_icaltimezone_to_string,
		NULL, (GDestroyNotify) NULL);
	g_settings_bind (
		settings, "use-system-timezone",
		widget, "sensitive",
		G_SETTINGS_BIND_DEFAULT |
		G_SETTINGS_BIND_INVERT_BOOLEAN);

	/* General tab */
	prefs->day_second_zone = e_builder_get_widget (prefs->builder, "day_second_zone");

	widget = e_builder_get_widget (prefs->builder, "sun_button");
	g_settings_bind (
		settings, "work-day-sunday",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->builder, "mon_button");
	g_settings_bind (
		settings, "work-day-monday",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->builder, "tue_button");
	g_settings_bind (
		settings, "work-day-tuesday",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->builder, "wed_button");
	g_settings_bind (
		settings, "work-day-wednesday",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->builder, "thu_button");
	g_settings_bind (
		settings, "work-day-thursday",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->builder, "fri_button");
	g_settings_bind (
		settings, "work-day-friday",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->builder, "sat_button");
	g_settings_bind (
		settings, "work-day-saturday",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->builder, "week_start_day");
	g_settings_bind (
		settings, "week-start-day-name",
		widget, "active-id",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->builder, "start_of_day");
	prefs->start_of_day = widget;  /* XXX delete this */
	if (locale_supports_12_hour_format)
		g_settings_bind (
			settings, "use-24hour-format",
			widget, "use-24-hour-format",
			G_SETTINGS_BIND_GET);

	widget = e_builder_get_widget (prefs->builder, "end_of_day");
	prefs->end_of_day = widget;  /* XXX delete this */
	if (locale_supports_12_hour_format)
		g_settings_bind (
			settings, "use-24hour-format",
			widget, "use-24-hour-format",
			G_SETTINGS_BIND_GET);

	widget = e_builder_get_widget (prefs->builder, "use_12_hour");
	gtk_widget_set_sensitive (widget, locale_supports_12_hour_format);
	g_settings_bind (
		settings, "use-24hour-format",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT |
		G_SETTINGS_BIND_INVERT_BOOLEAN);

	widget = e_builder_get_widget (prefs->builder, "use_24_hour");
	gtk_widget_set_sensitive (widget, locale_supports_12_hour_format);
	g_settings_bind (
		settings, "use-24hour-format",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->builder, "confirm_delete");
	g_settings_bind (
		settings, "confirm-delete",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->builder, "default_reminder");
	g_settings_bind (
		settings, "use-default-reminder",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->builder, "default_reminder_interval");
	g_settings_bind (
		settings, "default-reminder-interval",
		widget, "value",
		G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (
		settings, "use-default-reminder",
		widget, "sensitive",
		G_SETTINGS_BIND_GET);

	widget = e_builder_get_widget (prefs->builder, "default_reminder_units");
	g_settings_bind_with_mapping (
		settings, "default-reminder-units",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT,
		calendar_preferences_map_string_to_integer,
		calendar_preferences_map_integer_to_string,
		g_type_class_ref (E_TYPE_DURATION_TYPE),
		(GDestroyNotify) g_type_class_unref);
	g_settings_bind (
		settings, "use-default-reminder",
		widget, "sensitive",
		G_SETTINGS_BIND_GET);

	/* These settings control the "Birthdays & Anniversaries" backend. */

	eds_settings =
		e_util_ref_settings ("org.gnome.evolution-data-server.calendar");

	widget = e_builder_get_widget (prefs->builder, "ba_reminder");
	g_settings_bind (
		eds_settings, "contacts-reminder-enabled",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->builder, "ba_reminder_interval");
	g_settings_bind (
		eds_settings, "contacts-reminder-interval",
		widget, "value",
		G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (
		eds_settings, "contacts-reminder-enabled",
		widget, "sensitive",
		G_SETTINGS_BIND_GET);

	widget = e_builder_get_widget (prefs->builder, "ba_reminder_units");
	g_settings_bind_with_mapping (
		eds_settings, "contacts-reminder-units",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT,
		calendar_preferences_map_string_to_integer,
		calendar_preferences_map_integer_to_string,
		g_type_class_ref (E_TYPE_DURATION_TYPE),
		(GDestroyNotify) g_type_class_unref);
	g_settings_bind (
		eds_settings, "contacts-reminder-enabled",
		widget, "sensitive",
		G_SETTINGS_BIND_GET);

	g_object_unref (eds_settings);

	/* Display tab */
	widget = e_builder_get_widget (prefs->builder, "time_divisions");
	g_settings_bind_with_mapping (
		settings, "time-divisions",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT,
		calendar_preferences_map_time_divisions_to_index,
		calendar_preferences_map_index_to_time_divisions,
		NULL, (GDestroyNotify) NULL);

	widget = e_builder_get_widget (prefs->builder, "show_end_times");
	g_settings_bind (
		settings, "show-event-end",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->builder, "compress_weekend");
	g_settings_bind (
		settings, "compress-weekend",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->builder, "show_week_numbers");
	g_settings_bind (
		settings, "show-week-numbers",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->builder, "recur_events_italic");
	g_settings_bind (
		settings, "recur-events-italic",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->builder, "month_scroll_by_week");
	g_settings_bind (
		settings, "month-scroll-by-week",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->builder, "tasks_due_today_highlight");
	g_settings_bind (
		settings, "task-due-today-highlight",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->builder, "tasks_due_today_color");
	g_settings_bind_with_mapping (
		settings, "task-due-today-color",
		widget, "color",
		G_SETTINGS_BIND_DEFAULT,
		calendar_preferences_map_string_to_gdk_color,
		calendar_preferences_map_gdk_color_to_string,
		NULL, (GDestroyNotify) NULL);
	g_settings_bind (
		settings, "task-due-today-highlight",
		widget, "sensitive",
		G_SETTINGS_BIND_GET);

	widget = e_builder_get_widget (prefs->builder, "tasks_overdue_highlight");
	g_settings_bind (
		settings, "task-overdue-highlight",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->builder, "tasks_overdue_color");
	g_settings_bind_with_mapping (
		settings, "task-overdue-color",
		widget, "color",
		G_SETTINGS_BIND_DEFAULT,
		calendar_preferences_map_string_to_gdk_color,
		calendar_preferences_map_gdk_color_to_string,
		NULL, (GDestroyNotify) NULL);
	g_settings_bind (
		settings, "task-overdue-highlight",
		widget, "sensitive",
		G_SETTINGS_BIND_GET);

	widget = e_builder_get_widget (prefs->builder, "tasks_hide_completed");
	g_settings_bind (
		settings, "hide-completed-tasks",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->builder, "tasks_hide_completed_interval");
	g_settings_bind (
		settings, "hide-completed-tasks-value",
		widget, "value",
		G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (
		settings, "hide-completed-tasks",
		widget, "sensitive",
		G_SETTINGS_BIND_GET);

	widget = e_builder_get_widget (prefs->builder, "tasks_hide_completed_units");
	g_settings_bind_with_mapping (
		settings, "hide-completed-tasks-units",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT,
		calendar_preferences_map_string_to_integer,
		calendar_preferences_map_integer_to_string,
		g_type_class_ref (E_TYPE_DURATION_TYPE),
		(GDestroyNotify) g_type_class_unref);
	g_settings_bind (
		settings, "hide-completed-tasks",
		widget, "sensitive",
		G_SETTINGS_BIND_GET);

	/* Alarms tab */
	widget = e_builder_get_widget (prefs->builder, "notify_with_tray");
	g_settings_bind (
		settings, "notify-with-tray",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->builder, "default-snooze-minutes-spin");
	g_settings_bind (
		settings, "default-snooze-minutes",
		widget, "value",
		G_SETTINGS_BIND_DEFAULT);

	prefs->scrolled_window = e_builder_get_widget (prefs->builder, "calendar-source-scrolled-window");

	/* Free/Busy tab */
	widget = e_builder_get_widget (prefs->builder, "template_url");
	g_settings_bind (
		settings, "publish-template",
		widget, "text",
		G_SETTINGS_BIND_DEFAULT);

	/* date/time format */
	table = e_builder_get_widget (prefs->builder, "datetime_format_table");
	e_datetime_format_add_setup_widget (table, 0, "calendar", "table",  DTFormatKindDateTime, _("Ti_me and date:"));
	e_datetime_format_add_setup_widget (table, 1, "calendar", "table",  DTFormatKindDate, _("_Date only:"));

	/* Hook up and add the toplevel widget */

	target = e_cal_config_target_new_prefs (ec);
	e_config_set_target ((EConfig *) ec, (EConfigTarget *) target);
	toplevel = e_config_create_widget ((EConfig *) ec);
	gtk_box_pack_start (GTK_BOX (prefs), toplevel, TRUE, TRUE, 0);

	show_config (prefs);
	/* FIXME: weakref? */
	setup_changes (prefs);

	g_object_unref (settings);
}

void
e_calendar_preferences_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_calendar_preferences_register_type (type_module);
}

GtkWidget *
e_calendar_preferences_new (EPreferencesWindow *window)
{
	EShell *shell;
	ESourceRegistry *registry;
	ECalendarPreferences *preferences;

	shell = e_preferences_window_get_shell (window);

	registry = e_shell_get_registry (shell);

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	preferences = g_object_new (E_TYPE_CALENDAR_PREFERENCES, NULL);

	preferences->registry = g_object_ref (registry);

	/* FIXME Kill this function. */
	calendar_preferences_construct (preferences, shell);

	return GTK_WIDGET (preferences);
}
