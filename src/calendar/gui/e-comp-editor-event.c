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

#include "evolution-config.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include "e-util/e-util.h"
#include "shell/e-shell.h"

#include "calendar-config.h"
#include "comp-util.h"
#include "e-cal-day-column.h"
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
	ECompEditorPropertyPart *description;
	GtkWidget *all_day_check;
	ECalDayColumn *day_column;
	GtkAdjustment *day_column_vadjustment; /* owned */

	gpointer in_the_past_alert;
	gpointer insensitive_info_alert;

	GdkRGBA bg_rgba_freetime;
	GdkRGBA bg_rgba_clash;
	gboolean day_column_needs_update;

	/* to not layout items inside size-allocate signal */
	guint day_column_layout_id;
	gint day_column_scrolled_last_width;
	gint day_column_scrolled_last_height;
};

G_DEFINE_TYPE_WITH_PRIVATE (ECompEditorEvent, e_comp_editor_event, E_TYPE_COMP_EDITOR)

static void
ece_event_update_times (ECompEditorEvent *event_editor,
			EDateEdit *date_edit,
			gboolean change_end_datetime)
{
	guint flags;

	g_return_if_fail (E_IS_COMP_EDITOR_EVENT (event_editor));
	g_return_if_fail (E_IS_DATE_EDIT (date_edit));

	if (e_date_edit_has_focus (date_edit) ||
	    !e_date_edit_date_is_valid (date_edit) ||
	    !e_date_edit_time_is_valid (date_edit))
		return;

	if (!e_comp_editor_get_updating (E_COMP_EDITOR (event_editor))) {
		e_comp_editor_ensure_start_before_end (E_COMP_EDITOR (event_editor),
			event_editor->priv->dtstart,
			event_editor->priv->dtend,
			change_end_datetime);
		e_comp_editor_ensure_same_value_type (E_COMP_EDITOR (event_editor),
			change_end_datetime ? event_editor->priv->dtstart : event_editor->priv->dtend,
			change_end_datetime ? event_editor->priv->dtend : event_editor->priv->dtstart);
	}

	flags = e_comp_editor_get_flags (E_COMP_EDITOR (event_editor));

	if ((flags & E_COMP_EDITOR_FLAG_IS_NEW) != 0) {
		ICalTime *start_tt;

		start_tt = e_comp_editor_property_part_datetime_get_value (
			E_COMP_EDITOR_PROPERTY_PART_DATETIME (event_editor->priv->dtstart));

		if (cal_comp_util_compare_time_with_today (start_tt) < 0) {
			if (!event_editor->priv->in_the_past_alert) {
				EAlert *alert;

				alert = e_comp_editor_add_warning (E_COMP_EDITOR (event_editor),
					_("Event’s time is in the past"), NULL);

				event_editor->priv->in_the_past_alert = alert;

				if (alert)
					g_object_add_weak_pointer (G_OBJECT (alert), &event_editor->priv->in_the_past_alert);

				g_clear_object (&alert);
			}
		} else if (event_editor->priv->in_the_past_alert) {
			e_alert_response (event_editor->priv->in_the_past_alert, GTK_RESPONSE_OK);
		}

		g_clear_object (&start_tt);
	}
}

static void
ece_event_dtstart_changed_cb (EDateEdit *date_edit,
			      ECompEditorEvent *event_editor)
{
	g_return_if_fail (E_IS_DATE_EDIT (date_edit));
	g_return_if_fail (E_IS_COMP_EDITOR_EVENT (event_editor));

	if (!e_date_edit_has_focus (date_edit))
		ece_event_update_times (event_editor, date_edit, TRUE);
}

static void
ece_event_dtend_changed_cb (EDateEdit *date_edit,
			    ECompEditorEvent *event_editor)
{
	g_return_if_fail (E_IS_DATE_EDIT (date_edit));
	g_return_if_fail (E_IS_COMP_EDITOR_EVENT (event_editor));

	if (!e_date_edit_has_focus (date_edit))
		ece_event_update_times (event_editor, date_edit, FALSE);
}

static void
ece_event_all_day_toggled_cb (ECompEditorEvent *event_editor)
{
	GtkWidget *edit_widget;

	g_return_if_fail (E_IS_COMP_EDITOR_EVENT (event_editor));

	edit_widget = e_comp_editor_property_part_get_edit_widget (event_editor->priv->dtstart);

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (event_editor->priv->all_day_check))) {
		gint hour, minute;

		if (!e_date_edit_get_time_of_day (E_DATE_EDIT (edit_widget), &hour, &minute))
			e_date_edit_set_time_of_day (E_DATE_EDIT (edit_widget), 0, 0);
	}

	ece_event_update_times (event_editor, E_DATE_EDIT (edit_widget), TRUE);

	e_comp_editor_ensure_changed (E_COMP_EDITOR (event_editor));
}

