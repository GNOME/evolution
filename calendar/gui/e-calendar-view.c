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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <time.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gdk/gdkkeysyms.h>
#include <libebackend/libebackend.h>

#include <shell/e-shell.h>

#include "comp-util.h"
#include "ea-calendar.h"
#include "e-cal-ops.h"
#include "e-cal-model-calendar.h"
#include "e-calendar-view.h"
#include "e-day-view.h"
#include "e-month-view.h"
#include "e-cal-list-view.h"
#include "ea-cal-view.h"
#include "itip-utils.h"
#include "dialogs/comp-editor-util.h"
#include "dialogs/delete-comp.h"
#include "dialogs/event-editor.h"
#include "dialogs/send-comp.h"
#include "dialogs/cancel-comp.h"
#include "dialogs/recur-comp.h"
#include "dialogs/select-source-dialog.h"
#include "dialogs/goto-dialog.h"
#include "print.h"
#include "misc.h"

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
};

enum {
	PROP_0,
	PROP_COPY_TARGET_LIST,
	PROP_MODEL,
	PROP_PASTE_TARGET_LIST,
	PROP_TIME_DIVISIONS,
	PROP_IS_EDITING
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
	ECalendarView, e_calendar_view, GTK_TYPE_TABLE,
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL)
	G_IMPLEMENT_INTERFACE (E_TYPE_SELECTABLE, calendar_view_selectable_init));

static void
calendar_view_add_retract_data (ECalComponent *comp,
                                const gchar *retract_comment,
                                ECalObjModType mod)
{
	icalcomponent *icalcomp = NULL;
	icalproperty *icalprop = NULL;

	icalcomp = e_cal_component_get_icalcomponent (comp);
	if (retract_comment && *retract_comment)
		icalprop = icalproperty_new_x (retract_comment);
	else
		icalprop = icalproperty_new_x ("0");
	icalproperty_set_x_name (icalprop, "X-EVOLUTION-RETRACT-COMMENT");
	icalcomponent_add_property (icalcomp, icalprop);

	if (mod == E_CAL_OBJ_MOD_ALL)
		icalprop = icalproperty_new_x ("All");
	else
		icalprop = icalproperty_new_x ("This");
	icalproperty_set_x_name (icalprop, "X-EVOLUTION-RECUR-MOD");
	icalcomponent_add_property (icalcomp, icalprop);
}

