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

#include "evolution-config.h"

#include "e-calendar-preferences.h"

#include <string.h>
#include <glib/gi18n.h>

#include "calendar/gui/e-cal-config.h"
#include "calendar/gui/e-timezone-entry.h"
#include "calendar/gui/e-to-do-pane.h"
#include "calendar/gui/calendar-config.h"
#include "shell/e-shell-utils.h"

struct _ECalendarPreferencesPrivate {
	GtkBuilder *builder;
	ESourceRegistry *registry;

	/* General tab */
	GtkWidget *day_second_zone;
	GtkWidget *start_of_day;
	GtkWidget *end_of_day;

	/* Reminders tab */
	GtkWidget *reminder_calendars_scrolled_window;
	GtkWidget *reminder_tasks_scrolled_window;
};

G_DEFINE_DYNAMIC_TYPE_EXTENDED (ECalendarPreferences, e_calendar_preferences, GTK_TYPE_BOX, 0,
	G_ADD_PRIVATE_DYNAMIC (ECalendarPreferences))

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
	ICalTimezone *timezone = NULL;

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");

	if (g_settings_get_boolean (settings, "use-system-timezone"))
		timezone = e_cal_util_get_system_timezone ();
	else
		location = g_variant_get_string (variant, NULL);

	if (location != NULL && *location != '\0')
		timezone = i_cal_timezone_get_builtin_timezone (location);

	if (timezone == NULL)
		timezone = i_cal_timezone_get_utc_timezone ();

	g_value_set_object (value, timezone);

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
	ICalTimezone *timezone;

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");

	if (g_settings_get_boolean (settings, "use-system-timezone")) {
		location_str = g_settings_get_string (settings, "timezone");
		location = location_str;
	} else {
		timezone = g_value_get_object (value);

		if (timezone != NULL)
			location = i_cal_timezone_get_location (timezone);
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
calendar_preferences_map_string_to_gdk_rgba (GValue *value,
                                             GVariant *variant,
                                             gpointer user_data)
{
	GdkRGBA rgba;
	const gchar *string;

	string = g_variant_get_string (variant, NULL);
	if (gdk_rgba_parse (&rgba, string)) {
		g_value_set_boxed (value, &rgba);
		return TRUE;
	}

	return FALSE;
}

static GVariant *
calendar_preferences_map_gdk_rgba_to_string (const GValue *value,
                                             const GVariantType *expected_type,
                                             gpointer user_data)
{
	GVariant *variant;
	const GdkRGBA *color;

	color = g_value_get_boxed (value);
	if (color == NULL) {
		variant = g_variant_new_string ("");
	} else {
		gchar *string;

		string = gdk_rgba_to_string (color);
		variant = g_variant_new_string (string);
		g_free (string);
	}

	return variant;
}

static gboolean
calendar_preferences_shorten_time_kind_to_object_cb (GValue *value,
						     GVariant *variant,
						     gpointer user_data)
{
	gboolean shorten_time_end;

	shorten_time_end = g_variant_get_boolean (variant);

	g_value_set_string (value, shorten_time_end ? "end" : "start");

	return TRUE;
}

static GVariant *
calendar_preferences_shorten_time_kind_to_settings_cb (const GValue *value,
						       const GVariantType *expected_type,
						       gpointer user_data)
{
	const gchar *string;

	string = g_value_get_string (value);

	return g_variant_new_boolean (g_strcmp0 (string, "end") == 0);
}

static void
calendar_preferences_dispose (GObject *object)
{
	ECalendarPreferences *prefs = E_CALENDAR_PREFERENCES (object);

	g_clear_object (&prefs->priv->builder);
	g_clear_object (&prefs->priv->registry);

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
	preferences->priv = e_calendar_preferences_get_instance_private (preferences);

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

	return e_builder_get_widget (preferences->priv->builder, item->label);
}

static void
update_day_second_zone_caption (ECalendarPreferences *prefs)
{
	gchar *location;
	const gchar *caption;
	ICalTimezone *zone;

	g_return_if_fail (prefs != NULL);

	/* Translators: "None" indicates no second time zone set for a day view */
	caption = C_("cal-second-zone", "None");

	location = calendar_config_get_day_second_zone ();
	if (location && *location) {
		zone = i_cal_timezone_get_builtin_timezone (location);
		if (zone && i_cal_timezone_get_display_name (zone)) {
			caption = i_cal_timezone_get_display_name (zone);
		}
	}
	g_free (location);

	gtk_button_set_label (GTK_BUTTON (prefs->priv->day_second_zone), caption);
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

	calendar_config_select_day_second_zone (gtk_widget_get_toplevel (item));
	update_day_second_zone_caption (prefs);
}

static void
day_second_zone_clicked (GtkWidget *widget,
                         ECalendarPreferences *prefs)
{
	GtkWidget *menu, *item;
	GSList *group = NULL, *recent_zones, *s;
	gchar *location;
	ICalTimezone *zone, *second_zone = NULL;

	menu = gtk_menu_new ();

	location = calendar_config_get_day_second_zone ();
	if (location && *location)
		second_zone = i_cal_timezone_get_builtin_timezone (location);
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
		zone = i_cal_timezone_get_builtin_timezone (s->data);
		if (!zone)
			continue;

		item = gtk_radio_menu_item_new_with_label (group, i_cal_timezone_get_display_name (zone));
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

	item = gtk_menu_item_new_with_label (_("Select…"));
	g_signal_connect (
		item, "activate",
		G_CALLBACK (on_select_day_second_zone), prefs);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

	gtk_widget_show_all (menu);

	gtk_menu_attach_to_widget (GTK_MENU (menu), widget, NULL);
	g_signal_connect (menu, "deactivate", G_CALLBACK (gtk_menu_detach), NULL);
	gtk_menu_popup_at_pointer (GTK_MENU (menu), NULL);
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

	start = E_DATE_EDIT (prefs->priv->start_of_day);
	end = E_DATE_EDIT (prefs->priv->end_of_day);

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

	start = E_DATE_EDIT (prefs->priv->start_of_day);
	end = E_DATE_EDIT (prefs->priv->end_of_day);

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
	ICalTimezone *zone;
	const gchar *display_name;
	gchar *text;

	widget = e_builder_get_widget (prefs->priv->builder, "use-system-tz-check");
	g_return_if_fail (GTK_IS_CHECK_BUTTON (widget));

	zone = e_cal_util_get_system_timezone ();
	if (zone != NULL)
		display_name = gettext (i_cal_timezone_get_display_name (zone));
	else
		display_name = "UTC";

	/* Translators: The '%s' is replaced with the time zone name, like "America/New York" or "UTC" */
	text = g_strdup_printf (_("Use s_ystem time zone (%s)"), display_name);
	gtk_button_set_label (GTK_BUTTON (widget), text);
	g_free (text);
}

static void
setup_changes (ECalendarPreferences *prefs)
{
	g_signal_connect (
		prefs->priv->day_second_zone, "clicked",
		G_CALLBACK (day_second_zone_clicked), prefs);

	g_signal_connect (
		prefs->priv->start_of_day, "changed",
		G_CALLBACK (start_of_day_changed), prefs);

	g_signal_connect (
		prefs->priv->end_of_day, "changed",
		G_CALLBACK (end_of_day_changed), prefs);
}

static void
show_alarms_config (ECalendarPreferences *prefs)
{
	GtkWidget *widget;

	widget = e_alarm_selector_new (prefs->priv->registry, E_SOURCE_EXTENSION_CALENDAR);
	atk_object_set_name (
		gtk_widget_get_accessible (widget),
		_("Selected Calendars for Notifications of Reminders"));
	gtk_container_add (GTK_CONTAINER (prefs->priv->reminder_calendars_scrolled_window), widget);
	gtk_widget_show (widget);

	widget = e_alarm_selector_new (prefs->priv->registry, E_SOURCE_EXTENSION_TASK_LIST);
	atk_object_set_name (
		gtk_widget_get_accessible (widget),
		_("Selected Task Lists for Notifications of Reminders"));
	gtk_container_add (GTK_CONTAINER (prefs->priv->reminder_tasks_scrolled_window), widget);
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
		E_DATE_EDIT (prefs->priv->start_of_day),
		g_settings_get_int (settings, "day-start-hour"),
		g_settings_get_int (settings, "day-start-minute"));

	/* End of Day. */
	e_date_edit_set_time_of_day (
		E_DATE_EDIT (prefs->priv->end_of_day),
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
	{ E_CONFIG_SECTION_GRID,  (gchar *) "00.general/00.time",           (gchar *) "time",              calendar_preferences_get_config_widget },
	{ E_CONFIG_SECTION_GRID,  (gchar *) "00.general/10.workWeek",       (gchar *) "workWeek",          calendar_preferences_get_config_widget },
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
calendar_preferences_add_itip_formatter_page (EShell *shell,
					      GtkWidget *notebook)
{
	ESourceRegistry *registry;
	GtkWidget *page;
	GtkWidget *tab_label;
	GtkWidget *frame;
	GtkWidget *frame_label;
	GtkWidget *padding_label;
	GtkWidget *hbox;
	GtkWidget *inner_vbox;
	GtkWidget *check;
	GtkWidget *check_delete_on_decline;
	GtkWidget *label;
	GtkWidget *ess;
	GtkWidget *scrolledwin;
	gchar *str;
	GSettings *settings;

	g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

	registry = e_shell_get_registry (shell);

	/* Create a new notebook page */
	page = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_set_border_width (GTK_CONTAINER (page), 12);
	tab_label = gtk_label_new (_("Meeting Invitations"));
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), page, tab_label);

	/* Frame */
	frame = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_box_pack_start (GTK_BOX (page), frame, FALSE, FALSE, 0);

	/* "General" */
	frame_label = gtk_label_new ("");
	str = g_strdup_printf ("<span weight=\"bold\">%s</span>", _("General"));
	gtk_label_set_markup (GTK_LABEL (frame_label), str);
	g_free (str);
	gtk_label_set_xalign (GTK_LABEL (frame_label), 0);
	gtk_box_pack_start (GTK_BOX (frame), frame_label, FALSE, FALSE, 0);

	/* Indent/padding */
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_box_pack_start (GTK_BOX (frame), hbox, FALSE, TRUE, 0);
	padding_label = gtk_label_new ("");
	gtk_box_pack_start (GTK_BOX (hbox), padding_label, FALSE, FALSE, 0);
	inner_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_box_pack_start (GTK_BOX (hbox), inner_vbox, FALSE, FALSE, 0);

	/* Delete message after acting */
	settings = e_util_ref_settings ("org.gnome.evolution.plugin.itip");

	check = gtk_check_button_new_with_mnemonic (_("_Delete message after acting"));
	gtk_box_pack_start (GTK_BOX (inner_vbox), check, FALSE, FALSE, 0);

	g_settings_bind (settings, "delete-processed",
		check, "active",
		G_SETTINGS_BIND_DEFAULT);

	check_delete_on_decline = gtk_check_button_new_with_mnemonic (_("Delete _meeting from calendar on Decline"));
	gtk_box_pack_start (GTK_BOX (inner_vbox), check_delete_on_decline, FALSE, FALSE, 0);

	check = gtk_check_button_new_with_mnemonic (_("_Preserve existing reminder by default"));
	gtk_box_pack_start (GTK_BOX (inner_vbox), check, FALSE, FALSE, 0);

	g_settings_bind (settings, "preserve-reminder",
		check, "active",
		G_SETTINGS_BIND_DEFAULT);

	check = gtk_check_button_new_with_mnemonic (_("_Show invitation description provided by the sender"));
	gtk_box_pack_start (GTK_BOX (inner_vbox), check, FALSE, FALSE, 0);

	g_settings_bind (settings, "show-message-description",
		check, "active",
		G_SETTINGS_BIND_DEFAULT);

	check = gtk_check_button_new_with_mnemonic (_("Sh_ow day agenda at meeting start time"));
	gtk_box_pack_start (GTK_BOX (inner_vbox), check, FALSE, FALSE, 0);

	g_settings_bind (settings, "show-day-agenda",
		check, "active",
		G_SETTINGS_BIND_DEFAULT);

	check = gtk_check_button_new_with_mnemonic (_("_Always attach components in mail messages"));
	gtk_box_pack_start (GTK_BOX (inner_vbox), check, FALSE, FALSE, 0);

	g_settings_bind (settings, "attach-components",
		check, "active",
		G_SETTINGS_BIND_DEFAULT);

	g_object_unref (settings);

	settings = e_util_ref_settings ("org.gnome.evolution-data-server.calendar");

	g_settings_bind (settings, "delete-meeting-on-decline",
		check_delete_on_decline, "active",
		G_SETTINGS_BIND_DEFAULT);

	g_object_unref (settings);

	/* "Conflict searching" */
	frame = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_box_pack_start (GTK_BOX (page), frame, TRUE, TRUE, 24);

	frame_label = gtk_label_new ("");
	str = g_strdup_printf ("<span weight=\"bold\">%s</span>", _("Search for Meetings"));
	gtk_label_set_markup (GTK_LABEL (frame_label), str);
	g_free (str);
	gtk_label_set_xalign (GTK_LABEL (frame_label), 0);
	gtk_box_pack_start (GTK_BOX (frame), frame_label, FALSE, FALSE, 0);

	/* Indent/padding */
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_box_pack_start (GTK_BOX (frame), hbox, TRUE, TRUE, 0);
	padding_label = gtk_label_new ("");
	gtk_box_pack_start (GTK_BOX (hbox), padding_label, FALSE, FALSE, 0);
	inner_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_box_pack_start (GTK_BOX (hbox), inner_vbox, TRUE, TRUE, 0);

	/* Source selector */
	label = gtk_label_new (_("Select the calendars to search for the meetings and the conflicts"));
	gtk_label_set_xalign (GTK_LABEL (label), 0);
	gtk_box_pack_start (GTK_BOX (inner_vbox), label, FALSE, FALSE, 0);

	scrolledwin = gtk_scrolled_window_new (NULL, NULL);

	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (scrolledwin),
		GTK_POLICY_AUTOMATIC,
		GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (scrolledwin),
		GTK_SHADOW_IN);
	gtk_box_pack_start (GTK_BOX (inner_vbox), scrolledwin, TRUE, TRUE, 0);

	ess = e_conflict_search_selector_new (registry);
	atk_object_set_name (gtk_widget_get_accessible (ess), _("Search for Meetings"));
	gtk_container_add (GTK_CONTAINER (scrolledwin), ess);

	gtk_widget_show_all (page);
}