static void
ece_event_sensitize_widgets (ECompEditor *comp_editor,
			     gboolean force_insensitive)
{
	ECompEditorEvent *event_editor;
	gboolean is_organizer;
	EUIAction *action;
	GtkWidget *widget;
	guint32 flags;

	g_return_if_fail (E_IS_COMP_EDITOR_EVENT (comp_editor));

	E_COMP_EDITOR_CLASS (e_comp_editor_event_parent_class)->sensitize_widgets (comp_editor, force_insensitive);

	flags = e_comp_editor_get_flags (comp_editor);
	is_organizer = (flags & (E_COMP_EDITOR_FLAG_IS_NEW | E_COMP_EDITOR_FLAG_ORGANIZER_IS_USER)) != 0;
	event_editor = E_COMP_EDITOR_EVENT (comp_editor);

	gtk_widget_set_sensitive (event_editor->priv->all_day_check, !force_insensitive);

	#define sensitize_part(x) G_STMT_START { \
		widget = e_comp_editor_property_part_get_label_widget (x); \
		if (widget) \
			gtk_widget_set_sensitive (widget, !force_insensitive); \
		\
		widget = e_comp_editor_property_part_get_edit_widget (x); \
		if (widget) \
			gtk_widget_set_sensitive (widget, !force_insensitive); \
	} G_STMT_END

	sensitize_part (event_editor->priv->dtstart);
	sensitize_part (event_editor->priv->dtend);
	sensitize_part (event_editor->priv->timezone);

	#undef sensitize_part

	action = e_comp_editor_get_action (comp_editor, "all-day-event");
	e_ui_action_set_sensitive (action, !force_insensitive);

	/* Disable radio items, instead of the whole submenu,
	   to see the value with read-only events/calendars. */
	action = e_comp_editor_get_action (comp_editor, "classify-private");
	e_ui_action_set_sensitive (action, !force_insensitive);

	action = e_comp_editor_get_action (comp_editor, "classify-confidential");
	e_ui_action_set_sensitive (action, !force_insensitive);

	action = e_comp_editor_get_action (comp_editor, "classify-public");
	e_ui_action_set_sensitive (action, !force_insensitive);

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
			message = _("Changes made to the event will not be sent to the attendees, because you are not the organizer");

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

static ICalTimezone *
ece_event_get_timezone_from_property (ECompEditor *comp_editor,
				      ICalProperty *property)
{
	ECalClient *client;
	ICalParameter *param;
	ICalTimezone *zone = NULL;
	const gchar *tzid;

	g_return_val_if_fail (E_IS_COMP_EDITOR (comp_editor), NULL);

	if (!property)
		return NULL;

	param = i_cal_property_get_first_parameter (property, I_CAL_TZID_PARAMETER);
	if (!param)
		return NULL;

	tzid = i_cal_parameter_get_tzid (param);
	if (!tzid || !*tzid) {
		g_object_unref (param);
		return NULL;
	}

	if (g_ascii_strcasecmp (tzid, "UTC") == 0) {
		g_object_unref (param);
		return i_cal_timezone_get_utc_timezone ();
	}

	client = e_comp_editor_get_source_client (comp_editor);
	/* It should be already fetched for the UI, thus this should be non-blocking. */
	if (client && e_cal_client_get_timezone_sync (client, tzid, &zone, NULL, NULL) && zone) {
		g_object_unref (param);
		return zone;
	}

	zone = i_cal_timezone_get_builtin_timezone_from_tzid (tzid);
	if (!zone)
		zone = i_cal_timezone_get_builtin_timezone (tzid);

	g_object_unref (param);

	return zone;
}

static void
ece_event_update_timezone (ECompEditorEvent *event_editor,
			   ICalTime **out_dtstart,
			   ICalTime **out_dtend)
{
	ECompEditor *comp_editor;
	ICalTime *dtstart = NULL, *dtend = NULL;
	ICalComponent *component;
	ICalProperty *prop;
	ICalTimezone *zone = NULL;
	gboolean has_property = FALSE, is_date_value = FALSE;

	g_return_if_fail (E_IS_COMP_EDITOR_EVENT (event_editor));

	comp_editor = E_COMP_EDITOR (event_editor);

	component = e_comp_editor_get_component (comp_editor);
	if (!component) {
		if (out_dtstart)
			*out_dtstart = NULL;

		if (out_dtend)
			*out_dtend = NULL;

		return;
	}

	if (e_cal_util_component_has_property (component, I_CAL_DTSTART_PROPERTY)) {
		has_property = TRUE;

		dtstart = i_cal_component_get_dtstart (component);
		if (dtstart && i_cal_time_is_valid_time (dtstart)) {
			if (i_cal_time_is_date (dtstart)) {
				zone = NULL;
				is_date_value = TRUE;
			} else if (i_cal_time_is_utc (dtstart)) {
				zone = i_cal_timezone_get_utc_timezone ();
			} else {
				prop = i_cal_component_get_first_property (component, I_CAL_DTSTART_PROPERTY);
				zone = ece_event_get_timezone_from_property (comp_editor, prop);
				g_clear_object (&prop);
			}
		}
	}

	if (e_cal_util_component_has_property (component, I_CAL_DTEND_PROPERTY)) {
		has_property = TRUE;

		dtend = i_cal_component_get_dtend (component);
		if (!zone && i_cal_time_is_valid_time (dtend)) {
			if (i_cal_time_is_date (dtend)) {
				zone = NULL;
				is_date_value = TRUE;
			} else if (i_cal_time_is_utc (dtend)) {
				zone = i_cal_timezone_get_utc_timezone ();
			} else {
				prop = i_cal_component_get_first_property (component, I_CAL_DTEND_PROPERTY);
				zone = ece_event_get_timezone_from_property (comp_editor, prop);
				g_clear_object (&prop);
			}
		}
	}

	if (!zone && e_cal_util_component_has_property (component, I_CAL_DUE_PROPERTY)) {
		ICalTime *itt;

		has_property = TRUE;

		itt = i_cal_component_get_due (component);
		if (itt && i_cal_time_is_valid_time (itt)) {
			if (i_cal_time_is_date (itt)) {
				zone = NULL;
				is_date_value = TRUE;
			} else if (i_cal_time_is_utc (itt)) {
				zone = i_cal_timezone_get_utc_timezone ();
			} else {
				prop = i_cal_component_get_first_property (component, I_CAL_DUE_PROPERTY);
				zone = ece_event_get_timezone_from_property (comp_editor, prop);
				g_clear_object (&prop);
			}
		}

		g_clear_object (&itt);
	}

	if (has_property) {
		GtkWidget *edit_widget;
		ICalTimezone *cfg_zone;

		edit_widget = e_comp_editor_property_part_get_edit_widget (event_editor->priv->timezone);

		if (!zone && is_date_value)
			e_timezone_entry_set_timezone (E_TIMEZONE_ENTRY (edit_widget), calendar_config_get_icaltimezone ());
		else
			e_timezone_entry_set_timezone (E_TIMEZONE_ENTRY (edit_widget), zone);

		cfg_zone = calendar_config_get_icaltimezone ();

		if (zone && cfg_zone && zone != cfg_zone &&
		    (g_strcmp0 (i_cal_timezone_get_location (zone), i_cal_timezone_get_location (cfg_zone)) != 0 ||
		     g_strcmp0 (i_cal_timezone_get_tzid (zone), i_cal_timezone_get_tzid (cfg_zone)) != 0)) {
			/* Show timezone part */
			EUIAction *action;

			action = e_comp_editor_get_action (comp_editor, "view-timezone");
			e_ui_action_set_active (action, TRUE);
		}
	}

	if (out_dtstart)
		*out_dtstart = dtstart;
	else
		g_clear_object (&dtstart);

	if (out_dtend)
		*out_dtend = dtend;
	else
		g_clear_object (&dtend);
}

