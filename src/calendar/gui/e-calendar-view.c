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
 *		Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <string.h>
#include <time.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gdk/gdkkeysyms.h>
#include <libebackend/libebackend.h>

#include <shell/e-shell.h>

#include "comp-util.h"
#include "ea-cal-view.h"
#include "ea-calendar.h"
#include "e-cal-dialogs.h"
#include "e-cal-list-view.h"
#include "e-cal-model-calendar.h"
#include "e-cal-ops.h"
#include "e-calendar-view.h"
#include "e-day-view.h"
#include "e-month-view.h"
#include "itip-utils.h"
#include "misc.h"
#include "print.h"

#define E_CALENDAR_VIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_CALENDAR_VIEW, ECalendarViewPrivate))

struct _ECalendarViewPrivate {
	/* The calendar model we are monitoring */
	ECalModel *model;

	gint time_divisions;
	GSList *selected_cut_list;

	GtkTargetList *copy_target_list;
	GtkTargetList *paste_target_list;

	/* All keyboard devices are grabbed
	 * while a tooltip window is shown. */
	GQueue grabbed_keyboards;

	gboolean allow_direct_summary_edit;
};

enum {
	PROP_0,
	PROP_COPY_TARGET_LIST,
	PROP_MODEL,
	PROP_PASTE_TARGET_LIST,
	PROP_TIME_DIVISIONS,
	PROP_IS_EDITING,
	PROP_ALLOW_DIRECT_SUMMARY_EDIT
};

