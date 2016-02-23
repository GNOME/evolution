/*
 * Copyright (C) 2015 Red Hat, Inc. (www.redhat.com)
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include <e-util/e-util.h>

#include "calendar-config.h"
#include "comp-util.h"
#include "e-comp-editor.h"
#include "e-comp-editor-page.h"
#include "e-comp-editor-page-attachments.h"
#include "e-comp-editor-page-general.h"
#include "e-comp-editor-page-recurrence.h"
#include "e-comp-editor-page-reminders.h"
#include "e-comp-editor-page-schedule.h"
#include "e-comp-editor-property-parts.h"
#include "e-timezone-entry.h"

#include "e-comp-editor-event.h"

struct _ECompEditorEventPrivate {
	ECompEditorPage *page_general;
	ECompEditorPropertyPart *dtstart;
	ECompEditorPropertyPart *dtend;
	ECompEditorPropertyPart *categories;
	ECompEditorPropertyPart *timezone;
	ECompEditorPropertyPart *transparency;
	GtkWidget *all_day_check;

	gpointer in_the_past_alert;
	gpointer insensitive_info_alert;
};

G_DEFINE_TYPE (ECompEditorEvent, e_comp_editor_event, E_TYPE_COMP_EDITOR)

static void
ece_event_action_classification_cb (GtkRadioAction *action,
				    GtkRadioAction *current,
				    ECompEditorEvent *event_editor)
{
	g_return_if_fail (E_IS_COMP_EDITOR_EVENT (event_editor));

	e_comp_editor_set_changed (E_COMP_EDITOR (event_editor), TRUE);
}

static void
ece_event_update_times (ECompEditorEvent *event_editor,
			EDateEdit *date_edit,
			gboolean change_end_datetime)
{
	GtkWidget *widget;
	guint flags;

	g_return_if_fail (E_IS_COMP_EDITOR_EVENT (event_editor));
	g_return_if_fail (E_IS_DATE_EDIT (date_edit));

	widget = e_date_edit_get_entry (date_edit);
	if (widget && gtk_widget_has_focus (widget))
		return;

	if (!e_comp_editor_get_updating (E_COMP_EDITOR (event_editor))) {
		e_comp_editor_ensure_start_before_end (E_COMP_EDITOR (event_editor),
			event_editor->priv->dtstart,
			event_editor->priv->dtend,
			change_end_datetime);
	}

	flags = e_comp_editor_get_flags (E_COMP_EDITOR (event_editor));

	if ((flags & E_COMP_EDITOR_FLAG_IS_NEW) != 0) {
		struct icaltimetype start_tt;

		start_tt = e_comp_editor_property_part_datetime_get_value (
			E_COMP_EDITOR_PROPERTY_PART_DATETIME (event_editor->priv->dtstart));

		if (cal_comp_util_compare_time_with_today (start_tt) < 0) {
			if (!event_editor->priv->in_the_past_alert) {
				EAlert *alert;

				alert = e_comp_editor_add_warning (E_COMP_EDITOR (event_editor),
					_("Event's time is in the past"), NULL);

				event_editor->priv->in_the_past_alert = alert;

				if (alert)
					g_object_add_weak_pointer (G_OBJECT (alert), &event_editor->priv->in_the_past_alert);

				g_clear_object (&alert);
			}
		} else if (event_editor->priv->in_the_past_alert) {
			e_alert_response (event_editor->priv->in_the_past_alert, GTK_RESPONSE_OK);
		}
	}
}

static void
ece_event_dtstart_changed_cb (EDateEdit *date_edit,
			      ECompEditorEvent *event_editor)
{
	g_return_if_fail (E_IS_DATE_EDIT (date_edit));
	g_return_if_fail (E_IS_COMP_EDITOR_EVENT (event_editor));

	ece_event_update_times (event_editor, date_edit, TRUE);
}

static void
ece_event_dtend_changed_cb (EDateEdit *date_edit,
			    ECompEditorEvent *event_editor)
{
	g_return_if_fail (E_IS_DATE_EDIT (date_edit));
	g_return_if_fail (E_IS_COMP_EDITOR_EVENT (event_editor));

	ece_event_update_times (event_editor, date_edit, FALSE);
}

static void
ece_event_all_day_toggled_cb (ECompEditorEvent *event_editor)
{
	GtkWidget *edit_widget;

	g_return_if_fail (E_IS_COMP_EDITOR_EVENT (event_editor));

	edit_widget = e_comp_editor_property_part_get_edit_widget (event_editor->priv->dtstart);

	ece_event_update_times (event_editor, E_DATE_EDIT (edit_widget), TRUE);

	e_comp_editor_ensure_changed (E_COMP_EDITOR (event_editor));
}

static void
ece_event_sensitize_widgets (ECompEditor *comp_editor,
			     gboolean force_insensitive)
{
	ECompEditorEvent *event_editor;
	gboolean is_organizer;
	GtkAction *action;
	guint32 flags;

	g_return_if_fail (E_IS_COMP_EDITOR_EVENT (comp_editor));

	E_COMP_EDITOR_CLASS (e_comp_editor_event_parent_class)->sensitize_widgets (comp_editor, force_insensitive);

	flags = e_comp_editor_get_flags (comp_editor);
	is_organizer = (flags & (E_COMP_EDITOR_FLAG_IS_NEW | E_COMP_EDITOR_FLAG_ORGANIZER_IS_USER)) != 0;
	event_editor = E_COMP_EDITOR_EVENT (comp_editor);

	gtk_widget_set_sensitive (event_editor->priv->all_day_check, !force_insensitive && is_organizer);

	#define sensitize_part(x) G_STMT_START { \
		GtkWidget *widget; \
		\
		widget = e_comp_editor_property_part_get_label_widget (x); \
		if (widget) \
			gtk_widget_set_sensitive (widget, !force_insensitive && is_organizer); \
		\
		widget = e_comp_editor_property_part_get_edit_widget (x); \
		if (widget) \
			gtk_widget_set_sensitive (widget, !force_insensitive && is_organizer); \
	} G_STMT_END

	sensitize_part (event_editor->priv->dtstart);
	sensitize_part (event_editor->priv->dtend);
	sensitize_part (event_editor->priv->timezone);

	#undef sensitize_part

	action = e_comp_editor_get_action (comp_editor, "all-day-event");
	gtk_action_set_sensitive (action, !force_insensitive && is_organizer);

	action = e_comp_editor_get_action (comp_editor, "classification-menu");
	gtk_action_set_sensitive (action, !force_insensitive && is_organizer);

	if (event_editor->priv->insensitive_info_alert)
		e_alert_response (event_editor->priv->insensitive_info_alert, GTK_RESPONSE_OK);

	if (force_insensitive || !is_organizer) {
		ECalClient *client;
		const gchar *message = NULL;

		client = e_comp_editor_get_target_client (comp_editor);
		if (!client)
			message = _("Event cannot be edited, because the selected calendar could not be opened");
		else if (e_client_is_readonly (E_CLIENT (client)))
			message = _("Event cannot be edited, because the selected calendar is read only");
		else if (!is_organizer)
			message = _("Event cannot be fully edited, because you are not the organizer");

		if (message) {
			EAlert *alert;

			alert = e_comp_editor_add_information (comp_editor, message, NULL);

			event_editor->priv->insensitive_info_alert = alert;

			if (alert)
				g_object_add_weak_pointer (G_OBJECT (alert), &event_editor->priv->insensitive_info_alert);

			g_clear_object (&alert);
		}
	}
}

static void
ece_event_fill_widgets (ECompEditor *comp_editor,
			icalcomponent *component)
{
	ECompEditorEvent *event_editor;
	struct icaltimetype dtstart, dtend;
	icalproperty *prop;
	icaltimezone *zone = NULL;
	gboolean all_day_event = FALSE;
	GtkAction *action;
	guint32 flags;

	g_return_if_fail (E_IS_COMP_EDITOR_EVENT (comp_editor));
	g_return_if_fail (component != NULL);

	E_COMP_EDITOR_CLASS (e_comp_editor_event_parent_class)->fill_widgets (comp_editor, component);

	event_editor = E_COMP_EDITOR_EVENT (comp_editor);

	flags = e_comp_editor_get_flags (comp_editor);
	dtstart = icaltime_null_time ();
	dtend = icaltime_null_time ();

	if (icalcomponent_get_first_property (component, ICAL_DTSTART_PROPERTY)) {
		dtstart = icalcomponent_get_dtstart (component);
		if (icaltime_is_valid_time (dtstart))
			zone = (icaltimezone *) dtstart.zone;
	}

	if (icalcomponent_get_first_property (component, ICAL_DTEND_PROPERTY)) {
		dtend = icalcomponent_get_dtend (component);
		if (!zone && icaltime_is_valid_time (dtend))
			zone = (icaltimezone *) dtend.zone;
	}

	if (!zone) {
		struct icaltimetype itt;

		itt = icalcomponent_get_due (component);
		if (icaltime_is_valid_time (itt))
			zone = (icaltimezone *) itt.zone;
	}

	if (zone) {
		GtkWidget *edit_widget;

		edit_widget = e_comp_editor_property_part_get_edit_widget (event_editor->priv->timezone);

		e_timezone_entry_set_timezone (E_TIMEZONE_ENTRY (edit_widget), zone);

		if (zone == calendar_config_get_icaltimezone ()) {
			/* Hide timezone part */
			GtkAction *action;

			action = e_comp_editor_get_action (comp_editor, "view-timezone");
			gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), FALSE);
		}
	}

	if (icaltime_is_valid_time (dtstart) && !icaltime_is_null_time (dtstart) &&
	    (!icaltime_is_valid_time (dtend) || icaltime_is_null_time (dtend))) {
		dtend = dtstart;
		if (dtstart.is_date)
			icaltime_adjust (&dtend, 1, 0, 0, 0);
	}

	if (icaltime_is_valid_time (dtend) && !icaltime_is_null_time (dtend)) {
		if (dtstart.is_date && dtend.is_date) {
			all_day_event = TRUE;
			if (icaltime_compare_date_only (dtend, dtstart) > 0) {
				icaltime_adjust (&dtend, -1, 0, 0, 0);
			}
		}

		e_comp_editor_property_part_datetime_set_value (
			E_COMP_EDITOR_PROPERTY_PART_DATETIME (event_editor->priv->dtend), dtend);
	}

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (event_editor->priv->all_day_check), all_day_event);

	prop = icalcomponent_get_first_property (component, ICAL_CLASS_PROPERTY);
	if (prop && icalproperty_get_class (prop) == ICAL_CLASS_PRIVATE)
		action = e_comp_editor_get_action (comp_editor, "classify-private");
	else if (prop && icalproperty_get_class (prop) == ICAL_CLASS_CONFIDENTIAL)
		action = e_comp_editor_get_action (comp_editor, "classify-confidential");
	else if (!(flags & E_COMP_EDITOR_FLAG_IS_NEW))
		action = e_comp_editor_get_action (comp_editor, "classify-public");
	else {
		GSettings *settings;

		settings = e_util_ref_settings ("org.gnome.evolution.calendar");

		if (g_settings_get_boolean (settings, "classify-private")) {
			action = e_comp_editor_get_action (comp_editor, "classify-private");
		} else {
			action = e_comp_editor_get_action (comp_editor, "classify-public");
		}

		g_object_unref (settings);
	}

	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);
}