static void ece_event_update_day_agenda (ECompEditor *comp_editor);

static void
ece_event_fill_widgets (ECompEditor *comp_editor,
			ICalComponent *component)
{
	ECompEditorEvent *event_editor;
	EUIAction *action;
	ICalTime *dtstart, *dtend;
	ICalProperty *prop;
	gboolean all_day_event = FALSE;
	guint32 flags;

	g_return_if_fail (E_IS_COMP_EDITOR_EVENT (comp_editor));
	g_return_if_fail (component != NULL);

	event_editor = E_COMP_EDITOR_EVENT (comp_editor);

	flags = e_comp_editor_get_flags (comp_editor);
	dtstart = NULL;
	dtend = NULL;

	/* Set timezone before the times, because they are converted into this timezone */
	ece_event_update_timezone (event_editor, &dtstart, &dtend);

	E_COMP_EDITOR_CLASS (e_comp_editor_event_parent_class)->fill_widgets (comp_editor, component);

	if (dtstart && i_cal_time_is_valid_time (dtstart) && !i_cal_time_is_null_time (dtstart) &&
	    (!dtend || !i_cal_time_is_valid_time (dtend) || i_cal_time_is_null_time (dtend))) {
		gboolean dtend_set = FALSE;
		g_clear_object (&dtend);
		dtend = i_cal_time_clone (dtstart);

		if (e_cal_util_component_has_property (component, I_CAL_DURATION_PROPERTY)) {
			prop = i_cal_component_get_first_property (component, I_CAL_DURATION_PROPERTY);
			if (prop) {
				ICalDuration *duration;

				g_clear_object (&prop);

				duration = i_cal_component_get_duration (component);
				if (!duration || i_cal_duration_is_null_duration (duration) || i_cal_duration_is_bad_duration (duration)) {
					g_clear_object (&duration);
				/* The DURATION shouldn't be negative, but just return DTSTART if it
				 * is, i.e. assume it is 0. */
				} else if (!i_cal_duration_is_neg (duration)) {
					guint dur_days, dur_hours, dur_minutes, dur_seconds;

					/* If DTSTART is a DATE value, then we need to check if the DURATION
					 * includes any hours, minutes or seconds. If it does, we need to
					 * make the DTEND/DUE a DATE-TIME value. */
					dur_days = i_cal_duration_get_days (duration) + (7 * i_cal_duration_get_weeks (duration));
					dur_hours = i_cal_duration_get_hours (duration);
					dur_minutes = i_cal_duration_get_minutes (duration);
					dur_seconds = i_cal_duration_get_seconds (duration);

					if (i_cal_time_is_date (dtend) && (
					    dur_hours != 0 || dur_minutes != 0 || dur_seconds != 0)) {
						i_cal_time_set_is_date (dtend, FALSE);
					}

					/* Add on the DURATION. */
					i_cal_time_adjust (dtend, dur_days, dur_hours, dur_minutes, dur_seconds);

					dtend_set = TRUE;
				}

				g_clear_object (&duration);
			}
		}

		if (!dtend_set && i_cal_time_is_date (dtstart))
			i_cal_time_adjust (dtend, 1, 0, 0, 0);
	}

	if (dtend && i_cal_time_is_valid_time (dtend) && !i_cal_time_is_null_time (dtend)) {
		if (i_cal_time_is_date (dtstart) && i_cal_time_is_date (dtend)) {
			all_day_event = TRUE;
			if (i_cal_time_compare_date_only (dtend, dtstart) > 0) {
				i_cal_time_adjust (dtend, -1, 0, 0, 0);
			}
		}

		e_comp_editor_property_part_datetime_set_value (
			E_COMP_EDITOR_PROPERTY_PART_DATETIME (event_editor->priv->dtend), dtend);
	}

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (event_editor->priv->all_day_check), all_day_event);

	prop = i_cal_component_get_first_property (component, I_CAL_CLASS_PROPERTY);
	if (prop && i_cal_property_get_class (prop) == I_CAL_CLASS_PRIVATE)
		action = e_comp_editor_get_action (comp_editor, "classify-private");
	else if (prop && i_cal_property_get_class (prop) == I_CAL_CLASS_CONFIDENTIAL)
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

	e_ui_action_set_active (action, TRUE);

	g_clear_object (&dtstart);
	g_clear_object (&dtend);
	g_clear_object (&prop);

	ece_event_update_day_agenda (comp_editor);
}

static gboolean
ece_event_client_needs_all_day_as_time (ECompEditor *comp_editor)
{
	ECalClient *client;

	g_return_val_if_fail (E_IS_COMP_EDITOR (comp_editor), FALSE);

	client = e_comp_editor_get_target_client (comp_editor);

	return client && e_client_check_capability (E_CLIENT (client), E_CAL_STATIC_CAPABILITY_ALL_DAY_EVENT_AS_TIME);
}