/* FIXME Why are we emitting these event signals here? Can't the model just be listened to? */
/* Signal IDs */
enum {
	POPUP_EVENT,
	SELECTION_CHANGED,
	SELECTED_TIME_CHANGED,
	TIMEZONE_CHANGED,
	EVENT_CHANGED,
	EVENT_ADDED,
	OPEN_EVENT,
	MOVE_VIEW_RANGE,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void calendar_view_selectable_init (ESelectableInterface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (
	ECalendarView, e_calendar_view, GTK_TYPE_GRID,
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL)
	G_IMPLEMENT_INTERFACE (E_TYPE_SELECTABLE, calendar_view_selectable_init));

static void
calendar_view_add_retract_data (ECalComponent *comp,
                                const gchar *retract_comment,
                                ECalObjModType mod)
{
	ICalComponent *icomp;
	ICalProperty *prop;

	icomp = e_cal_component_get_icalcomponent (comp);
	if (retract_comment && *retract_comment)
		prop = i_cal_property_new_x (retract_comment);
	else
		prop = i_cal_property_new_x ("0");
	i_cal_property_set_x_name (prop, "X-EVOLUTION-RETRACT-COMMENT");
	i_cal_component_take_property (icomp, prop);

	if (mod == E_CAL_OBJ_MOD_ALL)
		prop = i_cal_property_new_x ("All");
	else
		prop = i_cal_property_new_x ("This");
	i_cal_property_set_x_name (prop, "X-EVOLUTION-RECUR-MOD");
	i_cal_component_take_property (icomp, prop);
}

static gboolean
calendar_view_check_for_retract (ECalComponent *comp,
                                 ECalClient *client)
{
	ECalComponentOrganizer *organizer;
	const gchar *strip;
	gchar *email = NULL;
	gboolean ret_val;

	if (!e_cal_component_has_attendees (comp))
		return FALSE;

	if (!e_cal_client_check_save_schedules (client))
		return FALSE;

	organizer = e_cal_component_get_organizer (comp);
	if (!organizer)
		return FALSE;

	strip = itip_strip_mailto (e_cal_component_organizer_get_value (organizer));

	ret_val =
		e_client_get_backend_property_sync (E_CLIENT (client), E_CAL_BACKEND_PROPERTY_CAL_EMAIL_ADDRESS, &email, NULL, NULL) &&
		(g_ascii_strcasecmp (email, strip) == 0);

	g_free (email);

	e_cal_component_organizer_free (organizer);

	return ret_val;
}

static void
calendar_view_delete_event (ECalendarView *cal_view,
                            ECalendarViewEvent *event,
			    gboolean only_occurrence)
{
	ECalModel *model;
	ECalComponent *comp;
	ECalComponentVType vtype;
	ESourceRegistry *registry;
	gboolean delete = TRUE;

	if (!is_comp_data_valid (event))
		return;

	model = e_calendar_view_get_model (cal_view);
	registry = e_cal_model_get_registry (model);

	comp = e_cal_component_new ();
	e_cal_component_set_icalcomponent (comp, i_cal_component_new_clone (event->comp_data->icalcomp));
	vtype = e_cal_component_get_vtype (comp);

	/*FIXME remove it once the we dont set the recurrence id for all the generated instances */
	if (!only_occurrence && !e_cal_client_check_recurrences_no_master (event->comp_data->client))
		e_cal_component_set_recurid (comp, NULL);

	/*FIXME Retract should be moved to Groupwise features plugin */
	if (calendar_view_check_for_retract (comp, event->comp_data->client)) {
		gchar *retract_comment = NULL;
		gboolean retract = FALSE;

		delete = e_cal_dialogs_prompt_retract (GTK_WIDGET (cal_view), comp, &retract_comment, &retract);
		if (retract) {
			ICalComponent *icomp;

			calendar_view_add_retract_data (comp, retract_comment, E_CAL_OBJ_MOD_ALL);
			icomp = e_cal_component_get_icalcomponent (comp);
			i_cal_component_set_method (icomp, I_CAL_METHOD_CANCEL);

			e_cal_ops_send_component (model, event->comp_data->client, icomp);
		}
	} else if (e_cal_model_get_confirm_delete (model))
		delete = e_cal_dialogs_delete_component (
			comp, FALSE, 1, vtype, GTK_WIDGET (cal_view));

	if (delete) {
		const gchar *uid;
		gchar *rid;

		rid = e_cal_component_get_recurid_as_string (comp);

		if (itip_has_any_attendees (comp) &&
		    (itip_organizer_is_user (registry, comp, event->comp_data->client) ||
		     itip_sentby_is_user (registry, comp, event->comp_data->client))
		    && e_cal_dialogs_cancel_component ((GtkWindow *) gtk_widget_get_toplevel (GTK_WIDGET (cal_view)),
						event->comp_data->client,
						comp, TRUE)) {
			if (only_occurrence && !e_cal_component_is_instance (comp)) {
				ECalComponentRange *range;
				ECalComponentDateTime *dtstart;

				dtstart = e_cal_component_get_dtstart (comp);
				i_cal_time_set_is_date (e_cal_component_datetime_get_value (dtstart), 1);

				/* set the recurrence ID of the object we send */
				range = e_cal_component_range_new_take (E_CAL_COMPONENT_RANGE_SINGLE, dtstart);
				e_cal_component_set_recurid (comp, range);

				e_cal_component_range_free (range);
			}

			itip_send_component_with_model (model, E_CAL_COMPONENT_METHOD_CANCEL,
				comp, event->comp_data->client, NULL, NULL,
				NULL, TRUE, FALSE, FALSE);
		}

		uid = e_cal_component_get_uid (comp);
		if (!uid || !*uid) {
			g_object_unref (comp);
			g_free (rid);
			return;
		}

		if (only_occurrence) {
			if (e_cal_component_is_instance (comp)) {
				e_cal_ops_remove_component (model, event->comp_data->client, uid, rid, E_CAL_OBJ_MOD_THIS, FALSE);
			} else {
				ICalTime *instance_rid;
				ICalTimezone *zone = NULL;
				ECalComponentDateTime *dt;

				dt = e_cal_component_get_dtstart (comp);

				if (dt && e_cal_component_datetime_get_tzid (dt)) {
					GError *local_error = NULL;

					if (!e_cal_client_get_timezone_sync (event->comp_data->client,
						e_cal_component_datetime_get_tzid (dt), &zone, NULL, &local_error))
						zone = NULL;

					if (local_error != NULL) {
						zone = e_calendar_view_get_timezone (cal_view);
						g_clear_error (&local_error);
					}
				} else {
					zone = e_calendar_view_get_timezone (cal_view);
				}

				e_cal_component_datetime_free (dt);

				instance_rid = i_cal_time_from_timet_with_zone (
					event->comp_data->instance_start,
					TRUE, zone ? zone : i_cal_timezone_get_utc_timezone ());
				e_cal_util_remove_instances (event->comp_data->icalcomp, instance_rid, E_CAL_OBJ_MOD_THIS);
				e_cal_ops_modify_component (model, event->comp_data->client, event->comp_data->icalcomp,
					E_CAL_OBJ_MOD_THIS, E_CAL_OPS_SEND_FLAG_DONT_SEND);

				g_clear_object (&instance_rid);
			}
		} else if (e_cal_util_component_is_instance (event->comp_data->icalcomp) ||
			   e_cal_util_component_has_recurrences (event->comp_data->icalcomp))
			e_cal_ops_remove_component (model, event->comp_data->client, uid, rid, E_CAL_OBJ_MOD_ALL, FALSE);
		else
			e_cal_ops_remove_component (model, event->comp_data->client, uid, NULL, E_CAL_OBJ_MOD_THIS, FALSE);

		g_free (rid);
	}

	g_object_unref (comp);
}

static void
calendar_view_set_model (ECalendarView *calendar_view,
                         ECalModel *model)
{
	g_return_if_fail (calendar_view->priv->model == NULL);
	g_return_if_fail (E_IS_CAL_MODEL (model));

	calendar_view->priv->model = g_object_ref (model);
}

static void
calendar_view_set_property (GObject *object,
                            guint property_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_MODEL:
			calendar_view_set_model (
				E_CALENDAR_VIEW (object),
				g_value_get_object (value));
			return;

		case PROP_TIME_DIVISIONS:
			e_calendar_view_set_time_divisions (
				E_CALENDAR_VIEW (object),
				g_value_get_int (value));
			return;

		case PROP_ALLOW_DIRECT_SUMMARY_EDIT:
			e_calendar_view_set_allow_direct_summary_edit (
				E_CALENDAR_VIEW (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
calendar_view_get_property (GObject *object,
                            guint property_id,
                            GValue *value,
                            GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_COPY_TARGET_LIST:
			g_value_set_boxed (
				value, e_calendar_view_get_copy_target_list (
				E_CALENDAR_VIEW (object)));
			return;

		case PROP_MODEL:
			g_value_set_object (
				value, e_calendar_view_get_model (
				E_CALENDAR_VIEW (object)));
			return;

		case PROP_PASTE_TARGET_LIST:
			g_value_set_boxed (
				value, e_calendar_view_get_paste_target_list (
				E_CALENDAR_VIEW (object)));
			return;

		case PROP_TIME_DIVISIONS:
			g_value_set_int (
				value, e_calendar_view_get_time_divisions (
				E_CALENDAR_VIEW (object)));
			return;

		case PROP_IS_EDITING:
			g_value_set_boolean (value, e_calendar_view_is_editing (E_CALENDAR_VIEW (object)));
			return;

		case PROP_ALLOW_DIRECT_SUMMARY_EDIT:
			g_value_set_boolean (value, e_calendar_view_get_allow_direct_summary_edit (E_CALENDAR_VIEW (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
calendar_view_dispose (GObject *object)
{
	ECalendarViewPrivate *priv;

	priv = E_CALENDAR_VIEW_GET_PRIVATE (object);

	if (priv->model != NULL) {
		g_signal_handlers_disconnect_matched (
			priv->model, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, object);
		g_object_unref (priv->model);
		priv->model = NULL;
	}

	if (priv->copy_target_list != NULL) {
		gtk_target_list_unref (priv->copy_target_list);
		priv->copy_target_list = NULL;
	}

	if (priv->paste_target_list != NULL) {
		gtk_target_list_unref (priv->paste_target_list);
		priv->paste_target_list = NULL;
	}

	if (priv->selected_cut_list) {
		g_slist_foreach (priv->selected_cut_list, (GFunc) g_object_unref, NULL);
		g_slist_free (priv->selected_cut_list);
		priv->selected_cut_list = NULL;
	}

	while (!g_queue_is_empty (&priv->grabbed_keyboards)) {
		GdkDevice *keyboard;
		keyboard = g_queue_pop_head (&priv->grabbed_keyboards);
		gdk_device_ungrab (keyboard, GDK_CURRENT_TIME);
		g_object_unref (keyboard);
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_calendar_view_parent_class)->dispose (object);
}

static void
calendar_view_constructed (GObject *object)
{
	/* Do this after calendar_view_init() so extensions can query
	 * the GType accurately.  See GInstanceInitFunc documentation
	 * for details of the problem. */
	e_extensible_load_extensions (E_EXTENSIBLE (object));

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_calendar_view_parent_class)->constructed (object);
}

static void
calendar_view_update_actions (ESelectable *selectable,
                              EFocusTracker *focus_tracker,
                              GdkAtom *clipboard_targets,
                              gint n_clipboard_targets)
{
	ECalendarView *view;
	GtkAction *action;
	GtkTargetList *target_list;
	GList *list, *iter;
	gboolean can_paste = FALSE;
	gboolean sources_are_editable = TRUE;
	gboolean recurring = FALSE;
	gboolean is_editing;
	gboolean sensitive;
	const gchar *tooltip;
	gint n_selected;
	gint ii;

	view = E_CALENDAR_VIEW (selectable);
	is_editing = e_calendar_view_is_editing (view);

	list = e_calendar_view_get_selected_events (view);
	n_selected = g_list_length (list);

	for (iter = list; iter != NULL; iter = iter->next) {
		ECalendarViewEvent *event = iter->data;
		ECalClient *client;
		ICalComponent *icomp;

		if (event == NULL || event->comp_data == NULL)
			continue;

		client = event->comp_data->client;
		icomp = event->comp_data->icalcomp;

		sources_are_editable = sources_are_editable && !e_client_is_readonly (E_CLIENT (client));

		recurring |=
			e_cal_util_component_is_instance (icomp) ||
			e_cal_util_component_has_recurrences (icomp);
	}

	g_list_free (list);

	target_list = e_selectable_get_paste_target_list (selectable);
	for (ii = 0; ii < n_clipboard_targets && !can_paste; ii++)
		can_paste = gtk_target_list_find (
			target_list, clipboard_targets[ii], NULL);

	action = e_focus_tracker_get_cut_clipboard_action (focus_tracker);
	sensitive = (n_selected > 0) && sources_are_editable && !is_editing;
	tooltip = _("Cut selected events to the clipboard");
	gtk_action_set_sensitive (action, sensitive);
	gtk_action_set_tooltip (action, tooltip);

	action = e_focus_tracker_get_copy_clipboard_action (focus_tracker);
	sensitive = (n_selected > 0) && !is_editing;
	tooltip = _("Copy selected events to the clipboard");
	gtk_action_set_sensitive (action, sensitive);
	gtk_action_set_tooltip (action, tooltip);

	action = e_focus_tracker_get_paste_clipboard_action (focus_tracker);
	sensitive = sources_are_editable && can_paste && !is_editing;
	tooltip = _("Paste events from the clipboard");
	gtk_action_set_sensitive (action, sensitive);
	gtk_action_set_tooltip (action, tooltip);

	action = e_focus_tracker_get_delete_selection_action (focus_tracker);
	sensitive = (n_selected > 0) && sources_are_editable && !recurring && !is_editing;
	tooltip = _("Delete selected events");
	gtk_action_set_sensitive (action, sensitive);
	gtk_action_set_tooltip (action, tooltip);
}

static void
calendar_view_cut_clipboard (ESelectable *selectable)
{
	ECalendarView *cal_view;
	ECalendarViewPrivate *priv;
	GList *selected, *l;

	cal_view = E_CALENDAR_VIEW (selectable);
	priv = cal_view->priv;

	selected = e_calendar_view_get_selected_events (cal_view);
	if (!selected)
		return;

	e_selectable_copy_clipboard (selectable);

	for (l = selected; l != NULL; l = g_list_next (l)) {
		ECalendarViewEvent *event = (ECalendarViewEvent *) l->data;

		priv->selected_cut_list = g_slist_prepend (priv->selected_cut_list, g_object_ref (event->comp_data));
	}

	g_list_free (selected);
}

static void
add_related_timezones (ICalComponent *des_icomp,
		       ICalComponent *src_icomp,
		       ECalClient *client)
{
	ICalPropertyKind look_in[] = {
		I_CAL_DTSTART_PROPERTY,
		I_CAL_DTEND_PROPERTY,
		I_CAL_NO_PROPERTY
	};
	gint ii;

	g_return_if_fail (des_icomp != NULL);
	g_return_if_fail (src_icomp != NULL);
	g_return_if_fail (client != NULL);

	for (ii = 0; look_in[ii] != I_CAL_NO_PROPERTY; ii++) {
		ICalProperty *prop = i_cal_component_get_first_property (src_icomp, look_in[ii]);

		if (prop) {
			ICalParameter *par = i_cal_property_get_first_parameter (prop, I_CAL_TZID_PARAMETER);

			if (par) {
				const gchar *tzid = i_cal_parameter_get_tzid (par);

				if (tzid) {
					GError *error = NULL;
					ICalTimezone *zone = NULL;

					if (!e_cal_client_get_timezone_sync (client, tzid, &zone, NULL, &error))
						zone = NULL;
					if (error != NULL) {
						g_warning (
							"%s: Cannot get timezone for '%s'. %s",
							G_STRFUNC, tzid, error->message);
						g_error_free (error);
					} else if (zone) {
						ICalTimezone *existing_zone;

						/* do not duplicate timezones in the component */
						existing_zone = i_cal_component_get_timezone (des_icomp, i_cal_timezone_get_tzid (zone));
						if (existing_zone) {
							g_object_unref (existing_zone);
						} else {
							ICalComponent *vtz_comp;

							vtz_comp = i_cal_timezone_get_component (zone);
							if (vtz_comp) {
								i_cal_component_take_component (des_icomp, i_cal_component_new_clone (vtz_comp));
								g_object_unref (vtz_comp);
							}
						}
					}
				}

				g_object_unref (par);
			}

			g_object_unref (prop);
		}
	}
}

static void
calendar_view_copy_clipboard (ESelectable *selectable)
{
	ECalendarView *cal_view;
	ECalendarViewPrivate *priv;
	GList *selected, *l;
	gchar *comp_str;
	ICalComponent *vcal_comp;
	ECalendarViewEvent *event;
	GtkClipboard *clipboard;

	cal_view = E_CALENDAR_VIEW (selectable);
	priv = cal_view->priv;

	selected = e_calendar_view_get_selected_events (cal_view);
	if (!selected)
		return;

	if (priv->selected_cut_list) {
		g_slist_foreach (priv->selected_cut_list, (GFunc) g_object_unref, NULL);
		g_slist_free (priv->selected_cut_list);
		priv->selected_cut_list = NULL;
	}

	/* create top-level VCALENDAR component and add VTIMEZONE's */
	vcal_comp = e_cal_util_new_top_level ();
	for (l = selected; l != NULL; l = l->next) {
		event = (ECalendarViewEvent *) l->data;

		if (event && is_comp_data_valid (event)) {
			e_cal_util_add_timezones_from_component (vcal_comp, event->comp_data->icalcomp);

			add_related_timezones (vcal_comp, event->comp_data->icalcomp, event->comp_data->client);
		}
	}

	for (l = selected; l != NULL; l = l->next) {
		ICalComponent *new_icomp;

		event = (ECalendarViewEvent *) l->data;

		if (!is_comp_data_valid (event))
			continue;

		new_icomp = i_cal_component_new_clone (event->comp_data->icalcomp);

		/* do not remove RECURRENCE-IDs from copied objects */
		i_cal_component_take_component (vcal_comp, new_icomp);
	}

	comp_str = i_cal_component_as_ical_string_r (vcal_comp);

	/* copy the VCALENDAR to the clipboard */
	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
	e_clipboard_set_calendar (clipboard, comp_str, -1);
	gtk_clipboard_store (clipboard);

	/* free memory */
	g_object_unref (vcal_comp);
	g_free (comp_str);
	g_list_free (selected);
}

static void
calendar_view_component_created_cb (ECalModel *model,
				    ECalClient *client,
				    ICalComponent *original_icomp,
				    const gchar *new_uid,
				    gpointer user_data)
{
	gboolean strip_alarms = TRUE;
	ECalComponent *comp;
	ESourceRegistry *registry;
	GtkWidget *toplevel = user_data;

	comp = e_cal_component_new_from_icalcomponent (i_cal_component_new_clone (original_icomp));
	g_return_if_fail (comp != NULL);

	registry = e_cal_model_get_registry (model);

	if (new_uid)
		e_cal_component_set_uid (comp, new_uid);

	if (itip_has_any_attendees (comp) &&
	    (itip_organizer_is_user (registry, comp, client) ||
	     itip_sentby_is_user (registry, comp, client)) &&
	     e_cal_dialogs_send_component ((GtkWindow *) toplevel, client, comp, TRUE, &strip_alarms, NULL)) {
		itip_send_component_with_model (model, E_CAL_COMPONENT_METHOD_REQUEST,
			comp, client, NULL, NULL, NULL, strip_alarms, FALSE, FALSE);
	}

	g_object_unref (comp);
}

static void
e_calendar_view_add_event_sync (ECalModel *model,
				ECalClient *client,
				time_t dtstart,
				ICalTimezone *default_zone,
				ICalComponent *icomp,
				gboolean all_day,
				gboolean is_day_view,
				gint time_division,
				GtkWidget *top_level)
{
	ECalComponent *comp;
	ICalTime *itime, *btime, *old_dtstart, *old_dtend;
	ICalDuration *ic_dur, *ic_oneday;
	ICalTimezone *old_dtstart_zone;
	time_t tt_start, tt_end, new_dtstart = 0;
	gchar *uid;
	gint start_offset, end_offset;
	gboolean all_day_event = FALSE;

	start_offset = 0;
	end_offset = 0;

	old_dtstart = i_cal_component_get_dtstart (icomp);
	tt_start = i_cal_time_as_timet (old_dtstart);
	old_dtend = i_cal_component_get_dtend (icomp);
	tt_end = i_cal_time_as_timet (old_dtend);
	ic_dur = i_cal_duration_from_int (tt_end - tt_start);

	if (i_cal_duration_as_int (ic_dur) > 60 * 60 * 24) {
		/* This is a long event */
		start_offset = i_cal_time_get_hour (old_dtstart) * 60 + i_cal_time_get_minute (old_dtstart);
		end_offset = i_cal_time_get_hour (old_dtstart) * 60 + i_cal_time_get_minute (old_dtend);
	}

	ic_oneday = i_cal_duration_null_duration ();
	i_cal_duration_set_days (ic_oneday, 1);

	old_dtstart_zone = i_cal_time_get_timezone (old_dtstart);
	if (!old_dtstart_zone)
		old_dtstart_zone = default_zone;

	if (is_day_view) {
		if (start_offset == 0 && end_offset == 0 && all_day)
			all_day_event = TRUE;

		if (all_day_event) {
			g_clear_object (&ic_dur);
			ic_dur = g_object_ref (ic_oneday);
		} else if (i_cal_duration_as_int (ic_dur) >= 60 * 60 * 24 && !all_day) {
			g_clear_object (&ic_dur);
			/* copy & paste from top canvas to main canvas */
			ic_dur = i_cal_duration_from_int (time_division * 60);
		}

		if (all_day)
			new_dtstart = dtstart + start_offset * 60;
		else
			new_dtstart = dtstart;
	} else {
		if (i_cal_time_is_date (old_dtstart) && i_cal_time_is_date (old_dtend) &&
		    i_cal_duration_as_int (ic_dur) == i_cal_duration_as_int (ic_oneday)) {
			all_day_event = TRUE;
			new_dtstart = dtstart;
		} else {
			ICalTime *new_time = i_cal_time_from_timet_with_zone (dtstart, FALSE, default_zone);

			i_cal_time_set_hour (new_time, i_cal_time_get_hour (old_dtstart));
			i_cal_time_set_minute (new_time, i_cal_time_get_minute (old_dtstart));
			i_cal_time_set_second (new_time, i_cal_time_get_second (old_dtstart));

			new_dtstart = i_cal_time_as_timet_with_zone (new_time, old_dtstart_zone);

			g_clear_object (&new_time);
		}
	}

	itime = i_cal_time_from_timet_with_zone (new_dtstart, FALSE, old_dtstart_zone);
	/* set the timezone properly */
	i_cal_time_set_timezone (itime, old_dtstart_zone);
	if (all_day_event)
		i_cal_time_set_is_date (itime, TRUE);
	i_cal_component_set_dtstart (icomp, itime);

	i_cal_time_set_is_date (itime, FALSE);
	btime = i_cal_time_add (itime, ic_dur);
	if (all_day_event)
		i_cal_time_set_is_date (itime, TRUE);
	i_cal_component_set_dtend (icomp, itime);

	g_clear_object (&itime);
	g_clear_object (&btime);
	g_clear_object (&old_dtstart);
	g_clear_object (&old_dtend);
	g_clear_object (&ic_dur);
	g_clear_object (&ic_oneday);

	/* The new uid stuff can go away once we actually set it in the backend */
	uid = e_util_generate_uid ();
	comp = e_cal_component_new_from_icalcomponent (i_cal_component_new_clone (icomp));
	e_cal_component_set_uid (comp, uid);
	g_free (uid);

	e_cal_component_commit_sequence (comp);

	e_cal_ops_create_component (model, client, e_cal_component_get_icalcomponent (comp),
		calendar_view_component_created_cb, g_object_ref (top_level), g_object_unref);

	g_object_unref (comp);
}

typedef struct {
	ECalendarView *cal_view;
	GSList *selected_cut_list; /* ECalModelComponent * */
	GSList *copied_uids; /* gchar * */
	gchar *ical_str;
	time_t selection_start;
	time_t selection_end;
	gboolean is_day_view;
	gint time_division;
	GtkWidget *top_level;
	gboolean success;
	ECalClient *client;
} PasteClipboardData;

static void
paste_clipboard_data_free (gpointer ptr)
{
	PasteClipboardData *pcd = ptr;

	if (pcd) {
		if (pcd->success && pcd->copied_uids && pcd->selected_cut_list) {
			ECalModel *model;
			ESourceRegistry *registry;
			GSList *link;

			model = e_calendar_view_get_model (pcd->cal_view);
			registry = e_cal_model_get_registry (model);

			for (link = pcd->selected_cut_list; link != NULL; link = g_slist_next (link)) {
				ECalModelComponent *comp_data = (ECalModelComponent *) link->data;
				ECalComponent *comp;
				const gchar *uid;
				GSList *found = NULL;

				/* Remove them one by one after ensuring it has been copied to the destination successfully */
				found = g_slist_find_custom (pcd->copied_uids, i_cal_component_get_uid (comp_data->icalcomp), (GCompareFunc) strcmp);
				if (!found)
					continue;

				g_free (found->data);
				pcd->copied_uids = g_slist_delete_link (pcd->copied_uids, found);

				comp = e_cal_component_new_from_icalcomponent (i_cal_component_new_clone (comp_data->icalcomp));

				if (itip_has_any_attendees (comp) &&
				    (itip_organizer_is_user (registry, comp, comp_data->client) ||
				    itip_sentby_is_user (registry, comp, comp_data->client))
				    && e_cal_dialogs_cancel_component ((GtkWindow *) pcd->top_level, comp_data->client, comp, TRUE))
					itip_send_component_with_model (model, E_CAL_COMPONENT_METHOD_CANCEL,
						comp, comp_data->client, NULL, NULL, NULL, TRUE, FALSE, TRUE);

				uid = e_cal_component_get_uid (comp);
				if (e_cal_component_is_instance (comp)) {
					gchar *rid = NULL;

					/* when cutting detached instances, only cut that instance */
					rid = e_cal_component_get_recurid_as_string (comp);
					e_cal_ops_remove_component (model, comp_data->client, uid, rid, E_CAL_OBJ_MOD_THIS, TRUE);
					g_free (rid);
				} else {
					e_cal_ops_remove_component (model, comp_data->client, uid, NULL, E_CAL_OBJ_MOD_ALL, FALSE);
				}

				g_object_unref (comp);
			}
		}

		if (pcd->success && pcd->client) {
			ECalModel *model;

			model = e_calendar_view_get_model (pcd->cal_view);
			e_cal_model_emit_object_created (model, pcd->client);
		}

		g_clear_object (&pcd->cal_view);
		g_clear_object (&pcd->top_level);
		g_clear_object (&pcd->client);
		g_slist_free_full (pcd->selected_cut_list, g_object_unref);
		g_slist_free_full (pcd->copied_uids, g_free);
		g_free (pcd->ical_str);
		g_free (pcd);
	}
}

static void
cal_view_paste_clipboard_thread (EAlertSinkThreadJobData *job_data,
				 gpointer user_data,
				 GCancellable *cancellable,
				 GError **error)
{
	PasteClipboardData *pcd = user_data;
	ICalComponent *icomp;
	ICalComponentKind kind;
	ICalTimezone *default_zone;
	ECalModel *model;
	ESourceRegistry *registry;
	ESource *source = NULL, *default_source = NULL;
	EClientCache *client_cache;
	EClient *e_client;
	ECalClient *client = NULL;
	const gchar *message;
	const gchar *extension_name;
	gchar *display_name;
	guint copied_components = 1;
	gboolean all_day;
	GError *local_error = NULL;

	g_return_if_fail (pcd != NULL);

	icomp = i_cal_parser_parse_string (pcd->ical_str);
	if (!icomp) {
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
			_("Pasted text doesn’t contain valid iCalendar data"));
		return;
	}

	model = e_calendar_view_get_model (pcd->cal_view);
	registry = e_cal_model_get_registry (model);

	switch (e_cal_model_get_component_kind (model)) {
		case I_CAL_VEVENT_COMPONENT:
			default_source = e_source_registry_ref_default_calendar (registry);
			extension_name = E_SOURCE_EXTENSION_CALENDAR;
			message = _("Default calendar not found");
			break;
		case I_CAL_VJOURNAL_COMPONENT:
			default_source = e_source_registry_ref_default_memo_list (registry);
			extension_name = E_SOURCE_EXTENSION_MEMO_LIST;
			message = _("Default memo list not found");
			break;
		case I_CAL_VTODO_COMPONENT:
			default_source = e_source_registry_ref_default_task_list (registry);
			extension_name = E_SOURCE_EXTENSION_TASK_LIST;
			message = _("Default task list not found");
			break;
		default:
			g_warn_if_reached ();
			goto out;
	}

	source = e_source_registry_ref_source (registry, e_cal_model_get_default_source_uid (model));
	if (!source) {
		source = default_source;
		default_source = NULL;
	}

	if (!source) {
		const gchar *default_source_uid = e_cal_model_get_default_source_uid (model);

		e_alert_sink_thread_job_set_alert_arg_0 (job_data, default_source_uid ? default_source_uid : "");
		g_set_error_literal (&local_error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND, message);

		return;
	}

	display_name = e_util_get_source_full_name (registry, source);
	e_alert_sink_thread_job_set_alert_arg_0 (job_data, display_name);
	g_free (display_name);
	client_cache = e_cal_model_get_client_cache (model);

	e_client = e_client_cache_get_client_sync (client_cache, source, extension_name, 30, cancellable, &local_error);
	if (!e_client) {
		e_util_propagate_open_source_job_error (job_data, extension_name, local_error, error);
		goto out;
	}

	client = E_CAL_CLIENT (e_client);
	kind = i_cal_component_isa (icomp);
	default_zone = e_cal_model_get_timezone (model);
	all_day = pcd->selection_end - pcd->selection_start == 60 * 60 * 24;
	copied_components = 0;

	if (kind == I_CAL_VCALENDAR_COMPONENT) {
		ICalComponent *subcomp;

		/* add timezones first, to have them ready */
		for (subcomp = i_cal_component_get_first_component (icomp, I_CAL_VTIMEZONE_COMPONENT);
		     subcomp;
		     g_object_unref (subcomp), subcomp = i_cal_component_get_next_component (icomp, I_CAL_VTIMEZONE_COMPONENT)) {
			ICalTimezone *zone;

			zone = i_cal_timezone_new ();
			i_cal_timezone_set_component (zone, i_cal_component_new_clone (subcomp));

			if (!e_cal_client_add_timezone_sync (client, zone, cancellable, error)) {
				g_object_unref (subcomp);
				g_object_unref (zone);
				goto out;
			}

			g_object_unref (zone);
		}

		for (subcomp = i_cal_component_get_first_component (icomp, I_CAL_VEVENT_COMPONENT);
		     subcomp;
		     g_object_unref (subcomp), subcomp = i_cal_component_get_next_component (icomp, I_CAL_VEVENT_COMPONENT)) {
			if (e_cal_util_component_has_recurrences (subcomp)) {
				ICalProperty *prop = i_cal_component_get_first_property (subcomp, I_CAL_RRULE_PROPERTY);
				if (prop) {
					i_cal_property_remove_parameter_by_name (prop, "X-EVOLUTION-ENDDATE");
					g_object_unref (prop);
				}
			}

			e_calendar_view_add_event_sync (model, client, pcd->selection_start, default_zone, subcomp, all_day,
				pcd->is_day_view, pcd->time_division, pcd->top_level);

			copied_components++;
			if (pcd->selected_cut_list)
				pcd->copied_uids = g_slist_prepend (pcd->copied_uids, g_strdup (i_cal_component_get_uid (subcomp)));
		}
	} else if (kind == e_cal_model_get_component_kind (model)) {
		e_calendar_view_add_event_sync (model, client, pcd->selection_start, default_zone, icomp, all_day,
			pcd->is_day_view, pcd->time_division, pcd->top_level);

		copied_components++;
		if (pcd->selected_cut_list)
			pcd->copied_uids = g_slist_prepend (pcd->copied_uids, g_strdup (i_cal_component_get_uid (icomp)));
	}

	pcd->success = !g_cancellable_is_cancelled (cancellable);
	pcd->client = g_object_ref (client);

 out:
	if (!copied_components && !g_cancellable_is_cancelled (cancellable) && error && !*error)
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT, _("No suitable component found"));

	g_clear_object (&icomp);
	g_clear_object (&source);
	g_clear_object (&default_source);
	g_clear_object (&client);
}

static void
calendar_view_paste_clipboard (ESelectable *selectable)
{
	ECalModel *model;
	ECalendarView *cal_view;
	GtkClipboard *clipboard;

	cal_view = E_CALENDAR_VIEW (selectable);

	model = e_calendar_view_get_model (cal_view);

	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);

	/* Paste text into an event being edited. */
	if (gtk_clipboard_wait_is_text_available (clipboard)) {
		ECalendarViewClass *class;

		class = E_CALENDAR_VIEW_GET_CLASS (cal_view);
		g_return_if_fail (class->paste_text != NULL);

		class->paste_text (cal_view);

	/* Paste iCalendar data into the view. */
	} else if (e_clipboard_wait_is_calendar_available (clipboard)) {
		PasteClipboardData *pcd;
		ECalDataModel *data_model;
		GCancellable *cancellable;
		const gchar *alert_ident = NULL;

		switch (e_cal_model_get_component_kind (model)) {
			case ICAL_VEVENT_COMPONENT:
				alert_ident = "calendar:failed-create-event";
				break;
			case ICAL_VJOURNAL_COMPONENT:
				alert_ident = "calendar:failed-create-memo";
				break;
			case ICAL_VTODO_COMPONENT:
				alert_ident = "calendar:failed-create-task";
				break;
			default:
				g_warn_if_reached ();
				return;
		}

		pcd = g_new0 (PasteClipboardData, 1);
		pcd->cal_view = g_object_ref (cal_view);
		pcd->selected_cut_list = cal_view->priv->selected_cut_list;
		cal_view->priv->selected_cut_list = NULL;
		pcd->copied_uids = NULL; /* gchar * */
		pcd->ical_str = e_clipboard_wait_for_calendar (clipboard);
		g_warn_if_fail (e_calendar_view_get_selected_time_range (cal_view, &pcd->selection_start, &pcd->selection_end));
		pcd->is_day_view = E_IS_DAY_VIEW (cal_view);
		if (pcd->is_day_view)
			pcd->time_division = e_calendar_view_get_time_divisions (cal_view);
		pcd->top_level = gtk_widget_get_toplevel (GTK_WIDGET (cal_view));
		if (pcd->top_level)
			g_object_ref (pcd->top_level);
		pcd->success = FALSE;
		pcd->client = NULL;

		data_model = e_cal_model_get_data_model (model);

		cancellable = e_cal_data_model_submit_thread_job (data_model, _("Pasting iCalendar data"), alert_ident,
			NULL, cal_view_paste_clipboard_thread, pcd, paste_clipboard_data_free);

		g_clear_object (&cancellable);
	}
}

static void
calendar_view_delete_selection (ESelectable *selectable)
{
	ECalendarView *cal_view;
	GList *selected, *iter;

	cal_view = E_CALENDAR_VIEW (selectable);

	selected = e_calendar_view_get_selected_events (cal_view);

	for (iter = selected; iter != NULL; iter = iter->next) {
		ECalendarViewEvent *event = iter->data;

		/* XXX Why would this ever be NULL? */
		if (event == NULL)
			continue;

		calendar_view_delete_event (cal_view, event, FALSE);
	}

	g_list_free (selected);
}

static void
e_calendar_view_class_init (ECalendarViewClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GtkBindingSet *binding_set;

	g_type_class_add_private (class, sizeof (ECalendarViewPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = calendar_view_set_property;
	object_class->get_property = calendar_view_get_property;
	object_class->dispose = calendar_view_dispose;
	object_class->constructed = calendar_view_constructed;

	class->selection_changed = NULL;
	class->selected_time_changed = NULL;
	class->event_changed = NULL;
	class->event_added = NULL;

	class->get_selected_events = NULL;
	class->get_selected_time_range = NULL;
	class->set_selected_time_range = NULL;
	class->get_visible_time_range = NULL;
	class->precalc_visible_time_range = NULL;
	class->update_query = NULL;
	class->open_event = e_calendar_view_open_event;
	class->paste_text = NULL;

	/* Inherited from ESelectableInterface */
	g_object_class_override_property (
		object_class,
		PROP_COPY_TARGET_LIST,
		"copy-target-list");

	g_object_class_install_property (
		object_class,
		PROP_MODEL,
		g_param_spec_object (
			"model",
			"Model",
			NULL,
			E_TYPE_CAL_MODEL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

	/* Inherited from ESelectableInterface */
	g_object_class_override_property (
		object_class,
		PROP_PASTE_TARGET_LIST,
		"paste-target-list");

	g_object_class_install_property (
		object_class,
		PROP_TIME_DIVISIONS,
		g_param_spec_int (
			"time-divisions",
			"Time Divisions",
			NULL,
			G_MININT,
			G_MAXINT,
			30,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_IS_EDITING,
		g_param_spec_boolean (
			"is-editing",
			"Whether is in an editing mode",
			"Whether is in an editing mode",
			FALSE,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_ALLOW_DIRECT_SUMMARY_EDIT,
		g_param_spec_boolean (
			"allow-direct-summary-edit",
			"Whether can edit event Summary directly",
			NULL,
			FALSE,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	signals[POPUP_EVENT] = g_signal_new (
		"popup-event",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (ECalendarViewClass, popup_event),
		NULL, NULL,
		g_cclosure_marshal_VOID__BOXED,
		G_TYPE_NONE, 1,
		GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

	signals[SELECTION_CHANGED] = g_signal_new (
		"selection-changed",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ECalendarViewClass, selection_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[SELECTED_TIME_CHANGED] = g_signal_new (
		"selected-time-changed",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ECalendarViewClass, selected_time_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[TIMEZONE_CHANGED] = g_signal_new (
		"timezone-changed",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ECalendarViewClass, timezone_changed),
		NULL, NULL,
		e_marshal_VOID__OBJECT_OBJECT,
		G_TYPE_NONE, 2,
		I_CAL_TYPE_TIMEZONE,
		I_CAL_TYPE_TIMEZONE);

	signals[EVENT_CHANGED] = g_signal_new (
		"event-changed",
		G_TYPE_FROM_CLASS (object_class),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (ECalendarViewClass, event_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1,
		G_TYPE_POINTER);

	signals[EVENT_ADDED] = g_signal_new (
		"event-added",
		G_TYPE_FROM_CLASS (object_class),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (ECalendarViewClass, event_added),
		NULL, NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1,
		G_TYPE_POINTER);

	signals[OPEN_EVENT] = g_signal_new (
		"open-event",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (ECalendarViewClass, open_event),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[MOVE_VIEW_RANGE] = g_signal_new (
		"move-view-range",
		G_TYPE_FROM_CLASS (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ECalendarViewClass, move_view_range),
		NULL, NULL,
		NULL, /* default marshal */
		G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_INT64);

	/* Key bindings */

	binding_set = gtk_binding_set_by_class (class);

	gtk_binding_entry_add_signal (
		binding_set, GDK_KEY_o, GDK_CONTROL_MASK, "open-event", 0);

	/* init the accessibility support for e_day_view */
	widget_class = GTK_WIDGET_CLASS (class);
	gtk_widget_class_set_accessible_type (widget_class, EA_TYPE_CAL_VIEW);
}

static void
e_calendar_view_init (ECalendarView *calendar_view)
{
	GtkTargetList *target_list;

	calendar_view->priv = E_CALENDAR_VIEW_GET_PRIVATE (calendar_view);

	/* Set this early to avoid a divide-by-zero during init. */
	calendar_view->priv->time_divisions = 30;

	target_list = gtk_target_list_new (NULL, 0);
	e_target_list_add_calendar_targets (target_list, 0);
	calendar_view->priv->copy_target_list = target_list;

	target_list = gtk_target_list_new (NULL, 0);
	e_target_list_add_calendar_targets (target_list, 0);
	calendar_view->priv->paste_target_list = target_list;
}

static void
calendar_view_selectable_init (ESelectableInterface *iface)
{
	iface->update_actions = calendar_view_update_actions;
	iface->cut_clipboard = calendar_view_cut_clipboard;
	iface->copy_clipboard = calendar_view_copy_clipboard;
	iface->paste_clipboard = calendar_view_paste_clipboard;
	iface->delete_selection = calendar_view_delete_selection;
}

void
e_calendar_view_popup_event (ECalendarView *calendar_view,
                             GdkEvent *button_event)
{
	g_return_if_fail (E_IS_CALENDAR_VIEW (calendar_view));
	g_return_if_fail (button_event != NULL);

	g_signal_emit (calendar_view, signals[POPUP_EVENT], 0, button_event);
}

ECalModel *
e_calendar_view_get_model (ECalendarView *cal_view)
{
	g_return_val_if_fail (E_IS_CALENDAR_VIEW (cal_view), NULL);

	return cal_view->priv->model;
}

ICalTimezone *
e_calendar_view_get_timezone (ECalendarView *cal_view)
{
	g_return_val_if_fail (E_IS_CALENDAR_VIEW (cal_view), NULL);

	return e_cal_model_get_timezone (cal_view->priv->model);
}

void
e_calendar_view_set_timezone (ECalendarView *cal_view,
			      const ICalTimezone *zone)
{
	ICalTimezone *old_zone;

	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	old_zone = e_cal_model_get_timezone (cal_view->priv->model);
	if (old_zone == zone)
		return;

	if (old_zone)
		g_object_ref (old_zone);

	e_cal_model_set_timezone (cal_view->priv->model, zone);
	g_signal_emit (
		cal_view, signals[TIMEZONE_CHANGED], 0,
		old_zone, zone);

	g_clear_object (&old_zone);
}

GtkTargetList *
e_calendar_view_get_copy_target_list (ECalendarView *cal_view)
{
	g_return_val_if_fail (E_IS_CALENDAR_VIEW (cal_view), NULL);

	return cal_view->priv->copy_target_list;
}

GtkTargetList *
e_calendar_view_get_paste_target_list (ECalendarView *cal_view)
{
	g_return_val_if_fail (E_IS_CALENDAR_VIEW (cal_view), NULL);

	return cal_view->priv->paste_target_list;
}

gint
e_calendar_view_get_time_divisions (ECalendarView *cal_view)
{
	g_return_val_if_fail (E_IS_CALENDAR_VIEW (cal_view), 0);

	return cal_view->priv->time_divisions;
}

void
e_calendar_view_set_time_divisions (ECalendarView *cal_view,
                                    gint time_divisions)
{
	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	if (cal_view->priv->time_divisions == time_divisions)
		return;

	cal_view->priv->time_divisions = time_divisions;

	g_object_notify (G_OBJECT (cal_view), "time-divisions");
}

GList *
e_calendar_view_get_selected_events (ECalendarView *cal_view)
{
	ECalendarViewClass *class;

	g_return_val_if_fail (E_IS_CALENDAR_VIEW (cal_view), NULL);

	class = E_CALENDAR_VIEW_GET_CLASS (cal_view);
	g_return_val_if_fail (class->get_selected_events != NULL, NULL);

	return class->get_selected_events (cal_view);
}

gboolean
e_calendar_view_get_selected_time_range (ECalendarView *cal_view,
                                         time_t *start_time,
                                         time_t *end_time)
{
	ECalendarViewClass *class;

	g_return_val_if_fail (E_IS_CALENDAR_VIEW (cal_view), FALSE);

	class = E_CALENDAR_VIEW_GET_CLASS (cal_view);
	g_return_val_if_fail (class->get_selected_time_range != NULL, FALSE);

	return class->get_selected_time_range (cal_view, start_time, end_time);
}

void
e_calendar_view_set_selected_time_range (ECalendarView *cal_view,
                                         time_t start_time,
                                         time_t end_time)
{
	ECalendarViewClass *class;

	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	/* Not all views implement this, so return silently. */
	class = E_CALENDAR_VIEW_GET_CLASS (cal_view);
	if (class->set_selected_time_range == NULL)
		return;

	class->set_selected_time_range (cal_view, start_time, end_time);
}

gboolean
e_calendar_view_get_visible_time_range (ECalendarView *cal_view,
                                        time_t *start_time,
                                        time_t *end_time)
{
	ECalendarViewClass *class;

	g_return_val_if_fail (E_IS_CALENDAR_VIEW (cal_view), FALSE);

	class = E_CALENDAR_VIEW_GET_CLASS (cal_view);
	g_return_val_if_fail (class->get_visible_time_range != NULL, FALSE);

	return class->get_visible_time_range (cal_view, start_time, end_time);
}

void
e_calendar_view_precalc_visible_time_range (ECalendarView *cal_view,
					    time_t in_start_time,
					    time_t in_end_time,
					    time_t *out_start_time,
					    time_t *out_end_time)
{
	ECalendarViewClass *class;

	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	/* Not all views implement this, so return silently. */
	class = E_CALENDAR_VIEW_GET_CLASS (cal_view);
	if (class->precalc_visible_time_range == NULL)
		return;

	class->precalc_visible_time_range (cal_view, in_start_time, in_end_time, out_start_time, out_end_time);
}

void
e_calendar_view_update_query (ECalendarView *cal_view)
{
	ECalendarViewClass *class;

	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	class = E_CALENDAR_VIEW_GET_CLASS (cal_view);
	g_return_if_fail (class->update_query != NULL);

	class->update_query (cal_view);
}

void
e_calendar_view_delete_selected_occurrence (ECalendarView *cal_view)
{
	ECalendarViewEvent *event;
	GList *selected;

	selected = e_calendar_view_get_selected_events (cal_view);
	if (!selected)
		return;

	event = (ECalendarViewEvent *) selected->data;
	if (is_comp_data_valid (event)) {
		calendar_view_delete_event (cal_view, event, TRUE);
	}

	g_list_free (selected);
}

void
e_calendar_view_open_event (ECalendarView *cal_view)
{
	GList *selected;

	selected = e_calendar_view_get_selected_events (cal_view);
	if (selected) {
		ECalendarViewEvent *event = (ECalendarViewEvent *) selected->data;
		if (event && is_comp_data_valid (event))
			e_calendar_view_edit_appointment (cal_view, event->comp_data->client, event->comp_data->icalcomp, EDIT_EVENT_AUTODETECT);

		g_list_free (selected);
	}
}

/**
 * e_calendar_view_new_appointment
 * @cal_view: an #ECalendarView
 * @flags: bit-or of ENewAppointmentFlags
 *
 * Opens an event editor dialog for a new appointment. The appointment's
 * start and end times are set to the currently selected time range in
 * the calendar view, unless the flags contain E_NEW_APPOINTMENT_FLAG_FORCE_CURRENT_TIME,
 * in which case the current time is used.
 *
 * When the selection is for all day and we don't need all day event,
 * then this does a rounding to the actual hour for actual day (today) and
 * to the 'day begins' from preferences in other selected day.
 */
void
e_calendar_view_new_appointment (ECalendarView *cal_view,
				 guint32 flags)
{
	ECalModel *model;
	time_t dtstart, dtend, now;
	gboolean do_rounding = FALSE;
	gboolean all_day = (flags & E_NEW_APPOINTMENT_FLAG_ALL_DAY) != 0;
	gboolean meeting = (flags & E_NEW_APPOINTMENT_FLAG_MEETING) != 0;
	gboolean no_past_date = (flags & E_NEW_APPOINTMENT_FLAG_NO_PAST_DATE) != 0;

	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	model = e_calendar_view_get_model (cal_view);

	now = time (NULL);

	if ((flags & E_NEW_APPOINTMENT_FLAG_FORCE_CURRENT_TIME) != 0 ||
	    !e_calendar_view_get_selected_time_range (cal_view, &dtstart, &dtend)) {
		dtstart = now;
		dtend = dtstart + 3600;
	}

	if (no_past_date && dtstart <= now) {
		dtend = time_day_begin (now) + (dtend - dtstart);
		dtstart = time_day_begin (now);
		do_rounding = TRUE;
	}

	/* We either need rounding or don't want to set all_day for this, we will rather use actual */
	/* time in this cases; dtstart should be a midnight in this case */
	if (do_rounding || (!all_day && (dtend - dtstart) == (60 * 60 * 24))) {
		struct tm local = *localtime (&now);
		gint time_div = e_calendar_view_get_time_divisions (cal_view);
		gint hours, mins;

		if (!time_div) /* Possible if your settings values aren't so nice */
			time_div = 30;

		if (time_day_begin (now) == time_day_begin (dtstart)) {
			/* same day as today */
			hours = local.tm_hour;
			mins = local.tm_min;

			/* round minutes to nearest time division, up or down */
			if ((mins % time_div) >= time_div / 2)
				mins += time_div;
			mins = (mins - (mins % time_div));
		} else {
			/* other day than today */
			hours = e_cal_model_get_work_day_start_hour (model);
			mins = e_cal_model_get_work_day_start_minute (model);
		}

		dtstart = dtstart + (60 * 60 * hours) + (mins * 60);
		if (no_past_date && dtstart <= now)
			dtstart += ((((now - dtstart) / 60 / time_div)) + time_div) * 60;
		dtend = dtstart + (time_div * 60);
	}

	e_cal_ops_new_component_editor_from_model (
		e_calendar_view_get_model (cal_view), NULL,
		dtstart, dtend, meeting, all_day);
}

/* Ensures the calendar is selected */
static void
object_created_cb (ECompEditor *comp_editor,
                   ECalendarView *cal_view)
{
	e_cal_model_emit_object_created (e_calendar_view_get_model (cal_view), e_comp_editor_get_target_client (comp_editor));
}

ECompEditor *
e_calendar_view_open_event_with_flags (ECalendarView *cal_view,
				       ECalClient *client,
				       ICalComponent *icomp,
				       guint32 flags)
{
	ECompEditor *comp_editor;
	EShell *shell;

	/* FIXME ECalendarView should own an EShell pointer. */
	shell = e_shell_get_default ();

	comp_editor = e_comp_editor_find_existing_for (e_client_get_source (E_CLIENT (client)), icomp);
	if (!comp_editor) {
		GtkWidget *toplevel;

		toplevel = gtk_widget_get_toplevel (GTK_WIDGET (cal_view));
		if (!GTK_IS_WINDOW (toplevel))
			toplevel = NULL;

		comp_editor = e_comp_editor_open_for_component (GTK_WINDOW (toplevel),
			shell, e_client_get_source (E_CLIENT (client)), icomp, flags);

		g_signal_connect (
			comp_editor, "object-created",
			G_CALLBACK (object_created_cb), cal_view);
	}

	gtk_window_present (GTK_WINDOW (comp_editor));

	return comp_editor;
}

/**
 * e_calendar_view_edit_appointment
 * @cal_view: A calendar view.
 * @client: Calendar client.
 * @icomp: The object to be edited.
 * @mode: one of #EEditEventMode
 *
 * Opens an editor window to allow the user to edit the selected
 * object.
 */
void
e_calendar_view_edit_appointment (ECalendarView *cal_view,
				  ECalClient *client,
				  ICalComponent *icomp,
				  EEditEventMode mode)
{
	ECalModel *model;
	ESourceRegistry *registry;
	guint32 flags = 0;

	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));
	g_return_if_fail (E_IS_CAL_CLIENT (client));
	g_return_if_fail (icomp != NULL);

	model = e_calendar_view_get_model (cal_view);
	registry = e_cal_model_get_registry (model);

	if ((mode == EDIT_EVENT_AUTODETECT && e_cal_util_component_has_attendee (icomp))
	    || mode == EDIT_EVENT_FORCE_MEETING) {
		ECalComponent *comp = e_cal_component_new_from_icalcomponent (i_cal_component_new_clone (icomp));
		flags |= E_COMP_EDITOR_FLAG_WITH_ATTENDEES;
		if (itip_organizer_is_user (registry, comp, client) ||
		    itip_sentby_is_user (registry, comp, client) ||
		    !e_cal_component_has_attendees (comp))
			flags |= E_COMP_EDITOR_FLAG_ORGANIZER_IS_USER;
		g_object_unref (comp);
	}

	e_calendar_view_open_event_with_flags (cal_view, client, icomp, flags);
}

static void
tooltip_ungrab (ECalendarView *view,
		guint32 event_time)
{
	GdkDevice *keyboard;

	while (!g_queue_is_empty (&view->priv->grabbed_keyboards)) {
		keyboard = g_queue_pop_head (&view->priv->grabbed_keyboards);
		gdk_device_ungrab (keyboard, event_time);
		g_object_unref (keyboard);
	}
}

static gboolean
tooltip_key_event (GtkWidget *tooltip,
		   GdkEvent *key_event,
		   ECalendarView *view)
{
	GtkWidget *widget;

	widget = g_object_get_data (G_OBJECT (view), "tooltip-window");
	if (widget == NULL)
		return TRUE;

	tooltip_ungrab (view, gdk_event_get_time (key_event));

	gtk_widget_destroy (widget);
	g_object_set_data (G_OBJECT (view), "tooltip-window", NULL);

	return FALSE;
}

static gchar *
get_label (ICalTime *tt,
	   ICalTimezone *f_zone,
	   ICalTimezone *t_zone)
{
	struct tm tmp_tm;

	tmp_tm = e_cal_util_icaltime_to_tm_with_zone (tt, f_zone, t_zone);

	return e_datetime_format_format_tm ("calendar", "table", i_cal_time_is_date (tt) ? DTFormatKindDate : DTFormatKindDateTime, &tmp_tm);
}

void
e_calendar_view_move_tip (GtkWidget *widget,
                          gint x,
                          gint y)
{
	GtkAllocation allocation;
	GtkRequisition requisition;
	GdkDisplay *display;
	GdkScreen *screen;
	GdkScreen *pointer_screen;
	GdkRectangle monitor;
	GdkDeviceManager *device_manager;
	GdkDevice *pointer;
	gint monitor_num, px, py;
	gint w, h;

	gtk_widget_get_preferred_size (widget, &requisition, NULL);
	w = requisition.width;
	h = requisition.height;

	screen = gtk_widget_get_screen (widget);
	display = gdk_screen_get_display (screen);
	device_manager = gdk_display_get_device_manager (display);
	pointer = gdk_device_manager_get_client_pointer (device_manager);

	gdk_device_get_position (pointer, &pointer_screen, &px, &py);
	if (pointer_screen != screen) {
		px = x;
		py = y;
	}
	monitor_num = gdk_screen_get_monitor_at_point (screen, px, py);
	gdk_screen_get_monitor_geometry (screen, monitor_num, &monitor);

	if ((x + w) > monitor.x + monitor.width)
		x -= (x + w) - (monitor.x + monitor.width);
	else if (x < monitor.x)
		x = monitor.x;

	gtk_widget_get_allocation (widget, &allocation);

	if ((y + h + allocation.height + 4) > monitor.y + monitor.height)
		y = y - h - 36;

	gtk_window_move (GTK_WINDOW (widget), x, y);
	gtk_widget_show (widget);
}

static void
tooltip_window_destroyed_cb (gpointer user_data,
			     GObject *gone)
{
	ECalendarView *view = user_data;

	tooltip_ungrab (view, GDK_CURRENT_TIME);
	g_object_unref (view);
}

/*
 * It is expected to show the tooltips in this below format
 *
 *	<B>SUBJECT OF THE MEETING</B>
 *	Organiser: NameOfTheUser<email@ofuser.com>
 *	Location: PlaceOfTheMeeting
 *	Time : DateAndTime (xx Minutes)
 *      Status: Accepted: X   Declined: Y   ...
 */

gboolean
e_calendar_view_get_tooltips (const ECalendarViewEventData *data)
{
	GtkWidget *label, *box, *hbox, *ebox, *frame, *toplevel;
	gchar *tmp, *tmp1 = NULL, *tmp2 = NULL;
	ECalComponentOrganizer *organizer;
	ECalComponentDateTime *dtstart, *dtend;
	time_t t_start, t_end;
	ECalendarViewEvent *pevent;
	GtkWidget *widget;
	GdkWindow *window;
	GdkDisplay *display;
	GdkDeviceManager *device_manager;
	GdkRGBA bg_rgba, fg_rgba;
	GQueue *grabbed_keyboards;
	ECalComponent *newcomp;
	ICalTimezone *zone, *default_zone;
	ECalModel *model;
	ECalClient *client = NULL;
	GList *list, *link;

	/* This function is a timeout callback. */

	g_return_val_if_fail (data != NULL, FALSE);
	g_return_val_if_fail (E_IS_CALENDAR_VIEW (data->cal_view), FALSE);

	e_utils_get_theme_color (GTK_WIDGET (data->cal_view), "theme_selected_bg_color", E_UTILS_DEFAULT_THEME_SELECTED_BG_COLOR, &bg_rgba);
	e_utils_get_theme_color (GTK_WIDGET (data->cal_view), "theme_selected_fg_color", E_UTILS_DEFAULT_THEME_SELECTED_FG_COLOR, &fg_rgba);

	model = e_calendar_view_get_model (data->cal_view);

	/* Delete any stray tooltip if left */
	widget = g_object_get_data (
		G_OBJECT (data->cal_view), "tooltip-window");
	if (GTK_IS_WIDGET (widget))
		gtk_widget_destroy (widget);

	default_zone = e_calendar_view_get_timezone (data->cal_view);
	pevent = data->get_view_event (data->cal_view, data->day, data->event_num);

	if (!is_comp_data_valid (pevent))
		return FALSE;

	client = pevent->comp_data->client;

	newcomp = e_cal_component_new_from_icalcomponent (i_cal_component_new_clone (pevent->comp_data->icalcomp));
	if (!newcomp) {
		g_warning ("couldn't update calendar component with modified data from backend\n");
		return FALSE;
	}

	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

	tmp1 = e_calendar_view_dup_component_summary (pevent->comp_data->icalcomp);

	if (!(tmp1 && *tmp1)) {
		g_object_unref (newcomp);
		gtk_widget_destroy (box);
		g_free (tmp1);

		return FALSE;
	}

	tmp = g_markup_printf_escaped ("<b>%s</b>", tmp1);
	label = gtk_label_new (NULL);
	gtk_label_set_line_wrap ((GtkLabel *) label, TRUE);
	gtk_label_set_markup ((GtkLabel *) label, tmp);

	g_free (tmp1);
	tmp1 = NULL;

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start ((GtkBox *) hbox, label, FALSE, FALSE, 0);
	ebox = gtk_event_box_new ();
	gtk_container_add ((GtkContainer *) ebox, hbox);
	gtk_widget_override_background_color (ebox, GTK_STATE_FLAG_NORMAL, &bg_rgba);
	gtk_widget_override_color (label, GTK_STATE_FLAG_NORMAL, &fg_rgba);

	gtk_box_pack_start ((GtkBox *) box, ebox, FALSE, FALSE, 0);
	g_free (tmp);

	organizer = e_cal_component_get_organizer (newcomp);
	if (organizer && e_cal_component_organizer_get_cn (organizer)) {
		const gchar *email;

		email = itip_strip_mailto (e_cal_component_organizer_get_value (organizer));

		if (email) {
			/* To Translators: It will display "Organiser: NameOfTheUser <email@ofuser.com>" */
			tmp = g_strdup_printf (_("Organizer: %s <%s>"), e_cal_component_organizer_get_cn (organizer), email);
		} else {
			/* With SunOne accouts, there may be no ':' in organiser.value*/
			tmp = g_strdup_printf (_("Organizer: %s"), e_cal_component_organizer_get_cn (organizer));
		}

		label = gtk_label_new (tmp);
		hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
		gtk_box_pack_start ((GtkBox *) hbox, label, FALSE, FALSE, 0);
		ebox = gtk_event_box_new ();
		gtk_container_add ((GtkContainer *) ebox, hbox);
		gtk_box_pack_start ((GtkBox *) box, ebox, FALSE, FALSE, 0);

		g_free (tmp);
	}

	e_cal_component_organizer_free (organizer);

	tmp1 = e_cal_component_get_location (newcomp);

	if (tmp1) {
		/* Translators: It will display "Location: PlaceOfTheMeeting" */
		tmp = g_markup_printf_escaped (_("Location: %s"), tmp1);
		label = gtk_label_new (NULL);
		gtk_widget_set_halign (label, GTK_ALIGN_START);
		gtk_misc_set_alignment ((GtkMisc *) label, 0.0, 0.0);
		gtk_label_set_markup ((GtkLabel *) label, tmp);
		gtk_label_set_line_wrap ((GtkLabel *) label, TRUE);
		gtk_label_set_max_width_chars ((GtkLabel *) label, 80);
		hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
		gtk_box_pack_start ((GtkBox *) hbox, label, FALSE, FALSE, 0);
		ebox = gtk_event_box_new ();
		gtk_container_add ((GtkContainer *) ebox, hbox);
		gtk_box_pack_start ((GtkBox *) box, ebox, FALSE, FALSE, 0);
		g_free (tmp);
	}

	g_free (tmp1);
	tmp1 = NULL;

	dtstart = e_cal_component_get_dtstart (newcomp);
	dtend = e_cal_component_get_dtend (newcomp);

	if (dtstart && e_cal_component_datetime_get_tzid (dtstart)) {
		zone = i_cal_component_get_timezone (e_cal_component_get_icalcomponent (newcomp), e_cal_component_datetime_get_tzid (dtstart));
		if (!zone &&
		    !e_cal_client_get_timezone_sync (client, e_cal_component_datetime_get_tzid (dtstart), &zone, NULL, NULL))
			zone = NULL;

		if (!zone)
			zone = default_zone;

	} else {
		zone = NULL;
	}

	if (dtstart && e_cal_component_datetime_get_value (dtstart)) {
		t_start = i_cal_time_as_timet_with_zone (e_cal_component_datetime_get_value (dtstart), zone);
		if (dtend && e_cal_component_datetime_get_value (dtend))
			t_end = i_cal_time_as_timet_with_zone (e_cal_component_datetime_get_value (dtend), zone);
		else
			t_end = t_start;

		tmp1 = get_label (e_cal_component_datetime_get_value (dtstart), zone, default_zone);
		tmp = calculate_time (t_start, t_end);

		/* To Translators: It will display "Time: ActualStartDateAndTime (DurationOfTheMeeting)"*/
		tmp2 = g_strdup_printf (_("Time: %s %s"), tmp1, tmp);
		if (zone && !cal_comp_util_compare_event_timezones (newcomp, client, default_zone)) {
			g_free (tmp);
			g_free (tmp1);

			tmp1 = get_label (e_cal_component_datetime_get_value (dtstart), zone, zone);
			tmp = g_strconcat (tmp2, "\n\t[ ", tmp1, " ", i_cal_timezone_get_display_name (zone), " ]", NULL);
		} else {
			g_free (tmp);
			tmp = tmp2;
			tmp2 = NULL;
		}
	} else {
		tmp = NULL;
	}

	e_cal_component_datetime_free (dtstart);
	e_cal_component_datetime_free (dtend);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start ((GtkBox *) hbox, gtk_label_new_with_mnemonic (tmp), FALSE, FALSE, 0);
	ebox = gtk_event_box_new ();
	gtk_container_add ((GtkContainer *) ebox, hbox);
	gtk_box_pack_start ((GtkBox *) box, ebox, FALSE, FALSE, 0);

	g_free (tmp);
	g_free (tmp2);
	g_free (tmp1);

	tmp = e_cal_model_get_attendees_status_info (
		model, newcomp, pevent->comp_data->client);
	if (tmp) {
		hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
		gtk_box_pack_start ((GtkBox *) hbox, gtk_label_new (tmp), FALSE, FALSE, 0);
		ebox = gtk_event_box_new ();
		gtk_container_add ((GtkContainer *) ebox, hbox);
		gtk_box_pack_start ((GtkBox *) box, ebox, FALSE, FALSE, 0);

		g_free (tmp);
	}

	tmp = cal_comp_util_get_attendee_comments (e_cal_component_get_icalcomponent (newcomp));
	if (tmp) {
		hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
		gtk_box_pack_start ((GtkBox *) hbox, gtk_label_new (tmp), FALSE, FALSE, 0);
		ebox = gtk_event_box_new ();
		gtk_container_add ((GtkContainer *) ebox, hbox);
		gtk_box_pack_start ((GtkBox *) box, ebox, FALSE, FALSE, 0);

		g_free (tmp);
	}

	pevent->tooltip = gtk_window_new (GTK_WINDOW_POPUP);

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (data->cal_view));
	if (GTK_IS_WINDOW (toplevel)) {
		gtk_window_group_add_window (gtk_window_get_group (GTK_WINDOW (toplevel)), GTK_WINDOW (pevent->tooltip));
		gtk_window_set_transient_for (GTK_WINDOW (pevent->tooltip), GTK_WINDOW (toplevel));
	}

	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type ((GtkFrame *) frame, GTK_SHADOW_IN);

	gtk_window_set_type_hint (GTK_WINDOW (pevent->tooltip), GDK_WINDOW_TYPE_HINT_TOOLTIP);
	gtk_window_move ((GtkWindow *) pevent->tooltip, pevent->x +16, pevent->y + 16);
	gtk_container_add ((GtkContainer *) frame, box);
	gtk_container_add ((GtkContainer *) pevent->tooltip, frame);

	gtk_widget_show_all (pevent->tooltip);

	e_calendar_view_move_tip (pevent->tooltip, pevent->x +16, pevent->y + 16);

	/* Grab all keyboard devices.  A key press from
	 * any of them will dismiss the tooltip window. */

	window = gtk_widget_get_window (pevent->tooltip);
	display = gdk_window_get_display (window);
	device_manager = gdk_display_get_device_manager (display);

	grabbed_keyboards = &data->cal_view->priv->grabbed_keyboards;
	g_warn_if_fail (g_queue_is_empty (grabbed_keyboards));

	list = gdk_device_manager_list_devices (
		device_manager, GDK_DEVICE_TYPE_MASTER);

	for (link = list; link != NULL; link = g_list_next (link)) {
		GdkDevice *device = GDK_DEVICE (link->data);
		GdkGrabStatus grab_status;

		if (gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD)
			continue;

		grab_status = gdk_device_grab (
			device,
			window,
			GDK_OWNERSHIP_NONE,
			FALSE,
			GDK_KEY_PRESS_MASK |
			GDK_KEY_RELEASE_MASK,
			NULL,
			GDK_CURRENT_TIME);

		if (grab_status == GDK_GRAB_SUCCESS)
			g_queue_push_tail (
				grabbed_keyboards,
				g_object_ref (device));
	}

	g_list_free (list);

	g_signal_connect (
		pevent->tooltip, "key-press-event",
		G_CALLBACK (tooltip_key_event), data->cal_view);
	pevent->timeout = -1;

	g_object_weak_ref (G_OBJECT (pevent->tooltip), tooltip_window_destroyed_cb, g_object_ref (data->cal_view));
	g_object_set_data (G_OBJECT (data->cal_view), "tooltip-window", pevent->tooltip);
	g_object_unref (newcomp);

	return FALSE;
}

static gboolean
icomp_contains_category (ICalComponent *icomp,
			 const gchar *category)
{
	ICalProperty *prop;

	g_return_val_if_fail (icomp != NULL && category != NULL, FALSE);

	for (prop = i_cal_component_get_first_property (icomp, I_CAL_CATEGORIES_PROPERTY);
	     prop;
	     g_object_unref (prop), prop = i_cal_component_get_next_property (icomp, I_CAL_CATEGORIES_PROPERTY)) {
		const gchar *value = i_cal_property_get_categories (prop);

		if (g_strcmp0 (category, value) == 0) {
			g_object_unref (prop);
			return TRUE;
		}
	}

	return FALSE;
}

/* e_calendar_view_dup_component_summary returns summary of icomp,
 * and for type of birthday or anniversary it appends number of years since
 * beginning. Free the returned string with g_free(), when no longer needed.
 */
gchar *
e_calendar_view_dup_component_summary (ICalComponent *icomp)
{
	const gchar *summary;

	g_return_val_if_fail (icomp != NULL, NULL);

	summary = i_cal_component_get_summary (icomp);

	if (icomp_contains_category (icomp, _("Birthday")) ||
	    icomp_contains_category (icomp, _("Anniversary"))) {
		gchar *since_year_str;

		since_year_str = e_cal_util_component_dup_x_property (icomp, "X-EVOLUTION-SINCE-YEAR");

		if (since_year_str) {
			ICalTime *dtstart;
			gint since_year;
			gchar *res = NULL;

			since_year = atoi (since_year_str);

			dtstart = i_cal_component_get_dtstart (icomp);

			if (since_year > 0 && dtstart && i_cal_time_is_valid_time (dtstart) &&
			    i_cal_time_get_year (dtstart) - since_year > 0) {
				/* Translators: the '%s' stands for a component summary, the '%d' for the years.
				   The string is used for Birthday & Anniversary events where the first year is
				   know, constructing a summary which also shows how many years the birthday or
				   anniversary is for. Example: "Birthday: John Doe (13)" */
				summary = g_strdup_printf (C_("BirthdaySummary", "%s (%d)"), summary ? summary : "", i_cal_time_get_year (dtstart) - since_year);
			}

			g_clear_object (&dtstart);
			g_free (since_year_str);

			return res ? res : g_strdup (summary);
		}
	}

	return g_strdup (summary);
}

/* A callback for e_cal_ops_create_component(), whose @user_data is an ECalendarView instance */
void
e_calendar_view_component_created_cb (ECalModel *model,
				      ECalClient *client,
				      ICalComponent *original_icomp,
				      const gchar *new_uid,
				      gpointer user_data)
{
	ECalendarView *cal_view = user_data;

	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	e_cal_model_emit_object_created (model, client);
}

void
draw_curved_rectangle (cairo_t *cr,
                       gdouble x0,
                       gdouble y0,
                       gdouble rect_width,
                       gdouble rect_height,
                       gdouble radius)
{
	gdouble x1, y1;

	x1 = x0 + rect_width;
	y1 = y0 + rect_height;

	if (!rect_width || !rect_height)
	    return;
	if (rect_width / 2 < radius) {
	    if (rect_height / 2 < radius) {
		cairo_move_to  (cr, x0, (y0 + y1) / 2);
		cairo_curve_to (cr, x0 ,y0, x0, y0, (x0 + x1) / 2, y0);
		cairo_curve_to (cr, x1, y0, x1, y0, x1, (y0 + y1) / 2);
		cairo_curve_to (cr, x1, y1, x1, y1, (x1 + x0) / 2, y1);
		cairo_curve_to (cr, x0, y1, x0, y1, x0, (y0 + y1) / 2);
	    } else {
		cairo_move_to  (cr, x0, y0 + radius);
		cairo_curve_to (cr, x0 ,y0, x0, y0, (x0 + x1) / 2, y0);
		cairo_curve_to (cr, x1, y0, x1, y0, x1, y0 + radius);
		cairo_line_to (cr, x1 , y1 - radius);
		cairo_curve_to (cr, x1, y1, x1, y1, (x1 + x0) / 2, y1);
		cairo_curve_to (cr, x0, y1, x0, y1, x0, y1- radius);
		}
	} else {
	    if (rect_height / 2 < radius) {
		cairo_move_to  (cr, x0, (y0 + y1) / 2);
		cairo_curve_to (cr, x0 , y0, x0 , y0, x0 + radius, y0);
		cairo_line_to (cr, x1 - radius, y0);
		cairo_curve_to (cr, x1, y0, x1, y0, x1, (y0 + y1) / 2);
		cairo_curve_to (cr, x1, y1, x1, y1, x1 - radius, y1);
		cairo_line_to (cr, x0 + radius, y1);
		cairo_curve_to (cr, x0, y1, x0, y1, x0, (y0 + y1) / 2);
	    } else {
		cairo_move_to  (cr, x0, y0 + radius);
		cairo_curve_to (cr, x0 , y0, x0 , y0, x0 + radius, y0);
		cairo_line_to (cr, x1 - radius, y0);
		cairo_curve_to (cr, x1, y0, x1, y0, x1, y0 + radius);
		cairo_line_to (cr, x1 , y1 - radius);
		cairo_curve_to (cr, x1, y1, x1, y1, x1 - radius, y1);
		cairo_line_to (cr, x0 + radius, y1);
		cairo_curve_to (cr, x0, y1, x0, y1, x0, y1- radius);
		}
	}
	cairo_close_path (cr);
}

/* returns either light or dark yellow, based on the base_background,
 * which is the default background color */
GdkColor
get_today_background (const GdkColor base_background)
{
	GdkColor res = base_background;

	if (res.red > 0x7FFF) {
		/* light yellow for a light theme */
		res.red = 0xFFFF;
		res.green = 0xFFFF;
		res.blue = 0xC0C0;
	} else {
		/* dark yellow for a dark theme */
		res.red = 0x3F3F;
		res.green = 0x3F3F;
		res.blue = 0x0000;
	}

	return res;
}

gboolean
is_comp_data_valid_func (ECalendarViewEvent *event,
                         const gchar *location)
{
	g_return_val_if_fail (location != NULL, FALSE);

	if (!event) {
		g_warning ("%s: event is NULL", location);
		return FALSE;
	}

	if (!event->comp_data) {
		g_warning ("%s: event's (%p) comp_data is NULL", location, event);
		return FALSE;
	}

	return TRUE;
}

gboolean
is_array_index_in_bounds_func (GArray *array,
                               gint index,
                               const gchar *location)
{
	g_return_val_if_fail (location != NULL, FALSE);

	if (!array) {
		g_warning ("%s: array is NULL", location);
		return FALSE;
	}

	if (index < 0 || index >= array->len) {
		g_warning ("%s: index %d is out of bounds [0,%d) at array %p", location, index, array->len, array);
		return FALSE;
	}

	return TRUE;
}

gboolean
e_calendar_view_is_editing (ECalendarView *cal_view)
{
	static gboolean in = FALSE;
	gboolean is_editing = FALSE;

	g_return_val_if_fail (E_IS_CALENDAR_VIEW (cal_view), FALSE);

	/* this should be called from the main thread only,
	 * and each descendant overrides the property,
	 * thus might cause no call recursion */
	if (in) {
		g_warn_if_reached ();
		return FALSE;
	}

	in = TRUE;

	g_object_get (G_OBJECT (cal_view), "is-editing", &is_editing, NULL);

	in = FALSE;

	return is_editing;
}

/* Returns text description of the current view. */
gchar *
e_calendar_view_get_description_text (ECalendarView *cal_view)
{
	time_t start_time, end_time;
	struct tm start_tm, end_tm;
	ICalTime *tt;
	ICalTimezone *zone;
	gchar start_buffer[512] = { 0 };
	gchar end_buffer[512] = { 0 };

	g_return_val_if_fail (E_IS_CALENDAR_VIEW (cal_view), NULL);

	if (!e_calendar_view_get_visible_time_range (cal_view, &start_time, &end_time))
		return NULL;

	zone = e_cal_model_get_timezone (cal_view->priv->model);

	tt = i_cal_time_from_timet_with_zone (start_time, FALSE, zone);
	start_tm = e_cal_util_icaltime_to_tm (tt);
	g_clear_object (&tt);

	/* Subtract one from end_time so we don't get an extra day. */
	tt = i_cal_time_from_timet_with_zone (end_time - 1, FALSE, zone);
	end_tm = e_cal_util_icaltime_to_tm (tt);
	g_clear_object (&tt);

	if (E_IS_MONTH_VIEW (cal_view) || E_IS_CAL_LIST_VIEW (cal_view)) {
		if (start_tm.tm_year == end_tm.tm_year) {
			if (start_tm.tm_mon == end_tm.tm_mon) {
				e_utf8_strftime (start_buffer, sizeof (start_buffer), "%d", &start_tm);
				e_utf8_strftime (end_buffer, sizeof (end_buffer), _("%d %b %Y"), &end_tm);
			} else {
				e_utf8_strftime (start_buffer, sizeof (start_buffer), _("%d %b"), &start_tm);
				e_utf8_strftime (end_buffer, sizeof (end_buffer), _("%d %b %Y"), &end_tm);
			}
		} else {
			e_utf8_strftime (start_buffer, sizeof (start_buffer), _("%d %b %Y"), &start_tm);
			e_utf8_strftime (end_buffer, sizeof (end_buffer), _("%d %b %Y"), &end_tm);
		}
	} else {
		if (start_tm.tm_year == end_tm.tm_year &&
			start_tm.tm_mon == end_tm.tm_mon &&
			start_tm.tm_mday == end_tm.tm_mday) {
			e_utf8_strftime (start_buffer, sizeof (start_buffer), _("%A %d %b %Y"), &start_tm);
		} else if (start_tm.tm_year == end_tm.tm_year) {
			e_utf8_strftime (start_buffer, sizeof (start_buffer), _("%a %d %b"), &start_tm);
			e_utf8_strftime (end_buffer, sizeof (end_buffer), _("%a %d %b %Y"), &end_tm);
		} else {
			e_utf8_strftime (start_buffer, sizeof (start_buffer), _("%a %d %b %Y"), &start_tm);
			e_utf8_strftime (end_buffer, sizeof (end_buffer), _("%a %d %b %Y"), &end_tm);
		}
	}

	if (*start_buffer && *end_buffer)
		return g_strdup_printf ("%s - %s", start_buffer, end_buffer);

	return g_strdup_printf ("%s%s", start_buffer, end_buffer);
}

void
e_calendar_view_move_view_range (ECalendarView *cal_view,
				 ECalendarViewMoveType mode_type,
				 time_t exact_date)
{
	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	g_signal_emit (cal_view, signals[MOVE_VIEW_RANGE], 0, mode_type, (gint64) exact_date);
}

gboolean
e_calendar_view_get_allow_direct_summary_edit (ECalendarView *cal_view)
{
	g_return_val_if_fail (E_IS_CALENDAR_VIEW (cal_view), FALSE);

	return cal_view->priv->allow_direct_summary_edit;
}

void
e_calendar_view_set_allow_direct_summary_edit (ECalendarView *cal_view,
					       gboolean allow)
{
	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	if ((cal_view->priv->allow_direct_summary_edit ? 1 : 0) == (allow ? 1 : 0))
		return;

	cal_view->priv->allow_direct_summary_edit = allow;

	g_object_notify (G_OBJECT (cal_view), "allow-direct-summary-edit");
}