static void
calendar_preferences_construct (ECalendarPreferences *prefs,
                                EShell *shell)
{
	ECalConfig *ec;
	ECalConfigTargetPrefs *target;
	GSettings *settings;
	GSettings *eds_settings;
	GSettings *mail_settings;
	GSettings *eds_calendar_settings;
	gboolean locale_supports_12_hour_format;
	gint i;
	GObject *object;
	GtkWidget *toplevel;
	GtkWidget *widget;
	GtkGrid *grid;
	GSList *l;

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");
	mail_settings = e_util_ref_settings ("org.gnome.evolution.mail");
	eds_calendar_settings = e_util_ref_settings ("org.gnome.evolution-data-server.calendar");

	locale_supports_12_hour_format =
		calendar_config_locale_supports_12_hour_format ();

	/* Make sure our custom widget classes are registered with
	 * GType before we load the GtkBuilder definition file. */
	g_type_ensure (E_TYPE_DATE_EDIT);
	g_type_ensure (E_TYPE_TIMEZONE_ENTRY);

	prefs->priv->builder = gtk_builder_new ();
	e_load_ui_builder_definition (prefs->priv->builder, "e-calendar-preferences.ui");

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

	calendar_preferences_add_itip_formatter_page (shell, e_builder_get_widget (prefs->priv->builder, "toplevel-notebook"));

	widget = e_builder_get_widget (prefs->priv->builder, "use-system-tz-check");
	g_settings_bind (
		settings, "use-system-timezone",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);
	g_signal_connect (
		widget, "toggled",
		G_CALLBACK (update_system_tz_widgets), prefs);
	update_system_tz_widgets (GTK_CHECK_BUTTON (widget), prefs);

	widget = e_builder_get_widget (prefs->priv->builder, "timezone");
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
	prefs->priv->day_second_zone = e_builder_get_widget (prefs->priv->builder, "day_second_zone");

	widget = e_builder_get_widget (prefs->priv->builder, "sun_button");
	g_settings_bind (
		settings, "work-day-sunday",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->priv->builder, "mon_button");
	g_settings_bind (
		settings, "work-day-monday",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->priv->builder, "tue_button");
	g_settings_bind (
		settings, "work-day-tuesday",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->priv->builder, "wed_button");
	g_settings_bind (
		settings, "work-day-wednesday",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->priv->builder, "thu_button");
	g_settings_bind (
		settings, "work-day-thursday",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->priv->builder, "fri_button");
	g_settings_bind (
		settings, "work-day-friday",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->priv->builder, "sat_button");
	g_settings_bind (
		settings, "work-day-saturday",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->priv->builder, "week_start_day");
	g_settings_bind (
		settings, "week-start-day-name",
		widget, "active-id",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->priv->builder, "start_of_day");
	prefs->priv->start_of_day = widget;  /* XXX delete this */
	if (locale_supports_12_hour_format)
		g_settings_bind (
			settings, "use-24hour-format",
			widget, "use-24-hour-format",
			G_SETTINGS_BIND_GET);

	widget = e_builder_get_widget (prefs->priv->builder, "end_of_day");
	prefs->priv->end_of_day = widget;  /* XXX delete this */
	if (locale_supports_12_hour_format)
		g_settings_bind (
			settings, "use-24hour-format",
			widget, "use-24-hour-format",
			G_SETTINGS_BIND_GET);

	widget = e_builder_get_widget (prefs->priv->builder, "hbox_time_format");
	gtk_widget_set_sensitive (widget, locale_supports_12_hour_format);

	widget = e_builder_get_widget (prefs->priv->builder, "use_12_hour");
	g_settings_bind (
		settings, "use-24hour-format",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT |
		G_SETTINGS_BIND_INVERT_BOOLEAN);

	widget = e_builder_get_widget (prefs->priv->builder, "use_24_hour");
	g_settings_bind (
		settings, "use-24hour-format",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->priv->builder, "shorten_time_interval");
	g_settings_bind (
		settings, "shorten-time",
		widget, "value",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->priv->builder, "shorten_time_kind");
	g_settings_bind_with_mapping (
		settings, "shorten-time-end",
		widget, "active-id",
		G_SETTINGS_BIND_DEFAULT,
		calendar_preferences_shorten_time_kind_to_object_cb,
		calendar_preferences_shorten_time_kind_to_settings_cb,
		NULL, NULL);

	widget = e_builder_get_widget (prefs->priv->builder, "confirm_delete");
	g_settings_bind (
		settings, "confirm-delete",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->priv->builder, "default_reminder");
	g_settings_bind (
		settings, "use-default-reminder",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->priv->builder, "default_reminder_interval");
	g_settings_bind (
		settings, "default-reminder-interval",
		widget, "value",
		G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (
		settings, "use-default-reminder",
		widget, "sensitive",
		G_SETTINGS_BIND_GET);

	widget = e_builder_get_widget (prefs->priv->builder, "default_reminder_units");
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

	widget = e_builder_get_widget (prefs->priv->builder, "classify-private");
	g_settings_bind (
		settings, "classify-private",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->priv->builder, "use-markdown-editor");
	g_settings_bind (
		settings, "use-markdown-editor",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	/* These settings control the "Birthdays & Anniversaries" backend. */

	eds_settings =
		e_util_ref_settings ("org.gnome.evolution-data-server.calendar");

	widget = e_builder_get_widget (prefs->priv->builder, "ba_reminder");
	g_settings_bind (
		eds_settings, "contacts-reminder-enabled",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->priv->builder, "ba_reminder_interval");
	g_settings_bind (
		eds_settings, "contacts-reminder-interval",
		widget, "value",
		G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (
		eds_settings, "contacts-reminder-enabled",
		widget, "sensitive",
		G_SETTINGS_BIND_GET);

	widget = e_builder_get_widget (prefs->priv->builder, "ba_reminder_units");
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

	widget = e_builder_get_widget (prefs->priv->builder, "defall_reminder");
	g_settings_bind (
		eds_settings, "defall-reminder-enabled",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->priv->builder, "defall_reminder_interval");
	g_settings_bind (
		eds_settings, "defall-reminder-interval",
		widget, "value",
		G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (
		eds_settings, "defall-reminder-enabled",
		widget, "sensitive",
		G_SETTINGS_BIND_GET);

	widget = e_builder_get_widget (prefs->priv->builder, "defall_reminder_units");
	g_settings_bind_with_mapping (
		eds_settings, "defall-reminder-units",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT,
		calendar_preferences_map_string_to_integer,
		calendar_preferences_map_integer_to_string,
		g_type_class_ref (E_TYPE_DURATION_TYPE),
		(GDestroyNotify) g_type_class_unref);
	g_settings_bind (
		eds_settings, "defall-reminder-enabled",
		widget, "sensitive",
		G_SETTINGS_BIND_GET);

	g_object_unref (eds_settings);

	/* Display tab */
	widget = e_builder_get_widget (prefs->priv->builder, "time_divisions");
	g_settings_bind_with_mapping (
		settings, "time-divisions",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT,
		calendar_preferences_map_time_divisions_to_index,
		calendar_preferences_map_index_to_time_divisions,
		NULL, (GDestroyNotify) NULL);

	widget = e_builder_get_widget (prefs->priv->builder, "show_end_times");
	g_settings_bind (
		settings, "show-event-end",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->priv->builder, "show_icons_month_view");
	g_settings_bind (
		settings, "show-icons-month-view",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->priv->builder, "compress_weekend");
	g_settings_bind (
		settings, "compress-weekend",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->priv->builder, "show_week_numbers");
	g_settings_bind (
		settings, "show-week-numbers",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->priv->builder, "recur_events_italic");
	g_settings_bind (
		settings, "recur-events-italic",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->priv->builder, "month_scroll_by_week");
	g_settings_bind (
		settings, "month-scroll-by-week",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget(prefs->priv->builder, "month_start_with_current_week");
	g_settings_bind(
		settings, "month-start-with-current-week",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->priv->builder, "week_view_days_left_to_right");
	g_settings_bind (
		settings, "week-view-days-left-to-right",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->priv->builder, "allow_direct_summary_edit");
	g_settings_bind (
		settings, "allow-direct-summary-edit",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->priv->builder, "allow_event_dnd");
	g_settings_bind (
		settings, "allow-event-dnd",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->priv->builder, "hide_cancelled_events");
	g_settings_bind (
		settings, "hide-cancelled-events",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->priv->builder, "tasks_due_today_highlight");
	g_settings_bind (
		settings, "task-due-today-highlight",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->priv->builder, "tasks_due_today_color");
	g_settings_bind_with_mapping (
		settings, "task-due-today-color",
		widget, "rgba",
		G_SETTINGS_BIND_DEFAULT,
		calendar_preferences_map_string_to_gdk_rgba,
		calendar_preferences_map_gdk_rgba_to_string,
		NULL, (GDestroyNotify) NULL);
	g_settings_bind (
		settings, "task-due-today-highlight",
		widget, "sensitive",
		G_SETTINGS_BIND_GET);

	widget = e_builder_get_widget (prefs->priv->builder, "tasks_overdue_highlight");
	g_settings_bind (
		settings, "task-overdue-highlight",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->priv->builder, "tasks_overdue_color");
	g_settings_bind_with_mapping (
		settings, "task-overdue-color",
		widget, "rgba",
		G_SETTINGS_BIND_DEFAULT,
		calendar_preferences_map_string_to_gdk_rgba,
		calendar_preferences_map_gdk_rgba_to_string,
		NULL, (GDestroyNotify) NULL);
	g_settings_bind (
		settings, "task-overdue-highlight",
		widget, "sensitive",
		G_SETTINGS_BIND_GET);

	widget = e_builder_get_widget (prefs->priv->builder, "tasks_hide_completed");
	g_settings_bind (
		settings, "hide-completed-tasks",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->priv->builder, "tasks_hide_completed_interval");
	g_settings_bind (
		settings, "hide-completed-tasks-value",
		widget, "value",
		G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (
		settings, "hide-completed-tasks",
		widget, "sensitive",
		G_SETTINGS_BIND_GET);

	widget = e_builder_get_widget (prefs->priv->builder, "tasks_hide_completed_units");
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

	widget = e_builder_get_widget (prefs->priv->builder, "tasks_hide_cancelled");
	g_settings_bind (
		settings, "hide-cancelled-tasks",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->priv->builder, "to_do_bar_show_no_duedate_tasks");
	g_settings_bind (
		mail_settings, "to-do-bar-show-no-duedate-tasks",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->priv->builder, "to_do_bar_time_in_smaller_font");
	g_settings_bind (
		mail_settings, "to-do-bar-time-in-smaller-font",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	object = gtk_builder_get_object (prefs->priv->builder, "tdbndaysadjustment");
	gtk_adjustment_set_lower (GTK_ADJUSTMENT (object), E_TO_DO_PANE_MIN_SHOW_N_DAYS);
	gtk_adjustment_set_upper (GTK_ADJUSTMENT (object), E_TO_DO_PANE_MAX_SHOW_N_DAYS);

	widget = e_builder_get_widget (prefs->priv->builder, "to_do_bar_show_n_days");
	g_settings_bind (
		mail_settings, "to-do-bar-show-n-days",
		widget, "value",
		G_SETTINGS_BIND_DEFAULT);

	/* Alarms tab */
	widget = e_builder_get_widget (prefs->priv->builder, "notify_with_tray");
	g_settings_bind (
		eds_calendar_settings, "notify-with-tray",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_INVERT_BOOLEAN);

	widget = e_builder_get_widget (prefs->priv->builder, "notify_window_on_top");
	g_settings_bind (
		eds_calendar_settings, "notify-window-on-top",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->priv->builder, "notify_enable_display");
	g_settings_bind (
		eds_calendar_settings, "notify-enable-display",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->priv->builder, "notify_enable_audio");
	g_settings_bind (
		eds_calendar_settings, "notify-enable-audio",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->priv->builder, "task_reminder_for_completed");
	g_settings_bind (
		eds_calendar_settings, "notify-completed-tasks",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	widget = e_builder_get_widget (prefs->priv->builder, "allow-past-reminders");
	g_settings_bind (
		eds_calendar_settings, "notify-past-events",
		widget, "active",
		G_SETTINGS_BIND_DEFAULT);

	prefs->priv->reminder_calendars_scrolled_window = e_builder_get_widget (prefs->priv->builder, "reminder-calendars-scrolled-window");
	prefs->priv->reminder_tasks_scrolled_window = e_builder_get_widget (prefs->priv->builder, "reminder-tasks-scrolled-window");

	/* Free/Busy tab */
	widget = e_builder_get_widget (prefs->priv->builder, "template_url");
	g_settings_bind (
		settings, "publish-template",
		widget, "text",
		G_SETTINGS_BIND_DEFAULT);

	/* date/time format */
	grid = GTK_GRID (e_builder_get_widget (prefs->priv->builder, "datetime_format_grid"));
	e_datetime_format_add_setup_widget (grid, 0, "calendar", "table",  DTFormatKindDateTime, _("Ti_me and date:"));
	e_datetime_format_add_setup_widget (grid, 1, "calendar", "table",  DTFormatKindDate, _("_Date only:"));

	/* Hook up and add the toplevel widget */

	target = e_cal_config_target_new_prefs (ec);
	e_config_set_target ((EConfig *) ec, (EConfigTarget *) target);
	toplevel = e_config_create_widget ((EConfig *) ec);
	gtk_box_pack_start (GTK_BOX (prefs), toplevel, TRUE, TRUE, 0);

	show_config (prefs);
	/* FIXME: weakref? */
	setup_changes (prefs);

	g_object_unref (eds_calendar_settings);
	g_object_unref (mail_settings);
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

	preferences->priv->registry = g_object_ref (registry);

	/* FIXME Kill this function. */
	calendar_preferences_construct (preferences, shell);

	return GTK_WIDGET (preferences);
}