static gboolean
ece_event_fill_component (ECompEditor *comp_editor,
			  ICalComponent *component)
{
	ECompEditorEvent *event_editor;
	ICalProperty *dtstart_prop, *dtend_prop;
	ICalProperty *prop;
	ICalProperty_Class class_value;
	gboolean date_valid, time_valid;

	g_return_val_if_fail (E_IS_COMP_EDITOR (comp_editor), FALSE);
	g_return_val_if_fail (I_CAL_IS_COMPONENT (component), FALSE);

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

	dtstart_prop = i_cal_component_get_first_property (component, I_CAL_DTSTART_PROPERTY);
	dtend_prop = i_cal_component_get_first_property (component, I_CAL_DTEND_PROPERTY);

	if (dtstart_prop && dtend_prop) {
		ICalTime *dtstart, *dtend;
		gboolean set_dtstart = FALSE, set_dtend = FALSE;

		dtstart = i_cal_property_get_dtstart (dtstart_prop);
		dtend = i_cal_property_get_dtend (dtend_prop);

		if (dtstart && i_cal_time_is_date (dtstart) &&
		    dtend && i_cal_time_is_date (dtend)) {
			/* Add 1 day to DTEND, as it is not inclusive. */
			i_cal_time_adjust (dtend, 1, 0, 0, 0);
			set_dtend = TRUE;

			if (ece_event_client_needs_all_day_as_time (comp_editor)) {
				GtkWidget *timezone_entry;
				ICalTimezone *zone;

				timezone_entry = e_comp_editor_property_part_get_edit_widget (event_editor->priv->timezone);
				zone = e_timezone_entry_get_timezone (E_TIMEZONE_ENTRY (timezone_entry));

				cal_comp_util_ensure_allday_timezone (dtstart, zone);
				cal_comp_util_ensure_allday_timezone (dtend, zone);

				set_dtstart = TRUE;
			}
		}

		if (set_dtstart) {
			/* Remove the VALUE parameter, to correspond to the actual value being set */
			i_cal_property_remove_parameter_by_kind (dtstart_prop, I_CAL_VALUE_PARAMETER);

			i_cal_property_set_dtstart (dtstart_prop, dtstart);
			cal_comp_util_update_tzid_parameter (dtstart_prop, dtstart);
		}

		if (set_dtend) {
			/* Remove the VALUE parameter, to correspond to the actual value being set */
			i_cal_property_remove_parameter_by_kind (dtend_prop, I_CAL_VALUE_PARAMETER);

			i_cal_property_set_dtend (dtend_prop, dtend);
			cal_comp_util_update_tzid_parameter (dtend_prop, dtend);

			e_cal_util_component_remove_property_by_kind (component, I_CAL_DURATION_PROPERTY, TRUE);
		}

		g_clear_object (&dtstart);
		g_clear_object (&dtend);
	}

	g_clear_object (&dtstart_prop);
	g_clear_object (&dtend_prop);

	if (e_ui_action_get_active (e_comp_editor_get_action (comp_editor, "classify-private")))
		class_value = I_CAL_CLASS_PRIVATE;
	else if (e_ui_action_get_active (e_comp_editor_get_action (comp_editor, "classify-confidential")))
		class_value = I_CAL_CLASS_CONFIDENTIAL;
	else
		class_value = I_CAL_CLASS_PUBLIC;

	prop = i_cal_component_get_first_property (component, I_CAL_CLASS_PROPERTY);
	if (prop) {
		i_cal_property_set_class (prop, class_value);
		g_object_unref (prop);
	} else {
		prop = i_cal_property_new_class (class_value);
		i_cal_component_take_property (component, prop);
	}

	return TRUE;
}

static void
ece_event_notify_source_client_cb (GObject *object,
				   GParamSpec *param,
				   gpointer user_data)
{
	g_return_if_fail (E_IS_COMP_EDITOR_EVENT (object));

	ece_event_update_timezone (E_COMP_EDITOR_EVENT (object), NULL, NULL);
}

static void
ece_event_notify_target_client_cb (GObject *object,
				   GParamSpec *param,
				   gpointer user_data)
{
	ECompEditorEvent *event_editor;
	EUIAction *action;

	g_return_if_fail (E_IS_COMP_EDITOR_EVENT (object));

	event_editor = E_COMP_EDITOR_EVENT (object);
	action = e_comp_editor_get_action (E_COMP_EDITOR (event_editor), "view-timezone");

	/* These influence whether the timezone part is visible and they
	   depend on the target's client ece_event_client_needs_all_day_as_time() */
	g_object_notify (G_OBJECT (action), "active");
	g_object_notify (G_OBJECT (event_editor->priv->all_day_check), "active");
}

static gboolean
transform_action_to_timezone_visible_cb (GBinding *binding,
					 const GValue *from_value,
					 GValue *to_value,
					 gpointer user_data)
{
	ECompEditorEvent *event_editor = user_data;
	GtkToggleButton *all_day_check = GTK_TOGGLE_BUTTON (event_editor->priv->all_day_check);

	g_value_set_boolean (to_value,
		g_value_get_boolean (from_value) &&
		(!gtk_toggle_button_get_active (all_day_check) || ece_event_client_needs_all_day_as_time (E_COMP_EDITOR (event_editor))));

	return TRUE;
}

static gboolean
transform_toggle_to_timezone_visible_cb (GBinding *binding,
					 const GValue *from_value,
					 GValue *to_value,
					 gpointer user_data)
{
	ECompEditor *comp_editor = user_data;
	EUIAction *action = e_comp_editor_get_action (comp_editor, "view-timezone");

	g_value_set_boolean (to_value,
		e_ui_action_get_active (action) &&
		(!g_value_get_boolean (from_value) || ece_event_client_needs_all_day_as_time (comp_editor)));

	return TRUE;
}