static gboolean
ece_event_fill_component (ECompEditor *comp_editor,
			  icalcomponent *component)
{
	ECompEditorEvent *event_editor;
	gboolean date_valid, time_valid;
	icalproperty *dtstart_prop, *dtend_prop;
	icalproperty *prop;
	icalproperty_class class_value;

	g_return_val_if_fail (E_IS_COMP_EDITOR (comp_editor), FALSE);
	g_return_val_if_fail (component != NULL, FALSE);

	if (!E_COMP_EDITOR_CLASS (e_comp_editor_event_parent_class)->fill_component (comp_editor, component))
		return FALSE;

	event_editor = E_COMP_EDITOR_EVENT (comp_editor);

	if (!e_comp_editor_property_part_datetime_check_validity (
		E_COMP_EDITOR_PROPERTY_PART_DATETIME (event_editor->priv->dtstart), &date_valid, &time_valid)) {
		const gchar *error_message = NULL;

		if (!date_valid)
			error_message = g_strdup (_("Start date is not a valid date"));
		else if (!time_valid)
			error_message = g_strdup (_("Start time is not a valid time"));

		e_comp_editor_set_validation_error (comp_editor, event_editor->priv->page_general,
			e_comp_editor_property_part_get_edit_widget (event_editor->priv->dtstart),
			error_message ? error_message : _("Unknown error"));

		return FALSE;
	}

	if (!e_comp_editor_property_part_datetime_check_validity (
		E_COMP_EDITOR_PROPERTY_PART_DATETIME (event_editor->priv->dtend), &date_valid, &time_valid)) {
		const gchar *error_message = NULL;

		if (!date_valid)
			error_message = g_strdup (_("End date is not a valid date"));
		else if (!time_valid)
			error_message = g_strdup (_("End time is not a valid time"));

		e_comp_editor_set_validation_error (comp_editor, event_editor->priv->page_general,
			e_comp_editor_property_part_get_edit_widget (event_editor->priv->dtend),
			error_message ? error_message : _("Unknown error"));

		return FALSE;
	}

	dtstart_prop = icalcomponent_get_first_property (component, ICAL_DTSTART_PROPERTY);
	dtend_prop = icalcomponent_get_first_property (component, ICAL_DTEND_PROPERTY);

	if (dtstart_prop && dtend_prop) {
		struct icaltimetype dtstart, dtend;
		gboolean set_dtstart = FALSE, set_dtend = FALSE;

		dtstart = icalproperty_get_dtstart (dtstart_prop);
		dtend = icalproperty_get_dtend (dtend_prop);

		if (dtstart.is_date && dtend.is_date) {
			ECalClient *client;

			/* Add 1 day to DTEND, as it is not inclusive. */
			icaltime_adjust (&dtend, 1, 0, 0, 0);
			set_dtend = TRUE;

			client = e_comp_editor_get_target_client (comp_editor);
			if (client && e_client_check_capability (E_CLIENT (client), CAL_STATIC_CAPABILITY_ALL_DAY_EVENT_AS_TIME)) {
				ECompEditorEvent *event_editor = E_COMP_EDITOR_EVENT (comp_editor);
				GtkWidget *timezone_entry;

				dtstart.is_date = FALSE;
				dtstart.hour = 0;
				dtstart.minute = 0;
				dtstart.second = 0;

				dtend.is_date = FALSE;
				dtend.hour = 0;
				dtend.minute = 0;
				dtend.second = 0;

				timezone_entry = e_comp_editor_property_part_get_edit_widget (event_editor->priv->timezone);

				dtstart.zone = e_timezone_entry_get_timezone (E_TIMEZONE_ENTRY (timezone_entry));
				if (!dtstart.zone)
					dtstart.zone = icaltimezone_get_utc_timezone ();
				dtstart.is_utc = dtstart.zone == icaltimezone_get_utc_timezone ();

				dtend.zone = dtstart.zone;
				dtend.is_utc = dtstart.is_utc;

				set_dtstart = TRUE;
				set_dtend = TRUE;
			}
		}

		if (set_dtstart) {
			icalproperty_set_dtstart (dtstart_prop, dtstart);
			cal_comp_util_update_tzid_parameter (dtstart_prop, dtstart);
		}

		if (set_dtend) {
			icalproperty_set_dtend (dtend_prop, dtend);
			cal_comp_util_update_tzid_parameter (dtend_prop, dtend);
		}
	}

	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (
		e_comp_editor_get_action (comp_editor, "classify-private"))))
		class_value = ICAL_CLASS_PRIVATE;
	else if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (
		e_comp_editor_get_action (comp_editor, "classify-confidential"))))
		class_value = ICAL_CLASS_CONFIDENTIAL;
	else
		class_value = ICAL_CLASS_PUBLIC;

	prop = icalcomponent_get_first_property (component, ICAL_CLASS_PROPERTY);
	if (prop) {
		icalproperty_set_class (prop, class_value);
	} else {
		prop = icalproperty_new_class (class_value);
		icalcomponent_add_property (component, prop);
	}

	return TRUE;
}