static gboolean
calendar_view_check_for_retract (ECalComponent *comp,
                                 ECalClient *client)
{
	ECalComponentOrganizer organizer;
	const gchar *strip;
	gchar *email = NULL;
	gboolean ret_val;

	if (!e_cal_component_has_attendees (comp))
		return FALSE;

	if (!e_cal_client_check_save_schedules (client))
		return FALSE;

	e_cal_component_get_organizer (comp, &organizer);
	strip = itip_strip_mailto (organizer.value);

	ret_val =
		e_client_get_backend_property_sync (E_CLIENT (client), CAL_BACKEND_PROPERTY_CAL_EMAIL_ADDRESS, &email, NULL, NULL) &&
		(g_ascii_strcasecmp (email, strip) == 0);

	g_free (email);

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
	e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (event->comp_data->icalcomp));
	vtype = e_cal_component_get_vtype (comp);

	/*FIXME remove it once the we dont set the recurrence id for all the generated instances */
	if (!only_occurrence && !e_cal_client_check_recurrences_no_master (event->comp_data->client))
		e_cal_component_set_recurid (comp, NULL);

	/*FIXME Retract should be moved to Groupwise features plugin */
	if (calendar_view_check_for_retract (comp, event->comp_data->client)) {
		gchar *retract_comment = NULL;
		gboolean retract = FALSE;

		delete = prompt_retract_dialog (comp, &retract_comment, GTK_WIDGET (cal_view), &retract);
		if (retract) {
			icalcomponent *icalcomp;

			calendar_view_add_retract_data (comp, retract_comment, E_CAL_OBJ_MOD_ALL);
			icalcomp = e_cal_component_get_icalcomponent (comp);
			icalcomponent_set_method (icalcomp, ICAL_METHOD_CANCEL);

			e_cal_ops_send_component (model, event->comp_data->client, icalcomp);
		}
	} else if (e_cal_model_get_confirm_delete (model))
		delete = delete_component_dialog (
			comp, FALSE, 1, vtype, GTK_WIDGET (cal_view));

	if (delete) {
		const gchar *uid;
		gchar *rid;

		rid = e_cal_component_get_recurid_as_string (comp);

		if ((itip_organizer_is_user (registry, comp, event->comp_data->client) ||
		     itip_sentby_is_user (registry, comp, event->comp_data->client))
		    && cancel_component_dialog ((GtkWindow *) gtk_widget_get_toplevel (GTK_WIDGET (cal_view)),
						event->comp_data->client,
						comp, TRUE)) {
			if (only_occurrence && !e_cal_component_is_instance (comp)) {
				ECalComponentRange range;

				/* set the recurrence ID of the object we send */
				range.type = E_CAL_COMPONENT_RANGE_SINGLE;
				e_cal_component_get_dtstart (comp, &range.datetime);
				range.datetime.value->is_date = 1;
				e_cal_component_set_recurid (comp, &range);

				e_cal_component_free_datetime (&range.datetime);
			}

			itip_send_component (model, E_CAL_COMPONENT_METHOD_CANCEL,
				comp, event->comp_data->client, NULL, NULL,
				NULL, TRUE, FALSE, FALSE);
		}

		e_cal_component_get_uid (comp, &uid);
		if (!uid || !*uid) {
			g_object_unref (comp);
			g_free (rid);
			return;
		}

		if (only_occurrence) {
			if (e_cal_component_is_instance (comp)) {
				e_cal_ops_remove_component (model, event->comp_data->client, uid, rid, E_CAL_OBJ_MOD_THIS, FALSE);
			} else {
				struct icaltimetype instance_rid;
				ECalComponentDateTime dt;
				icaltimezone *zone = NULL;

				e_cal_component_get_dtstart (comp, &dt);

				if (dt.tzid) {
					GError *local_error = NULL;

					e_cal_client_get_timezone_sync (event->comp_data->client, dt.tzid, &zone, NULL, &local_error);
					if (local_error != NULL) {
						zone = e_calendar_view_get_timezone (cal_view);
						g_clear_error (&local_error);
					}
				} else {
					zone = e_calendar_view_get_timezone (cal_view);
				}

				e_cal_component_free_datetime (&dt);

				instance_rid = icaltime_from_timet_with_zone (
					event->comp_data->instance_start,
					TRUE, zone ? zone : icaltimezone_get_utc_timezone ());
				e_cal_util_remove_instances (event->comp_data->icalcomp, instance_rid, E_CAL_OBJ_MOD_THIS);
				e_cal_ops_modify_component (model, event->comp_data->client, event->comp_data->icalcomp,
					E_CAL_OBJ_MOD_THIS, E_CAL_OPS_SEND_FLAG_DONT_SEND);
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
		icalcomponent *icalcomp;

		if (event == NULL || event->comp_data == NULL)
			continue;

		client = event->comp_data->client;
		icalcomp = event->comp_data->icalcomp;

		sources_are_editable = sources_are_editable && !e_client_is_readonly (E_CLIENT (client));

		recurring |=
			e_cal_util_component_is_instance (icalcomp) ||
			e_cal_util_component_has_recurrences (icalcomp);
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
add_related_timezones (icalcomponent *des_icalcomp,
                       icalcomponent *src_icalcomp,
                       ECalClient *client)
{
	icalproperty_kind look_in[] = {
		ICAL_DTSTART_PROPERTY,
		ICAL_DTEND_PROPERTY,
		ICAL_NO_PROPERTY
	};
	gint i;

	g_return_if_fail (des_icalcomp != NULL);
	g_return_if_fail (src_icalcomp != NULL);
	g_return_if_fail (client != NULL);

	for (i = 0; look_in[i] != ICAL_NO_PROPERTY; i++) {
		icalproperty *prop = icalcomponent_get_first_property (src_icalcomp, look_in[i]);

		if (prop) {
			icalparameter *par = icalproperty_get_first_parameter (prop, ICAL_TZID_PARAMETER);

			if (par) {
				const gchar *tzid = icalparameter_get_tzid (par);

				if (tzid) {
					GError *error = NULL;
					icaltimezone *zone = NULL;

					e_cal_client_get_timezone_sync (
						client, tzid, &zone, NULL, &error);
					if (error != NULL) {
						g_warning (
							"%s: Cannot get timezone for '%s'. %s",
							G_STRFUNC, tzid, error->message);
						g_error_free (error);
					} else if (zone &&
						icalcomponent_get_timezone (des_icalcomp, icaltimezone_get_tzid (zone)) == NULL) {
						/* do not duplicate timezones in the component */
						icalcomponent *vtz_comp;

						vtz_comp = icaltimezone_get_component (zone);
						if (vtz_comp)
							icalcomponent_add_component (des_icalcomp, icalcomponent_new_clone (vtz_comp));
					}
				}
			}
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
	icalcomponent *vcal_comp;
	icalcomponent *new_icalcomp;
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
		event = (ECalendarViewEvent *) l->data;

		if (!is_comp_data_valid (event))
			continue;

		new_icalcomp = icalcomponent_new_clone (event->comp_data->icalcomp);

		/* do not remove RECURRENCE-IDs from copied objects */
		icalcomponent_add_component (vcal_comp, new_icalcomp);
	}

	comp_str = icalcomponent_as_ical_string_r (vcal_comp);

	/* copy the VCALENDAR to the clipboard */
	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
	e_clipboard_set_calendar (clipboard, comp_str, -1);
	gtk_clipboard_store (clipboard);

	/* free memory */
	icalcomponent_free (vcal_comp);
	g_free (comp_str);
	g_list_free (selected);
}

static void
calendar_view_component_created_cb (ECalModel *model,
				    ECalClient *client,
				    icalcomponent *original_icalcomp,
				    const gchar *new_uid,
				    gpointer user_data)
{
	gboolean strip_alarms = TRUE;
	ECalComponent *comp;
	ESourceRegistry *registry;
	GtkWidget *toplevel = user_data;

	comp = e_cal_component_new_from_icalcomponent (icalcomponent_new_clone (original_icalcomp));
	g_return_if_fail (comp != NULL);

	registry = e_cal_model_get_registry (model);

	if (new_uid)
		e_cal_component_set_uid (comp, new_uid);

	if ((itip_organizer_is_user (registry, comp, client) ||
	     itip_sentby_is_user (registry, comp, client)) &&
	     send_component_dialog ((GtkWindow *) toplevel, client, comp, TRUE, &strip_alarms, NULL)) {
		itip_send_component (model, E_CAL_COMPONENT_METHOD_REQUEST,
			comp, client, NULL, NULL, NULL, strip_alarms, FALSE, FALSE);
	}

	g_object_unref (comp);
}

static void
e_calendar_view_add_event_sync (ECalModel *model,
				ECalClient *client,
				time_t dtstart,
				icaltimezone *default_zone,
				icalcomponent *icalcomp,
				gboolean all_day,
				gboolean is_day_view,
				gint time_division,
				GtkWidget *top_level)
{
	ECalComponent *comp;
	struct icaltimetype itime, old_dtstart, old_dtend;
	time_t tt_start, tt_end, new_dtstart = 0;
	struct icaldurationtype ic_dur, ic_oneday;
	gchar *uid;
	gint start_offset, end_offset;
	gboolean all_day_event = FALSE;

	start_offset = 0;
	end_offset = 0;

	old_dtstart = icalcomponent_get_dtstart (icalcomp);
	tt_start = icaltime_as_timet (old_dtstart);
	old_dtend = icalcomponent_get_dtend (icalcomp);
	tt_end = icaltime_as_timet (old_dtend);
	ic_dur = icaldurationtype_from_int (tt_end - tt_start);

	if (icaldurationtype_as_int (ic_dur) > 60 *60 *24) {
		/* This is a long event */
		start_offset = old_dtstart.hour * 60 + old_dtstart.minute;
		end_offset = old_dtstart.hour * 60 + old_dtend.minute;
	}

	ic_oneday = icaldurationtype_null_duration ();
	ic_oneday.days = 1;

	if (is_day_view) {
		if (start_offset == 0 && end_offset == 0 && all_day)
			all_day_event = TRUE;

		if (all_day_event) {
			ic_dur = ic_oneday;
		} else if (icaldurationtype_as_int (ic_dur) >= 60 *60 *24 && !all_day) {
			/* copy & paste from top canvas to main canvas */
			ic_dur = icaldurationtype_from_int (time_division * 60);
		}

		if (all_day)
			new_dtstart = dtstart + start_offset * 60;
		else
			new_dtstart = dtstart;
	} else {
		if (old_dtstart.is_date && old_dtend.is_date
			&& memcmp (&ic_dur, &ic_oneday, sizeof (ic_dur)) == 0) {
			all_day_event = TRUE;
			new_dtstart = dtstart;
		} else {
			icaltimetype new_time = icaltime_from_timet_with_zone (dtstart, FALSE, default_zone);

			new_time.hour = old_dtstart.hour;
			new_time.minute = old_dtstart.minute;
			new_time.second = old_dtstart.second;

			new_dtstart = icaltime_as_timet_with_zone (new_time, old_dtstart.zone ? old_dtstart.zone : default_zone);
		}
	}

	itime = icaltime_from_timet_with_zone (new_dtstart, FALSE, old_dtstart.zone ? old_dtstart.zone : default_zone);
	/* set the timezone properly */
	itime.zone = old_dtstart.zone ? old_dtstart.zone : default_zone;
	if (all_day_event)
		itime.is_date = TRUE;
	icalcomponent_set_dtstart (icalcomp, itime);

	itime.is_date = FALSE;
	itime = icaltime_add (itime, ic_dur);
	if (all_day_event)
		itime.is_date = TRUE;
	icalcomponent_set_dtend (icalcomp, itime);

	/* The new uid stuff can go away once we actually set it in the backend */
	uid = e_cal_component_gen_uid ();
	comp = e_cal_component_new ();
	e_cal_component_set_icalcomponent (
		comp, icalcomponent_new_clone (icalcomp));
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
				found = g_slist_find_custom (pcd->copied_uids, icalcomponent_get_uid (comp_data->icalcomp), (GCompareFunc) strcmp);
				if (!found)
					continue;

				g_free (found->data);
				pcd->copied_uids = g_slist_delete_link (pcd->copied_uids, found);

				comp = e_cal_component_new ();
				e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (comp_data->icalcomp));

				if ((itip_organizer_is_user (registry, comp, comp_data->client) ||
				    itip_sentby_is_user (registry, comp, comp_data->client))
				    && cancel_component_dialog ((GtkWindow *) pcd->top_level, comp_data->client, comp, TRUE))
					itip_send_component (model, E_CAL_COMPONENT_METHOD_CANCEL,
						comp, comp_data->client, NULL, NULL, NULL, TRUE, FALSE, TRUE);

				e_cal_component_get_uid (comp, &uid);
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
	icalcomponent *icalcomp;
	icalcomponent_kind kind;
	icaltimezone *default_zone;
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

	icalcomp = icalparser_parse_string (pcd->ical_str);
	if (!icalcomp) {
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
			_("Pasted text doesn't contain valid iCalendar data"));
		return;
	}

	model = e_calendar_view_get_model (pcd->cal_view);
	registry = e_cal_model_get_registry (model);

	switch (e_cal_model_get_component_kind (model)) {
		case ICAL_VEVENT_COMPONENT:
			default_source = e_source_registry_ref_default_calendar (registry);
			extension_name = E_SOURCE_EXTENSION_CALENDAR;
			message = _("Default calendar not found");
			break;
		case ICAL_VJOURNAL_COMPONENT:
			default_source = e_source_registry_ref_default_memo_list (registry);
			extension_name = E_SOURCE_EXTENSION_MEMO_LIST;
			message = _("Default memo list not found");
			break;
		case ICAL_VTODO_COMPONENT:
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
	kind = icalcomponent_isa (icalcomp);
	default_zone = e_cal_model_get_timezone (model);
	all_day = pcd->selection_end - pcd->selection_start == 60 * 60 * 24;
	copied_components = 0;

	if (kind == ICAL_VCALENDAR_COMPONENT) {
		icalcomponent *subcomp;

		/* add timezones first, to have them ready */
		for (subcomp = icalcomponent_get_first_component (icalcomp, ICAL_VTIMEZONE_COMPONENT);
		     subcomp;
		     subcomp = icalcomponent_get_next_component (icalcomp, ICAL_VTIMEZONE_COMPONENT)) {
			icaltimezone *zone;

			zone = icaltimezone_new ();
			icaltimezone_set_component (zone, subcomp);

			if (!e_cal_client_add_timezone_sync (client, zone, cancellable, error)) {
				icaltimezone_free (zone, 1);
				goto out;
			}

			icaltimezone_free (zone, 1);
		}

		for (subcomp = icalcomponent_get_first_component (icalcomp, ICAL_VEVENT_COMPONENT);
		     subcomp;
		     subcomp = icalcomponent_get_next_component (icalcomp, ICAL_VEVENT_COMPONENT)) {
			if (e_cal_util_component_has_recurrences (subcomp)) {
				icalproperty *icalprop = icalcomponent_get_first_property (subcomp, ICAL_RRULE_PROPERTY);
				if (icalprop)
					icalproperty_remove_parameter_by_name (icalprop, "X-EVOLUTION-ENDDATE");
			}

			e_calendar_view_add_event_sync (model, client, pcd->selection_start, default_zone, subcomp, all_day,
				pcd->is_day_view, pcd->time_division, pcd->top_level);

			copied_components++;
			if (pcd->selected_cut_list)
				pcd->copied_uids = g_slist_prepend (pcd->copied_uids, g_strdup (icalcomponent_get_uid (subcomp)));
		}
	} else if (kind == e_cal_model_get_component_kind (model)) {
		e_calendar_view_add_event_sync (model, client, pcd->selection_start, default_zone, icalcomp, all_day,
			pcd->is_day_view, pcd->time_division, pcd->top_level);

		copied_components++;
		if (pcd->selected_cut_list)
			pcd->copied_uids = g_slist_prepend (pcd->copied_uids, g_strdup (icalcomponent_get_uid (icalcomp)));
	}

	pcd->success = !g_cancellable_is_cancelled (cancellable);
	pcd->client = g_object_ref (client);

 out:
	if (!copied_components && !g_cancellable_is_cancelled (cancellable) && error && !*error)
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT, _("No suitable component found"));

	icalcomponent_free (icalcomp);
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
		e_marshal_VOID__POINTER_POINTER,
		G_TYPE_NONE, 2,
		G_TYPE_POINTER,
		G_TYPE_POINTER);

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

icaltimezone *
e_calendar_view_get_timezone (ECalendarView *cal_view)
{
	g_return_val_if_fail (E_IS_CALENDAR_VIEW (cal_view), NULL);
	return e_cal_model_get_timezone (cal_view->priv->model);
}

void
e_calendar_view_set_timezone (ECalendarView *cal_view,
                              icaltimezone *zone)
{
	icaltimezone *old_zone;

	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	old_zone = e_cal_model_get_timezone (cal_view->priv->model);
	if (old_zone == zone)
		return;

	e_cal_model_set_timezone (cal_view->priv->model, zone);
	g_signal_emit (
		cal_view, signals[TIMEZONE_CHANGED], 0,
		old_zone, zone);
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
 * e_calendar_view_new_appointment_full
 * @cal_view: an #ECalendarView
 * @all_day: Whether create all day event or not.
 * @meeting: This is a meeting or an appointment.
 * @no_past_date: Don't create event in past date, use actual date instead
 * (if %TRUE).
 *
 * Opens an event editor dialog for a new appointment. The appointment's
 * start and end times are set to the currently selected time range in
 * the calendar view.
 *
 * When the selection is for all day and we don't need @all_day event,
 * then this do a rounding to the actual hour for actual day (today) and
 * to the 'day begins' from preferences in other selected day.
 */
void
e_calendar_view_new_appointment_full (ECalendarView *cal_view,
                                      gboolean all_day,
                                      gboolean meeting,
                                      gboolean no_past_date)
{
	ECalModel *model;
	time_t dtstart, dtend, now;
	gboolean do_rounding = FALSE;

	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	model = e_calendar_view_get_model (cal_view);

	now = time (NULL);

	if (!e_calendar_view_get_selected_time_range (cal_view, &dtstart, &dtend)) {
		dtstart = now;
		dtend = dtstart + 3600;
	}

	if (no_past_date && dtstart < now) {
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
		dtend = dtstart + (time_div * 60);
	}

	e_cal_ops_new_component_editor_from_model (
		e_calendar_view_get_model (cal_view), NULL,
		dtstart, dtend, meeting, all_day);
}

void
e_calendar_view_new_appointment (ECalendarView *cal_view)
{
	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	e_calendar_view_new_appointment_full (cal_view, FALSE, FALSE, FALSE);
}

/* Ensures the calendar is selected */
static void
object_created_cb (CompEditor *ce,
                   ECalendarView *cal_view)
{
	e_cal_model_emit_object_created (e_calendar_view_get_model (cal_view), comp_editor_get_client (ce));
}

CompEditor *
e_calendar_view_open_event_with_flags (ECalendarView *cal_view,
                                       ECalClient *client,
                                       icalcomponent *icalcomp,
                                       guint32 flags)
{
	CompEditor *ce;
	const gchar *uid;
	ECalComponent *comp;
	EShell *shell;

	/* FIXME ECalendarView should own an EShell pointer. */
	shell = e_shell_get_default ();

	uid = icalcomponent_get_uid (icalcomp);

	ce = comp_editor_find_instance (uid);
	if (!ce) {
		ce = event_editor_new (client, shell, flags);

		g_signal_connect (
			ce, "object_created",
			G_CALLBACK (object_created_cb), cal_view);

		comp = e_cal_component_new ();
		e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (icalcomp));
		comp_editor_edit_comp (ce, comp);
		if (flags & COMP_EDITOR_MEETING)
			event_editor_show_meeting (EVENT_EDITOR (ce));

		g_object_unref (comp);
	}

	gtk_window_present (GTK_WINDOW (ce));

	return ce;
}

/**
 * e_calendar_view_edit_appointment
 * @cal_view: A calendar view.
 * @client: Calendar client.
 * @icalcomp: The object to be edited.
 * @mode: one of #EEditEventMode
 *
 * Opens an editor window to allow the user to edit the selected
 * object.
 */
void
e_calendar_view_edit_appointment (ECalendarView *cal_view,
                                  ECalClient *client,
                                  icalcomponent *icalcomp,
                                  EEditEventMode mode)
{
	ECalModel *model;
	ESourceRegistry *registry;
	guint32 flags = 0;

	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));
	g_return_if_fail (E_IS_CAL_CLIENT (client));
	g_return_if_fail (icalcomp != NULL);

	model = e_calendar_view_get_model (cal_view);
	registry = e_cal_model_get_registry (model);

	if ((mode == EDIT_EVENT_AUTODETECT && icalcomponent_get_first_property (icalcomp, ICAL_ATTENDEE_PROPERTY) != NULL)
	    || mode == EDIT_EVENT_FORCE_MEETING) {
		ECalComponent *comp = e_cal_component_new ();
		e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (icalcomp));
		flags |= COMP_EDITOR_MEETING;
		if (itip_organizer_is_user (registry, comp, client) ||
		    itip_sentby_is_user (registry, comp, client) ||
		    !e_cal_component_has_attendees (comp))
			flags |= COMP_EDITOR_USER_ORG;
		g_object_unref (comp);
	}

	e_calendar_view_open_event_with_flags (cal_view, client, icalcomp, flags);
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
get_label (struct icaltimetype *tt,
           icaltimezone *f_zone,
           icaltimezone *t_zone)
{
	struct tm tmp_tm;

	tmp_tm = icaltimetype_to_tm_with_zone (tt, f_zone, t_zone);

	return e_datetime_format_format_tm ("calendar", "table", tt->is_date ? DTFormatKindDate : DTFormatKindDateTime, &tmp_tm);
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
	GtkWidget *label, *box, *hbox, *ebox, *frame;
	const gchar *str;
	gchar *tmp, *tmp1 = NULL, *tmp2 = NULL;
	ECalComponentOrganizer organiser;
	ECalComponentDateTime dtstart, dtend;
	icalcomponent *clone_comp;
	time_t t_start, t_end;
	ECalendarViewEvent *pevent;
	GtkWidget *widget;
	GdkWindow *window;
	GdkDisplay *display;
	GdkDeviceManager *device_manager;
	GdkRGBA bg_rgba, fg_rgba;
	GQueue *grabbed_keyboards;
	ECalComponent *newcomp = e_cal_component_new ();
	icaltimezone *zone, *default_zone;
	ECalModel *model;
	ECalClient *client = NULL;
	GList *list, *link;
	gboolean free_text = FALSE;

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

	default_zone = e_calendar_view_get_timezone  (data->cal_view);
	pevent = data->get_view_event (data->cal_view, data->day, data->event_num);

	if (!is_comp_data_valid (pevent))
		return FALSE;

	client = pevent->comp_data->client;

	clone_comp = icalcomponent_new_clone (pevent->comp_data->icalcomp);
	if (!e_cal_component_set_icalcomponent (newcomp, clone_comp))
		g_warning ("couldn't update calendar component with modified data from backend\n");

	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

	str = e_calendar_view_get_icalcomponent_summary (pevent->comp_data->client, pevent->comp_data->icalcomp, &free_text);

	if (!(str && *str)) {
		g_object_unref (newcomp);
		gtk_widget_destroy (box);

		return FALSE;
	}

	tmp = g_markup_printf_escaped ("<b>%s</b>", str);
	label = gtk_label_new (NULL);
	gtk_label_set_line_wrap ((GtkLabel *) label, TRUE);
	gtk_label_set_markup ((GtkLabel *) label, tmp);

	if (free_text) {
		g_free ((gchar *) str);
		str = NULL;
	}

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start ((GtkBox *) hbox, label, FALSE, FALSE, 0);
	ebox = gtk_event_box_new ();
	gtk_container_add ((GtkContainer *) ebox, hbox);
	gtk_widget_override_background_color (ebox, GTK_STATE_FLAG_NORMAL, &bg_rgba);
	gtk_widget_override_color (label, GTK_STATE_FLAG_NORMAL, &fg_rgba);

	gtk_box_pack_start ((GtkBox *) box, ebox, FALSE, FALSE, 0);
	g_free (tmp);

	e_cal_component_get_organizer (newcomp, &organiser);
	if (organiser.cn) {
		gchar *ptr;
		ptr = strchr (organiser.value, ':');

		if (ptr) {
			ptr++;
			/* To Translators: It will display "Organiser: NameOfTheUser <email@ofuser.com>" */
			tmp = g_strdup_printf (_("Organizer: %s <%s>"), organiser.cn, ptr);
		}
		else
			/* With SunOne accouts, there may be no ':' in organiser.value*/
			tmp = g_strdup_printf (_("Organizer: %s"), organiser.cn);

		label = gtk_label_new (tmp);
		hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
		gtk_box_pack_start ((GtkBox *) hbox, label, FALSE, FALSE, 0);
		ebox = gtk_event_box_new ();
		gtk_container_add ((GtkContainer *) ebox, hbox);
		gtk_box_pack_start ((GtkBox *) box, ebox, FALSE, FALSE, 0);

		g_free (tmp);
	}

	e_cal_component_get_location (newcomp, &str);

	if (str) {
		/* To Translators: It will display "Location: PlaceOfTheMeeting" */
		tmp = g_markup_printf_escaped (_("Location: %s"), str);
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
	e_cal_component_get_dtstart (newcomp, &dtstart);
	e_cal_component_get_dtend (newcomp, &dtend);

	if (dtstart.tzid) {
		zone = icalcomponent_get_timezone (e_cal_component_get_icalcomponent (newcomp), dtstart.tzid);
		if (!zone)
			e_cal_client_get_timezone_sync (client, dtstart.tzid, &zone, NULL, NULL);

		if (!zone)
			zone = default_zone;

	} else {
		zone = NULL;
	}

	if (dtstart.value) {
		t_start = icaltime_as_timet_with_zone (*dtstart.value, zone);
		if (dtend.value)
			t_end = icaltime_as_timet_with_zone (*dtend.value, zone);
		else
			t_end = t_start;

		tmp1 = get_label (dtstart.value, zone, default_zone);
		tmp = calculate_time (t_start, t_end);

		/* To Translators: It will display "Time: ActualStartDateAndTime (DurationOfTheMeeting)"*/
		tmp2 = g_strdup_printf (_("Time: %s %s"), tmp1, tmp);
		if (zone && !cal_comp_util_compare_event_timezones (newcomp, client, default_zone)) {
			g_free (tmp);
			g_free (tmp1);

			tmp1 = get_label (dtstart.value, zone, zone);
			tmp = g_strconcat (tmp2, "\n\t[ ", tmp1, " ", icaltimezone_get_display_name (zone), " ]", NULL);
		} else {
			g_free (tmp);
			tmp = tmp2;
			tmp2 = NULL;
		}
	} else {
		tmp = NULL;
	}

	e_cal_component_free_datetime (&dtstart);
	e_cal_component_free_datetime (&dtend);

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

	pevent->tooltip = gtk_window_new (GTK_WINDOW_POPUP);
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
icalcomp_contains_category (icalcomponent *icalcomp,
                            const gchar *category)
{
	icalproperty *property;

	g_return_val_if_fail (icalcomp != NULL && category != NULL, FALSE);

	for (property = icalcomponent_get_first_property (icalcomp, ICAL_CATEGORIES_PROPERTY);
	     property != NULL;
	     property = icalcomponent_get_next_property (icalcomp, ICAL_CATEGORIES_PROPERTY)) {
		gchar *value = icalproperty_get_value_as_string_r (property);

		if (value && strcmp (category, value) == 0) {
			g_free (value);
			return TRUE;
		}
		g_free (value);
	}

	return FALSE;
}

/* e_calendar_view_get_icalcomponent_summary returns summary of calcomp,
 * and for type of birthday or anniversary it append number of years since
 * beginning. In this case, the free_text is set to TRUE and caller need
 * to g_free returned string, otherwise free_text is set to FALSE and
 * returned value is owned by calcomp.
 */

const gchar *
e_calendar_view_get_icalcomponent_summary (ECalClient *client,
                                           icalcomponent *icalcomp,
                                           gboolean *free_text)
{
	const gchar *summary;

	g_return_val_if_fail (icalcomp != NULL && free_text != NULL, NULL);

	*free_text = FALSE;
	summary = icalcomponent_get_summary (icalcomp);

	if (icalcomp_contains_category (icalcomp, _("Birthday")) ||
	    icalcomp_contains_category (icalcomp, _("Anniversary"))) {
		icalproperty *xprop;

		for (xprop = icalcomponent_get_first_property (icalcomp, ICAL_X_PROPERTY);
		     xprop;
		     xprop = icalcomponent_get_next_property (icalcomp, ICAL_X_PROPERTY)) {
			const gchar *xname = icalproperty_get_x_name (xprop);

			if (xname && g_ascii_strcasecmp (xname, "X-EVOLUTION-SINCE-YEAR") == 0) {
				struct icaltimetype dtnow;
				gint since_year;
				gchar *str;

				str = icalproperty_get_value_as_string_r (xprop);
				since_year = str ? atoi (str) : 0;
				g_free (str);

				dtnow = icalcomponent_get_dtstart (icalcomp);

				if (since_year > 0 && dtnow.year - since_year > 0) {
					summary = g_strdup_printf ("%s (%d)", summary ? summary : "", dtnow.year - since_year);
					*free_text = summary != NULL;
				}

				break;
			}
		}
	}

	return summary;
}

/* A callback for e_cal_ops_create_component(), whose @user_data is an ECalendarView instance */
void
e_calendar_view_component_created_cb (ECalModel *model,
				      ECalClient *client,
				      icalcomponent *original_icalcomp,
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
	struct icaltimetype start_tt, end_tt;
	icaltimezone *zone;
	gchar buffer[1024] = { 0 };
	gchar end_buffer[512] = { 0 };

	g_return_val_if_fail (E_IS_CALENDAR_VIEW (cal_view), NULL);

	if (!e_calendar_view_get_visible_time_range (cal_view, &start_time, &end_time))
		return NULL;

	zone = e_cal_model_get_timezone (cal_view->priv->model);

	start_tt = icaltime_from_timet_with_zone (start_time, FALSE, zone);
	start_tm.tm_year = start_tt.year - 1900;
	start_tm.tm_mon = start_tt.month - 1;
	start_tm.tm_mday = start_tt.day;
	start_tm.tm_hour = start_tt.hour;
	start_tm.tm_min = start_tt.minute;
	start_tm.tm_sec = start_tt.second;
	start_tm.tm_isdst = -1;
	start_tm.tm_wday = time_day_of_week (start_tt.day, start_tt.month - 1, start_tt.year);

	/* Subtract one from end_time so we don't get an extra day. */
	end_tt = icaltime_from_timet_with_zone (end_time - 1, FALSE, zone);
	end_tm.tm_year = end_tt.year - 1900;
	end_tm.tm_mon = end_tt.month - 1;
	end_tm.tm_mday = end_tt.day;
	end_tm.tm_hour = end_tt.hour;
	end_tm.tm_min = end_tt.minute;
	end_tm.tm_sec = end_tt.second;
	end_tm.tm_isdst = -1;
	end_tm.tm_wday = time_day_of_week (end_tt.day, end_tt.month - 1, end_tt.year);

	if (E_IS_MONTH_VIEW (cal_view) || E_IS_CAL_LIST_VIEW (cal_view)) {
		if (start_tm.tm_year == end_tm.tm_year) {
			if (start_tm.tm_mon == end_tm.tm_mon) {
				e_utf8_strftime (buffer, sizeof (buffer),
					"%d", &start_tm);
				e_utf8_strftime (end_buffer, sizeof (end_buffer),
					_("%d %b %Y"), &end_tm);
				strcat (buffer, " - ");
				strcat (buffer, end_buffer);
			} else {
				e_utf8_strftime (buffer, sizeof (buffer),
					_("%d %b"), &start_tm);
				e_utf8_strftime (end_buffer, sizeof (end_buffer),
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
	} else {
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
	}

	return g_strdup (buffer);
}

void
e_calendar_view_move_view_range (ECalendarView *cal_view,
				 ECalendarViewMoveType mode_type,
				 time_t exact_date)
{
	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	g_signal_emit (cal_view, signals[MOVE_VIEW_RANGE], 0, mode_type, (gint64) exact_date);
}