static gboolean
transform_all_day_check_to_action_sensitive_cb (GBinding *binding,
						const GValue *from_value,
						GValue *to_value,
						gpointer user_data)
{
	ECompEditor *comp_editor = user_data;

	g_value_set_boolean (to_value,
		!g_value_get_boolean (from_value) ||
		ece_event_client_needs_all_day_as_time (comp_editor));

	return TRUE;
}

static void
ece_event_classification_radio_set_state_cb (EUIAction *action,
					     GVariant *parameter,
					     gpointer user_data)
{
	ECompEditor *self = user_data;

	g_return_if_fail (E_IS_COMP_EDITOR_EVENT (self));

	e_ui_action_set_state (action, parameter);

	e_comp_editor_set_changed (self, TRUE);
}

static gboolean
e_comp_editor_event_source_filter_cb (ESource *source,
				      gpointer user_data)
{
	gpointer extension;

	if (!e_source_has_extension (source, E_SOURCE_EXTENSION_CALENDAR))
		return FALSE;

	extension = e_source_get_extension (source, E_SOURCE_EXTENSION_CALENDAR);
	if (!E_IS_SOURCE_SELECTABLE (extension))
		return FALSE;

	return e_source_selectable_get_selected (extension);
}

static void
ece_event_update_day_agenda (ECompEditor *comp_editor)
{
	ECompEditorEvent *self;
	ECompEditorPropertyPartDatetime *dtstart;
	ECompEditorPropertyPart *dtstart_part = NULL, *dtend_part = NULL;
	ECalClient *source_client;
	ICalTime *start_itt;
	ICalTimezone *zone;
	const gchar *uid;
	time_t range_start;
	gint second = 0;
	gint hl_hour_start = 0, hl_minute_start = 0;
	gint hl_hour_end, hl_minute_end;

	g_return_if_fail (E_IS_COMP_EDITOR_EVENT (comp_editor));

	self = E_COMP_EDITOR_EVENT (comp_editor);

	if (!gtk_widget_get_visible (GTK_WIDGET (self->priv->day_column))) {
		self->priv->day_column_needs_update = TRUE;
		e_cal_day_column_set_range (self->priv->day_column, 0, 0);
		return;
	}

	self->priv->day_column_needs_update = FALSE;

	e_comp_editor_get_time_parts (comp_editor, &dtstart_part, &dtend_part);

	if (!dtstart_part)
		return;

	dtstart = E_COMP_EDITOR_PROPERTY_PART_DATETIME (dtstart_part);
	start_itt = e_comp_editor_property_part_datetime_get_value (dtstart);

	if (!start_itt)
		return;

	if (i_cal_time_is_date (start_itt)) {
		i_cal_time_set_is_date (start_itt, FALSE);
		hl_hour_start = 0;
		hl_minute_start = 0;
	} else {
		i_cal_time_get_time (start_itt, &hl_hour_start, &hl_minute_start, &second);
	}
	i_cal_time_set_time (start_itt, 0, 0, 0);

	zone = e_cal_day_column_get_timezone (self->priv->day_column);
	range_start = i_cal_time_as_timet_with_zone (start_itt, zone);

	hl_hour_end = hl_hour_start;
	hl_minute_end = hl_minute_start;

	if (dtend_part) {
		ECompEditorPropertyPartDatetime *dtend;
		ICalTime *end_itt;

		dtend = E_COMP_EDITOR_PROPERTY_PART_DATETIME (dtend_part);
		end_itt = e_comp_editor_property_part_datetime_get_value (dtend);

		if (end_itt) {
			if (i_cal_time_is_date (end_itt) || i_cal_time_compare_date_only_tz (start_itt, end_itt, zone) != 0) {
				hl_hour_end = 23;
				hl_minute_end = 60;
			} else {
				i_cal_time_get_time (end_itt, &hl_hour_end, &hl_minute_end, &second);
			}

			g_clear_object (&end_itt);
		}
	}

	g_clear_object (&start_itt);

	e_cal_day_column_set_range (self->priv->day_column, range_start, range_start + (24 * 60 * 60));

	if (!(e_comp_editor_get_flags (comp_editor) & E_COMP_EDITOR_FLAG_IS_NEW)) {
		ICalComponent *icomp;

		icomp = e_comp_editor_get_component (comp_editor);
		uid = icomp ? i_cal_component_get_uid (icomp) : NULL;
		source_client = e_comp_editor_get_source_client (comp_editor);
	} else {
		uid = NULL;
		source_client = NULL;
	}

	e_cal_day_column_highlight_time	(self->priv->day_column,
		source_client, uid,
		hl_hour_start, hl_minute_start, hl_hour_end, hl_minute_end,
		&self->priv->bg_rgba_freetime, &self->priv->bg_rgba_clash);

	if (self->priv->day_column_vadjustment) {
		gint scroll_to_hour = hl_hour_start, yy;

		if (scroll_to_hour > 1)
			scroll_to_hour--;

		yy = e_cal_day_column_time_to_y (self->priv->day_column, scroll_to_hour, 0);
		gtk_adjustment_set_value (self->priv->day_column_vadjustment, yy);
	}
}

static void
e_comp_editor_event_unmap (GtkWidget *widget)
{
	ECompEditor *comp_editor = E_COMP_EDITOR (widget);
	GSettings *settings;
	gint width = 10, height = 10;

	gtk_window_get_size (GTK_WINDOW (comp_editor), &width, &height);

	/* the comp_editor can have the settings already freed, thus get the new one */
	settings = e_util_ref_settings ("org.gnome.evolution.calendar");
	g_settings_set_int (settings, "editor-event-window-width", width);
	g_clear_object (&settings);

	GTK_WIDGET_CLASS (e_comp_editor_event_parent_class)->unmap (widget);
}