static void
ece_event_setup_ui (ECompEditorEvent *event_editor)
{
	const gchar *ui =
		"<ui>"
		"  <menubar action='main-menu'>"
		"    <menu action='view-menu'>"
		"      <placeholder name='parts'>"
		"        <menuitem action='view-timezone'/>"
		"        <menuitem action='view-categories'/>"
		"      </placeholder>"
		"    </menu>"
		"    <menu action='options-menu'>"
		"      <placeholder name='toggles'>"
		"        <menuitem action='all-day-event'/>"
		"        <menuitem action='show-time-busy'/>"
		"        <menu action='classification-menu'>"
		"          <menuitem action='classify-public'/>"
		"          <menuitem action='classify-private'/>"
		"          <menuitem action='classify-confidential'/>"
		"        </menu>"
		"      </placeholder>"
		"    </menu>"
		"  </menubar>"
		"  <toolbar name='main-toolbar'>"
		"    <placeholder name='content'>\n"
		"      <toolitem action='all-day-event'/>\n"
		"      <toolitem action='show-time-busy'/>\n"
		"    </placeholder>"
		"  </toolbar>"
		"</ui>";

	const GtkToggleActionEntry view_actions[] = {
		{ "view-categories",
		  NULL,
		  N_("_Categories"),
		  NULL,
		  N_("Toggles whether to display categories"),
		  NULL,
		  FALSE },

		{ "view-timezone",
		  "stock_timezone",
		  N_("Time _Zone"),
		  NULL,
		  N_("Toggles whether the time zone is displayed"),
		  NULL,
		  FALSE },

		{ "all-day-event",
		  "stock_new-24h-appointment",
		  N_("All _Day Event"),
		  NULL,
		  N_("Toggles whether to have All Day Event"),
		  NULL,
		  FALSE },

		{ "show-time-busy",
		  "dialog-error",
		  N_("Show Time as _Busy"),
		  NULL,
		  N_("Toggles whether to show time as busy"),
		  NULL,
		  FALSE }
	};

	const GtkRadioActionEntry classification_radio_entries[] = {

		{ "classify-public",
		  NULL,
		  N_("Pu_blic"),
		  NULL,
		  N_("Classify as public"),
		  ICAL_CLASS_PUBLIC },

		{ "classify-private",
		  NULL,
		  N_("_Private"),
		  NULL,
		  N_("Classify as private"),
		  ICAL_CLASS_PRIVATE },

		{ "classify-confidential",
		  NULL,
		  N_("_Confidential"),
		  NULL,
		  N_("Classify as confidential"),
		  ICAL_CLASS_CONFIDENTIAL }
	};

	ECompEditor *comp_editor;
	GSettings *settings;
	GtkUIManager *ui_manager;
	GtkAction *action;
	GtkActionGroup *action_group;
	GtkWidget *widget;
	GError *error = NULL;

	g_return_if_fail (E_IS_COMP_EDITOR_EVENT (event_editor));

	comp_editor = E_COMP_EDITOR (event_editor);
	settings = e_comp_editor_get_settings (comp_editor);
	ui_manager = e_comp_editor_get_ui_manager (comp_editor);
	action_group = e_comp_editor_get_action_group (comp_editor, "individual");

	gtk_action_group_add_toggle_actions (action_group,
		view_actions, G_N_ELEMENTS (view_actions), event_editor);

	gtk_action_group_add_radio_actions (
		action_group, classification_radio_entries,
		G_N_ELEMENTS (classification_radio_entries),
		ICAL_CLASS_PUBLIC,
		G_CALLBACK (ece_event_action_classification_cb), event_editor);

	gtk_ui_manager_add_ui_from_string (ui_manager, ui, -1, &error);

	e_plugin_ui_register_manager (ui_manager, "org.gnome.evolution.event-editor", event_editor);
	e_plugin_ui_enable_manager (ui_manager, "org.gnome.evolution.event-editor");

	if (error) {
		g_critical ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
	}

	action = e_comp_editor_get_action (comp_editor, "view-categories");
	e_binding_bind_property (
		event_editor->priv->categories, "visible",
		action, "active",
		G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
	g_settings_bind (
		settings, "editor-show-categories",
		action, "active",
		G_SETTINGS_BIND_DEFAULT);

	action = e_comp_editor_get_action (comp_editor, "view-timezone");
	e_binding_bind_property (
		event_editor->priv->timezone, "visible",
		action, "active",
		G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
	g_settings_bind (
		settings, "editor-show-timezone",
		action, "active",
		G_SETTINGS_BIND_DEFAULT);

	action = e_comp_editor_get_action (comp_editor, "all-day-event");
	e_binding_bind_property (
		event_editor->priv->all_day_check, "active",
		action, "active",
		G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

	widget = e_comp_editor_property_part_get_edit_widget (event_editor->priv->transparency);
	action = e_comp_editor_get_action (comp_editor, "show-time-busy");
	e_binding_bind_property (
		widget, "active",
		action, "active",
		G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
}

static void
e_comp_editor_event_constructed (GObject *object)
{
	ECompEditor *comp_editor;
	ECompEditorEvent *event_editor;
	ECompEditorPage *page;
	ECompEditorPropertyPart *part;
	ECompEditorPropertyPart *summary;
	EFocusTracker *focus_tracker;
	GtkWidget *widget;

	G_OBJECT_CLASS (e_comp_editor_event_parent_class)->constructed (object);

	event_editor = E_COMP_EDITOR_EVENT (object);
	comp_editor = E_COMP_EDITOR (event_editor);
	focus_tracker = e_comp_editor_get_focus_tracker (comp_editor);

	page = e_comp_editor_page_general_new (comp_editor,
		_("_Calendar:"), E_SOURCE_EXTENSION_CALENDAR,
		NULL, FALSE, 2);
	event_editor->priv->page_general = page;

	part = e_comp_editor_property_part_summary_new (focus_tracker);
	e_comp_editor_page_add_property_part (page, part, 0, 2, 3, 1);
	summary = part;

	part = e_comp_editor_property_part_location_new (focus_tracker);
	e_comp_editor_page_add_property_part (page, part, 0, 3, 3, 1);

	part = e_comp_editor_property_part_dtstart_new (C_("ECompEditor", "_Start time:"), FALSE, FALSE);
	e_comp_editor_page_add_property_part (page, part, 0, 4, 2, 1);
	e_comp_editor_property_part_set_sensitize_handled (part, TRUE);
	event_editor->priv->dtstart = part;

	part = e_comp_editor_property_part_dtend_new (C_("ECompEditor", "_End time:"), FALSE, FALSE);
	e_comp_editor_page_add_property_part (page, part, 0, 5, 2, 1);
	e_comp_editor_property_part_set_sensitize_handled (part, TRUE);
	event_editor->priv->dtend = part;

	part = e_comp_editor_property_part_timezone_new ();
	e_comp_editor_page_add_property_part (page, part, 0, 6, 3, 1);
	e_comp_editor_property_part_set_sensitize_handled (part, TRUE);
	event_editor->priv->timezone = part;

	widget = gtk_check_button_new_with_mnemonic (C_("ECompEditor", "All da_y event"));
	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_START,
		NULL);
	gtk_grid_attach (GTK_GRID (page), widget, 2, 4, 1, 1);
	gtk_widget_show (widget);
	event_editor->priv->all_day_check = widget;

	part = e_comp_editor_property_part_transparency_new ();
	e_comp_editor_page_add_property_part (page, part, 2, 5, 1, 1);
	event_editor->priv->transparency = part;

	widget = e_comp_editor_property_part_get_edit_widget (event_editor->priv->transparency);
	/* Transparency checkbox is not shown in the page, even it's packed there */
	gtk_widget_hide (widget);

	part = e_comp_editor_property_part_categories_new (focus_tracker);
	e_comp_editor_page_add_property_part (page, part, 0, 7, 3, 1);
	event_editor->priv->categories = part;

	part = e_comp_editor_property_part_description_new (focus_tracker);
	e_comp_editor_page_add_property_part (page, part, 0, 8, 3, 1);

	widget = e_comp_editor_property_part_get_edit_widget (event_editor->priv->timezone);
	e_comp_editor_property_part_datetime_attach_timezone_entry (
		E_COMP_EDITOR_PROPERTY_PART_DATETIME (event_editor->priv->dtstart),
		E_TIMEZONE_ENTRY (widget));
	e_comp_editor_property_part_datetime_attach_timezone_entry (
		E_COMP_EDITOR_PROPERTY_PART_DATETIME (event_editor->priv->dtend),
		E_TIMEZONE_ENTRY (widget));

	e_comp_editor_set_time_parts (comp_editor, event_editor->priv->dtstart, event_editor->priv->dtend);

	widget = e_comp_editor_property_part_get_edit_widget (event_editor->priv->dtstart);
	e_binding_bind_property (
		event_editor->priv->all_day_check, "active",
		widget, "show-time",
		G_BINDING_INVERT_BOOLEAN | G_BINDING_BIDIRECTIONAL);
	g_signal_connect (widget, "changed", G_CALLBACK (ece_event_dtstart_changed_cb), event_editor);

	widget = e_comp_editor_property_part_get_edit_widget (event_editor->priv->dtend);
	e_binding_bind_property (
		event_editor->priv->all_day_check, "active",
		widget, "show-time",
		G_BINDING_INVERT_BOOLEAN | G_BINDING_BIDIRECTIONAL);
	g_signal_connect (widget, "changed", G_CALLBACK (ece_event_dtend_changed_cb), event_editor);

	e_signal_connect_notify_swapped (event_editor->priv->all_day_check, "notify::active",
		G_CALLBACK (ece_event_all_day_toggled_cb), event_editor);

	e_comp_editor_add_page (comp_editor, C_("ECompEditorPage", "General"), page);

	page = e_comp_editor_page_reminders_new (comp_editor);
	e_comp_editor_add_page (comp_editor, C_("ECompEditorPage", "Reminders"), page);

	page = e_comp_editor_page_recurrence_new (comp_editor);
	e_comp_editor_add_page (comp_editor, C_("ECompEditorPage", "Recurrence"), page);

	page = e_comp_editor_page_attachments_new (comp_editor);
	e_comp_editor_add_page (comp_editor, C_("ECompEditorPage", "Attachments"), page);

	page = e_comp_editor_page_schedule_new (comp_editor,
		e_comp_editor_page_general_get_meeting_store (
		E_COMP_EDITOR_PAGE_GENERAL (event_editor->priv->page_general)));
	e_binding_bind_property (
		event_editor->priv->page_general, "show-attendees",
		page, "visible",
		G_BINDING_SYNC_CREATE);

	e_comp_editor_add_page (comp_editor, C_("ECompEditorPage", "Schedule"), page);

	ece_event_setup_ui (event_editor);

	widget = e_comp_editor_property_part_get_edit_widget (summary);
	e_binding_bind_property (widget, "text", comp_editor, "title-suffix", 0);
	/* Do this as the last thing, because some widgets can call the function as well */
	gtk_widget_grab_focus (widget);
}

static void
e_comp_editor_event_init (ECompEditorEvent *event_editor)
{
	event_editor->priv = G_TYPE_INSTANCE_GET_PRIVATE (event_editor, E_TYPE_COMP_EDITOR_EVENT, ECompEditorEventPrivate);
}

static void
e_comp_editor_event_class_init (ECompEditorEventClass *klass)
{
	GObjectClass *object_class;
	ECompEditorClass *comp_editor_class;

	g_type_class_add_private (klass, sizeof (ECompEditorEventPrivate));

	object_class = G_OBJECT_CLASS (klass);
	object_class->constructed = e_comp_editor_event_constructed;

	comp_editor_class = E_COMP_EDITOR_CLASS (klass);
	comp_editor_class->help_section = "calendar-usage-add-appointment";
	comp_editor_class->title_format_with_attendees = _("Meeting - %s");
	comp_editor_class->title_format_without_attendees = _("Appointment - %s");
	comp_editor_class->icon_name = "appointment-new";
	comp_editor_class->sensitize_widgets = ece_event_sensitize_widgets;
	comp_editor_class->fill_widgets = ece_event_fill_widgets;
	comp_editor_class->fill_component = ece_event_fill_component;
}