static void
e_comp_editor_event_day_column_timezone_notify_cb (GObject *object,
						   GParamSpec *param,
						   gpointer user_data)
{
	ECompEditorEvent *self = user_data;

	if (gtk_widget_get_visible (GTK_WIDGET (object)))
		ece_event_update_day_agenda (E_COMP_EDITOR (self));
	else
		self->priv->day_column_needs_update = TRUE;
}

static void
e_comp_editor_event_day_column_visible_notify_cb (GObject *object,
						  GParamSpec *param,
						  gpointer user_data)
{
	ECompEditorEvent *self = user_data;

	if (self->priv->day_column_needs_update && gtk_widget_get_visible (GTK_WIDGET (object)))
		ece_event_update_day_agenda (E_COMP_EDITOR (self));
}

static gboolean
e_comp_editor_event_day_column_layout_idle_cb (gpointer user_data)
{
	ECompEditorEvent *self = user_data;

	self->priv->day_column_layout_id = 0;

	if (self->priv->day_column_vadjustment && gtk_widget_get_visible (GTK_WIDGET (self))) {
		GtkWidget *vscrollbar;
		GtkWidget *scolled_window;
		gint prefer_width;

		scolled_window = gtk_widget_get_ancestor (GTK_WIDGET (self->priv->day_column), GTK_TYPE_SCROLLED_WINDOW);

		if (scolled_window) {
			prefer_width = gtk_widget_get_allocated_width (scolled_window) - 15;

			vscrollbar = gtk_scrolled_window_get_vscrollbar (GTK_SCROLLED_WINDOW (scolled_window));
			if (vscrollbar && gtk_widget_get_visible (vscrollbar))
				prefer_width -= gtk_widget_get_allocated_width (vscrollbar);

			e_cal_day_column_layout_for_width (self->priv->day_column, prefer_width);
		}
	}

	return G_SOURCE_REMOVE;
}

static void
e_comp_editor_event_day_column_scrolled_size_allocate_cb (GtkWidget *widget,
							  GdkRectangle *rectangle,
							  gpointer user_data)
{
	ECompEditorEvent *self = user_data;

	if ((rectangle->width != self->priv->day_column_scrolled_last_width ||
	    rectangle->height != self->priv->day_column_scrolled_last_height) &&
	    self->priv->day_column_vadjustment && gtk_widget_get_visible (widget) &&
	    !self->priv->day_column_layout_id) {
		self->priv->day_column_scrolled_last_width = rectangle->width;
		self->priv->day_column_scrolled_last_height = rectangle->height;
		self->priv->day_column_layout_id = g_idle_add (e_comp_editor_event_day_column_layout_idle_cb, self);
	}
}

static void
ece_event_setup_ui (ECompEditorEvent *event_editor)
{
	static const gchar *eui =
		"<eui>"
		  "<menu id='main-menu'>"
		    "<submenu action='view-menu'>"
		      "<placeholder id='parts'>"
			"<item action='view-timezone' text_only='true'/>"
			"<item action='view-categories' text_only='true'/>"
			"<item action='view-day-agenda' text_only='true'/>"
		      "</placeholder>"
		    "</submenu>"
		    "<submenu action='options-menu'>"
		      "<placeholder id='toggles'>"
			"<item action='all-day-event' text_only='true'/>"
			"<item action='show-time-busy' text_only='true'/>"
			"<submenu action='classification-menu'>"
			  "<item action='classify-public' group='classification'/>"
			  "<item action='classify-private' group='classification'/>"
			  "<item action='classify-confidential' group='classification'/>"
			"</submenu>"
		      "</placeholder>"
		    "</submenu>"
		  "</menu>"
		  "<toolbar id='toolbar-with-headerbar'>"
		    "<placeholder id='content'>"
		      "<item action='all-day-event'/>"
		      "<item action='show-time-busy'/>"
		    "</placeholder>"
		  "</toolbar>"
		  "<toolbar id='toolbar-without-headerbar'>"
		    "<placeholder id='content'>"
		      "<item action='all-day-event'/>"
		      "<item action='show-time-busy'/>"
		    "</placeholder>"
		  "</toolbar>"
		"</eui>";

	static const EUIActionEntry entries[] = {
		{ "view-categories",
		  NULL,
		  N_("_Categories"),
		  NULL,
		  N_("Toggles whether to display categories"),
		  NULL, NULL, "false", (EUIActionFunc) e_ui_action_set_state },

		{ "view-timezone",
		  "stock_timezone",
		  N_("Time _Zone"),
		  NULL,
		  N_("Toggles whether the time zone is displayed"),
		  NULL, NULL, "false", (EUIActionFunc) e_ui_action_set_state },

		{ "view-day-agenda",
		  NULL,
		  N_("_Day Agenda"),
		  NULL,
		  N_("Toggles whether the day agenda is displayed"),
		  NULL, NULL, "true", (EUIActionFunc) e_ui_action_set_state },

		{ "all-day-event",
		  "stock_new-24h-appointment",
		  N_("All _Day Event"),
		  "<Control>Y",
		  N_("Toggles whether to have All Day Event"),
		  NULL, NULL, "false", (EUIActionFunc) e_ui_action_set_state },

		{ "show-time-busy",
		  "dialog-error",
		  N_("Show Time as _Busy"),
		  NULL,
		  N_("Toggles whether to show time as busy"),
		  NULL, NULL, "false", (EUIActionFunc) e_ui_action_set_state },

		{ "classify-public",
		  NULL,
		  N_("Pu_blic"),
		  NULL,
		  N_("Classify as public"),
		  NULL, "s", "'public'", ece_event_classification_radio_set_state_cb },

		{ "classify-private",
		  NULL,
		  N_("_Private"),
		  NULL,
		  N_("Classify as private"),
		  NULL, "s", "'private'", ece_event_classification_radio_set_state_cb },

		{ "classify-confidential",
		  NULL,
		  N_("_Confidential"),
		  NULL,
		  N_("Classify as confidential"),
		  NULL, "s", "'confidential'", ece_event_classification_radio_set_state_cb }
	};

	ECompEditor *comp_editor;
	GSettings *settings;
	EUIManager *ui_manager;
	EUIAction *action;
	GtkWidget *widget;

	g_return_if_fail (E_IS_COMP_EDITOR_EVENT (event_editor));

	comp_editor = E_COMP_EDITOR (event_editor);
	settings = e_comp_editor_get_settings (comp_editor);
	ui_manager = e_comp_editor_get_ui_manager (comp_editor);

	e_ui_manager_add_actions_with_eui_data (ui_manager, "individual", GETTEXT_PACKAGE,
		entries, G_N_ELEMENTS (entries), event_editor, eui);

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
	e_binding_bind_property_full (
		action, "active",
		event_editor->priv->timezone, "visible",
		G_BINDING_DEFAULT,
		transform_action_to_timezone_visible_cb,
		NULL, /* not used */
		event_editor, NULL);
	e_binding_bind_property_full (
		event_editor->priv->all_day_check, "active",
		event_editor->priv->timezone, "visible",
		G_BINDING_DEFAULT,
		transform_toggle_to_timezone_visible_cb,
		NULL, /* not used */
		event_editor, NULL);
	e_binding_bind_property_full (
		event_editor->priv->all_day_check, "active",
		action, "sensitive",
		G_BINDING_SYNC_CREATE,
		transform_all_day_check_to_action_sensitive_cb,
		NULL, /* not used */
		event_editor, NULL);
	g_settings_bind (
		settings, "editor-show-timezone",
		action, "active",
		G_SETTINGS_BIND_DEFAULT);

	action = e_comp_editor_get_action (comp_editor, "view-day-agenda");
	e_binding_bind_property (
		action, "active",
		event_editor->priv->day_column, "visible",
		G_BINDING_DEFAULT);
	g_settings_bind (
		settings, "editor-event-show-day-agenda",
		action, "active",
		G_SETTINGS_BIND_DEFAULT);

	g_settings_bind (
		settings, "time-divisions",
		event_editor->priv->day_column, "time-division-minutes",
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

	g_settings_bind (
		settings, "use-24hour-format",
		event_editor->priv->day_column, "use-24hour-format",
		G_SETTINGS_BIND_GET | G_SETTINGS_BIND_NO_SENSITIVITY);
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
	EMeetingStore *meeting_store;
	ENameSelector *name_selector;
	EUIManager *ui_manager;
	GtkWidget *widget, *paned, *scrolled_window, *timezone_entry;
	GSettings *settings;

	G_OBJECT_CLASS (e_comp_editor_event_parent_class)->constructed (object);

	event_editor = E_COMP_EDITOR_EVENT (object);
	comp_editor = E_COMP_EDITOR (event_editor);
	ui_manager = e_comp_editor_get_ui_manager (comp_editor);
	focus_tracker = e_comp_editor_get_focus_tracker (comp_editor);

	e_ui_manager_freeze (ui_manager);

	page = e_comp_editor_page_general_new (comp_editor,
		_("_Calendar:"), E_SOURCE_EXTENSION_CALENDAR,
		NULL, FALSE, 2);
	event_editor->priv->page_general = page;

	meeting_store = e_comp_editor_page_general_get_meeting_store (E_COMP_EDITOR_PAGE_GENERAL (event_editor->priv->page_general));
	name_selector = e_comp_editor_page_general_get_name_selector (E_COMP_EDITOR_PAGE_GENERAL (event_editor->priv->page_general));

	part = e_comp_editor_property_part_summary_new (focus_tracker);
	e_comp_editor_page_add_property_part (page, part, 0, 2, 3, 1);
	summary = part;

	part = e_comp_editor_property_part_location_new (focus_tracker);
	e_comp_editor_page_add_property_part (page, part, 0, 3, 3, 1);

	part = e_comp_editor_property_part_dtstart_new (C_("ECompEditor", "_Start time:"), FALSE, FALSE, TRUE);
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

	part = e_comp_editor_property_part_status_new (I_CAL_VEVENT_COMPONENT);
	e_comp_editor_page_add_property_part (page, part, 0, 7, 3, 1);

	widget = e_comp_editor_property_part_get_edit_widget (part);
	gtk_widget_set_halign (widget, GTK_ALIGN_START);
	gtk_widget_set_hexpand (widget, FALSE);

	part = e_comp_editor_property_part_url_new (focus_tracker);
	e_comp_editor_page_add_property_part (page, part, 0, 8, 3, 1);

	part = e_comp_editor_property_part_categories_new (focus_tracker);
	e_comp_editor_page_add_property_part (page, part, 0, 9, 3, 1);
	event_editor->priv->categories = part;

	part = e_comp_editor_property_part_description_new (focus_tracker);
	e_comp_editor_page_add_property_part (page, part, 0, 10, 3, 1);
	event_editor->priv->description = part;

	widget = e_comp_editor_property_part_get_edit_widget (event_editor->priv->timezone);

	e_binding_bind_property (widget, "timezone",
		meeting_store, "timezone",
		G_BINDING_SYNC_CREATE);

	e_comp_editor_property_part_datetime_attach_timezone_entry (
		E_COMP_EDITOR_PROPERTY_PART_DATETIME (event_editor->priv->dtstart),
		E_TIMEZONE_ENTRY (widget));
	g_signal_connect_swapped (event_editor->priv->dtstart, "lookup-timezone",
		G_CALLBACK (e_comp_editor_lookup_timezone), event_editor);
	e_comp_editor_property_part_datetime_attach_timezone_entry (
		E_COMP_EDITOR_PROPERTY_PART_DATETIME (event_editor->priv->dtend),
		E_TIMEZONE_ENTRY (widget));
	g_signal_connect_swapped (event_editor->priv->dtend, "lookup-timezone",
		G_CALLBACK (e_comp_editor_lookup_timezone), event_editor);

	timezone_entry = widget;

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

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	g_object_set (scrolled_window,
		"visible", TRUE,
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"can-focus", FALSE,
		"shadow-type", GTK_SHADOW_NONE,
		"hscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"vscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"propagate-natural-width", FALSE,
		"propagate-natural-height", FALSE,
		"min-content-width", 128,
		NULL);

	event_editor->priv->day_column = e_cal_day_column_new (e_shell_get_client_cache (e_shell_get_default ()), E_ALERT_SINK (event_editor),
		e_comp_editor_event_source_filter_cb, NULL);
	widget = GTK_WIDGET (event_editor->priv->day_column);
	g_object_set (widget,
		"visible", TRUE,
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"can-focus", FALSE,
		"show-time", TRUE,
		NULL);
	e_binding_bind_property (timezone_entry, "timezone",
		widget, "timezone",
		G_BINDING_SYNC_CREATE);
	e_signal_connect_notify_object (widget, "notify::timezone",
		G_CALLBACK (e_comp_editor_event_day_column_timezone_notify_cb), event_editor, 0);

	gtk_container_add (GTK_CONTAINER (scrolled_window), widget);

	e_binding_bind_property (widget, "visible",
		scrolled_window, "visible",
		G_BINDING_SYNC_CREATE);

	event_editor->priv->day_column_vadjustment = g_object_ref (gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (scrolled_window)));

	g_signal_connect (scrolled_window, "size-allocate",
		G_CALLBACK (e_comp_editor_event_day_column_scrolled_size_allocate_cb), event_editor);

	paned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_widget_set_visible (paned, TRUE);
	gtk_paned_pack1 (GTK_PANED (paned), GTK_WIDGET (page), TRUE, FALSE);
	gtk_paned_pack2 (GTK_PANED (paned), scrolled_window, FALSE, FALSE);

	e_comp_editor_add_encapsulated_page (comp_editor, C_("ECompEditorPage", "General"), page, paned);

	page = e_comp_editor_page_reminders_new (comp_editor);
	e_comp_editor_add_page (comp_editor, C_("ECompEditorPage", "Reminders"), page);

	page = e_comp_editor_page_recurrence_new (comp_editor);
	e_comp_editor_add_page (comp_editor, C_("ECompEditorPage", "Recurrence"), page);

	page = e_comp_editor_page_attachments_new (comp_editor);
	e_comp_editor_add_page (comp_editor, C_("ECompEditorPage", "Attachments"), page);

	page = e_comp_editor_page_schedule_new (comp_editor, meeting_store, name_selector);
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

	g_signal_connect (comp_editor, "notify::source-client",
		G_CALLBACK (ece_event_notify_source_client_cb), NULL);

	g_signal_connect (comp_editor, "notify::target-client",
		G_CALLBACK (ece_event_notify_target_client_cb), NULL);

	g_signal_connect (comp_editor, "times-changed",
		G_CALLBACK (ece_event_update_day_agenda), NULL);

	settings = e_comp_editor_get_settings (comp_editor);
	if (settings) {
		gint width = 10, height = 10;

		gtk_window_get_size (GTK_WINDOW (event_editor), &width, &height);

		if (g_settings_get_int (settings, "editor-event-window-width") > width) {
			width = g_settings_get_int (settings, "editor-event-window-width");
			gtk_window_resize (GTK_WINDOW (event_editor), width, height);
		}

		g_settings_bind (
			settings, "editor-event-day-agenda-paned-position",
			paned, "position",
			G_SETTINGS_BIND_DEFAULT);
	}

	e_extensible_load_extensions (E_EXTENSIBLE (comp_editor));

	e_ui_manager_thaw (ui_manager);

	e_signal_connect_notify_object (event_editor->priv->day_column, "notify::visible",
		G_CALLBACK (e_comp_editor_event_day_column_visible_notify_cb), event_editor, 0);
}

static void
e_comp_editor_event_dispose (GObject *object)
{
	ECompEditorEvent *self = E_COMP_EDITOR_EVENT (object);

	g_clear_object (&self->priv->day_column_vadjustment);

	if (self->priv->day_column_layout_id) {
		g_source_remove (self->priv->day_column_layout_id);
		self->priv->day_column_layout_id = 0;
	}

	G_OBJECT_CLASS (e_comp_editor_event_parent_class)->dispose (object);
}

static void
e_comp_editor_event_init (ECompEditorEvent *event_editor)
{
	event_editor->priv = e_comp_editor_event_get_instance_private (event_editor);

	g_warn_if_fail (gdk_rgba_parse (&event_editor->priv->bg_rgba_freetime, "#11ee11"));
	g_warn_if_fail (gdk_rgba_parse (&event_editor->priv->bg_rgba_clash, "#ee1111"));

	event_editor->priv->bg_rgba_freetime.alpha = 0.3;
	event_editor->priv->bg_rgba_clash.alpha = event_editor->priv->bg_rgba_freetime.alpha;
}

static void
e_comp_editor_event_class_init (ECompEditorEventClass *klass)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	ECompEditorClass *comp_editor_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->constructed = e_comp_editor_event_constructed;
	object_class->dispose = e_comp_editor_event_dispose;

	widget_class = GTK_WIDGET_CLASS (klass);
	widget_class->unmap = e_comp_editor_event_unmap;

	comp_editor_class = E_COMP_EDITOR_CLASS (klass);
	comp_editor_class->help_section = "calendar-usage-add-appointment";
	comp_editor_class->title_format_with_attendees = _("Meeting — %s");
	comp_editor_class->title_format_without_attendees = _("Appointment — %s");
	comp_editor_class->icon_name = "appointment-new";
	comp_editor_class->sensitize_widgets = ece_event_sensitize_widgets;
	comp_editor_class->fill_widgets = ece_event_fill_widgets;
	comp_editor_class->fill_component = ece_event_fill_component;
}
