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
#include "print.h"

#define X_EVOLUTION_CLIENT_UID "X-EVOLUTION-CLIENT-UID"

struct _ECalendarViewPrivate {
	/* The calendar model we are monitoring */
	ECalModel *model;

	gint time_divisions;
	GSList *selected_cut_list; /* ECalendarViewSelectionData * */

	GtkTargetList *copy_target_list;
	GtkTargetList *paste_target_list;

	gboolean allow_direct_summary_edit;
	gboolean allow_event_dnd;
};

enum {
	PROP_0,
	PROP_COPY_TARGET_LIST,
	PROP_MODEL,
	PROP_PASTE_TARGET_LIST,
	PROP_TIME_DIVISIONS,
	PROP_IS_EDITING,
	PROP_ALLOW_DIRECT_SUMMARY_EDIT,
	PROP_ALLOW_EVENT_DND
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

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (ECalendarView, e_calendar_view, GTK_TYPE_GRID,
	G_ADD_PRIVATE (ECalendarView)
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL)
	G_IMPLEMENT_INTERFACE (E_TYPE_SELECTABLE, calendar_view_selectable_init));

ECalendarViewSelectionData *
e_calendar_view_selection_data_new (ECalClient *client,
				    ICalComponent *icalcomp)
{
	ECalendarViewSelectionData *sel_data;

	sel_data = g_slice_new0 (ECalendarViewSelectionData);
	sel_data->client = g_object_ref (client);
	sel_data->icalcomp = g_object_ref (icalcomp);

	return sel_data;
}

void
e_calendar_view_selection_data_free (gpointer ptr)
{
	ECalendarViewSelectionData *sel_data = ptr;

	if (sel_data) {
		g_clear_object (&sel_data->client);
		g_clear_object (&sel_data->icalcomp);
		g_slice_free (ECalendarViewSelectionData, sel_data);
	}
}

static void
calendar_view_delete_event (ECalendarView *cal_view,
                            ECalendarViewSelectionData *sel_data,
			    ECalObjModType mod)
{
	ECalModel *model;
	ECalDataModel *data_model;
	ECalComponent *comp;
	ECalClient *client;
	GtkWidget *toplevel;
	GtkWindow *parent_window;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (cal_view));
	parent_window = GTK_IS_WINDOW (toplevel) ? GTK_WINDOW (toplevel) : NULL;

	model = e_calendar_view_get_model (cal_view);
	data_model = e_cal_model_get_data_model (model);

	comp = e_cal_component_new ();
	e_cal_component_set_icalcomponent (comp, i_cal_component_clone (sel_data->icalcomp));

	client = g_object_ref (sel_data->client);

	cal_comp_util_remove_component (parent_window, data_model, client, comp, mod,
		e_cal_model_get_confirm_delete (model));

	g_clear_object (&client);
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

		case PROP_ALLOW_EVENT_DND:
			e_calendar_view_set_allow_event_dnd (
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

		case PROP_ALLOW_EVENT_DND:
			g_value_set_boolean (value, e_calendar_view_get_allow_event_dnd (E_CALENDAR_VIEW (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
calendar_view_dispose (GObject *object)
{
	ECalendarView *self = E_CALENDAR_VIEW (object);

	if (self->priv->model != NULL) {
		g_signal_handlers_disconnect_matched (
			self->priv->model, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, object);
		g_clear_object (&self->priv->model);
	}

	g_clear_pointer (&self->priv->copy_target_list, gtk_target_list_unref);
	g_clear_pointer (&self->priv->paste_target_list, gtk_target_list_unref);

	if (self->priv->selected_cut_list) {
		g_slist_free_full (self->priv->selected_cut_list, e_calendar_view_selection_data_free);
		self->priv->selected_cut_list = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_calendar_view_parent_class)->dispose (object);
}

static void
calendar_view_constructed (GObject *object)
{
	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_calendar_view_parent_class)->constructed (object);

	/* Do this after calendar_view_init() so extensions can query
	 * the GType accurately.  See GInstanceInitFunc documentation
	 * for details of the problem. */
	e_extensible_load_extensions (E_EXTENSIBLE (object));
}

static void
calendar_view_update_actions (ESelectable *selectable,
                              EFocusTracker *focus_tracker,
                              GdkAtom *clipboard_targets,
                              gint n_clipboard_targets)
{
	ECalendarView *view;
	EUIAction *action;
	GtkTargetList *target_list;
	GSList *selected, *link;
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

	selected = e_calendar_view_get_selected_events (view);
	n_selected = g_slist_length (selected);

	for (link = selected; link; link = g_slist_next (link)) {
		ECalendarViewSelectionData *sel_data = link->data;
		ECalClient *client;
		ICalComponent *icomp;

		client = sel_data->client;
		icomp = sel_data->icalcomp;

		sources_are_editable = sources_are_editable && !e_client_is_readonly (E_CLIENT (client));

		recurring |=
			e_cal_util_component_is_instance (icomp) ||
			e_cal_util_component_has_recurrences (icomp);
	}

	g_slist_free_full (selected, e_calendar_view_selection_data_free);

	target_list = e_selectable_get_paste_target_list (selectable);
	for (ii = 0; ii < n_clipboard_targets && !can_paste; ii++)
		can_paste = gtk_target_list_find (
			target_list, clipboard_targets[ii], NULL);

	action = e_focus_tracker_get_cut_clipboard_action (focus_tracker);
	sensitive = (n_selected > 0) && sources_are_editable && !is_editing;
	tooltip = _("Cut selected events to the clipboard");
	e_ui_action_set_sensitive (action, sensitive);
	e_ui_action_set_tooltip (action, tooltip);

	action = e_focus_tracker_get_copy_clipboard_action (focus_tracker);
	sensitive = (n_selected > 0) && !is_editing;
	tooltip = _("Copy selected events to the clipboard");
	e_ui_action_set_sensitive (action, sensitive);
	e_ui_action_set_tooltip (action, tooltip);

	action = e_focus_tracker_get_paste_clipboard_action (focus_tracker);
	sensitive = sources_are_editable && can_paste && !is_editing;
	tooltip = _("Paste events from the clipboard");
	e_ui_action_set_sensitive (action, sensitive);
	e_ui_action_set_tooltip (action, tooltip);

	action = e_focus_tracker_get_delete_selection_action (focus_tracker);
	sensitive = (n_selected > 0) && sources_are_editable && !recurring && !is_editing;
	tooltip = _("Delete selected events");
	e_ui_action_set_sensitive (action, sensitive);
	e_ui_action_set_tooltip (action, tooltip);
}

static void
calendar_view_cut_clipboard (ESelectable *selectable)
{
	ECalendarView *cal_view;
	ECalendarViewPrivate *priv;
	GSList *selected;

	cal_view = E_CALENDAR_VIEW (selectable);
	priv = cal_view->priv;

	g_slist_free_full (priv->selected_cut_list, e_calendar_view_selection_data_free);
	priv->selected_cut_list = NULL;

	selected = e_calendar_view_get_selected_events (cal_view);
	if (!selected)
		return;

	e_selectable_copy_clipboard (selectable);

	priv->selected_cut_list = selected;
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
								i_cal_component_take_component (des_icomp, i_cal_component_clone (vtz_comp));
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
	GSList *selected, *link;
	gchar *comp_str;
	ICalComponent *vcal_comp;
	GtkClipboard *clipboard;

	cal_view = E_CALENDAR_VIEW (selectable);
	priv = cal_view->priv;

	selected = e_calendar_view_get_selected_events (cal_view);
	if (!selected)
		return;

	if (priv->selected_cut_list) {
		g_slist_free_full (priv->selected_cut_list, e_calendar_view_selection_data_free);
		priv->selected_cut_list = NULL;
	}

	/* create top-level VCALENDAR component and add VTIMEZONE's */
	vcal_comp = e_cal_util_new_top_level ();
	for (link = selected; link; link = g_slist_next (link)) {
		ECalendarViewSelectionData *sel_data = link->data;

		e_cal_util_add_timezones_from_component (vcal_comp, sel_data->icalcomp);
		add_related_timezones (vcal_comp, sel_data->icalcomp, sel_data->client);
	}

	for (link = selected; link; link = g_slist_next (link)) {
		ECalendarViewSelectionData *sel_data = link->data;
		ICalComponent *new_icomp;

		new_icomp = i_cal_component_clone (sel_data->icalcomp);
		e_cal_util_component_set_x_property (new_icomp, X_EVOLUTION_CLIENT_UID, e_source_get_uid (e_client_get_source (E_CLIENT (sel_data->client))));

		/* do not remove RECURRENCE-IDs from copied objects */
		i_cal_component_take_component (vcal_comp, new_icomp);
	}

	comp_str = i_cal_component_as_ical_string (vcal_comp);

	/* copy the VCALENDAR to the clipboard */
	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
	e_clipboard_set_calendar (clipboard, comp_str, -1);
	gtk_clipboard_store (clipboard);

	/* free memory */
	g_object_unref (vcal_comp);
	g_free (comp_str);
	g_slist_free_full (selected, e_calendar_view_selection_data_free);
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

	comp = e_cal_component_new_from_icalcomponent (i_cal_component_clone (original_icomp));
	g_return_if_fail (comp != NULL);

	registry = e_cal_model_get_registry (model);

	if (new_uid)
		e_cal_component_set_uid (comp, new_uid);

	if (itip_has_any_attendees (comp) &&
	    (itip_organizer_is_user (registry, comp, client) ||
	     itip_sentby_is_user (registry, comp, client)) &&
	     e_cal_dialogs_send_component ((GtkWindow *) toplevel, client, comp, TRUE, &strip_alarms, NULL)) {
		itip_send_component_with_model (e_cal_model_get_data_model (model), I_CAL_METHOD_REQUEST,
			comp, client, NULL, NULL, NULL, (strip_alarms ? E_ITIP_SEND_COMPONENT_FLAG_STRIP_ALARMS : 0));
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
	ic_dur = i_cal_duration_new_from_int (tt_end - tt_start);

	if (i_cal_duration_as_int (ic_dur) > 60 * 60 * 24) {
		/* This is a long event */
		start_offset = i_cal_time_get_hour (old_dtstart) * 60 + i_cal_time_get_minute (old_dtstart);
		end_offset = i_cal_time_get_hour (old_dtstart) * 60 + i_cal_time_get_minute (old_dtend);
	}

	ic_oneday = i_cal_duration_new_null_duration ();
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
			ic_dur = i_cal_duration_new_from_int (time_division * 60);
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
			ICalTime *new_time = i_cal_time_new_from_timet_with_zone (dtstart, FALSE, default_zone);

			i_cal_time_set_hour (new_time, i_cal_time_get_hour (old_dtstart));
			i_cal_time_set_minute (new_time, i_cal_time_get_minute (old_dtstart));
			i_cal_time_set_second (new_time, i_cal_time_get_second (old_dtstart));

			new_dtstart = i_cal_time_as_timet_with_zone (new_time, old_dtstart_zone);

			g_clear_object (&new_time);
		}
	}

	itime = i_cal_time_new_from_timet_with_zone (new_dtstart, FALSE, old_dtstart_zone);
	/* set the timezone properly */
	i_cal_time_set_timezone (itime, old_dtstart_zone);
	if (all_day_event)
		i_cal_time_set_is_date (itime, TRUE);
	i_cal_component_set_dtstart (icomp, itime);

	i_cal_time_set_is_date (itime, FALSE);
	btime = i_cal_time_add (itime, ic_dur);
	if (all_day_event)
		i_cal_time_set_is_date (btime, TRUE);
	i_cal_component_set_dtend (icomp, btime);

	g_clear_object (&itime);
	g_clear_object (&btime);
	g_clear_object (&old_dtstart);
	g_clear_object (&old_dtend);
	g_clear_object (&ic_dur);
	g_clear_object (&ic_oneday);

	/* The new uid stuff can go away once we actually set it in the backend */
	uid = e_util_generate_uid ();
	comp = e_cal_component_new_from_icalcomponent (i_cal_component_clone (icomp));
	e_cal_component_set_uid (comp, uid);
	g_free (uid);

	e_cal_component_commit_sequence (comp);

	e_cal_ops_create_component (model, client, e_cal_component_get_icalcomponent (comp),
		calendar_view_component_created_cb, g_object_ref (top_level), g_object_unref);

	g_object_unref (comp);
}

typedef struct {
	ECalendarView *cal_view;
	GSList *selected_cut_list; /* ECalendarViewSelectionData * */
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
			ECalDataModel *data_model;
			ESourceRegistry *registry;
			GSList *link;

			data_model = e_cal_model_get_data_model (e_calendar_view_get_model (pcd->cal_view));
			registry = e_cal_data_model_get_registry (data_model);

			for (link = pcd->selected_cut_list; link != NULL; link = g_slist_next (link)) {
				ECalendarViewSelectionData *sel_data = link->data;
				ECalComponent *comp;
				ECalOperationFlags op_flags = E_CAL_OPERATION_FLAG_NONE;
				gboolean organizer_is_user;
				const gchar *uid;
				GSList *found = NULL;

				/* Remove them one by one after ensuring it has been copied to the destination successfully */
				found = g_slist_find_custom (pcd->copied_uids, i_cal_component_get_uid (sel_data->icalcomp), (GCompareFunc) strcmp);
				if (!found)
					continue;

				g_free (found->data);
				pcd->copied_uids = g_slist_delete_link (pcd->copied_uids, found);

				comp = e_cal_component_new_from_icalcomponent (i_cal_component_clone (sel_data->icalcomp));
				organizer_is_user = itip_organizer_is_user (registry, comp, sel_data->client);

				if (itip_has_any_attendees (comp) && (organizer_is_user ||
				    itip_sentby_is_user (registry, comp, sel_data->client))) {
					if (e_cal_dialogs_cancel_component ((GtkWindow *) pcd->top_level, sel_data->client, comp, TRUE, organizer_is_user)) {
						itip_send_component_with_model (data_model, I_CAL_METHOD_CANCEL,
							comp, sel_data->client, NULL, NULL, NULL,
							E_ITIP_SEND_COMPONENT_FLAG_STRIP_ALARMS | E_ITIP_SEND_COMPONENT_FLAG_ENSURE_MASTER_OBJECT);
					} else {
						op_flags = E_CAL_OPERATION_FLAG_DISABLE_ITIP_MESSAGE;
					}
				} else if (e_cal_client_check_save_schedules (sel_data->client) &&
					   itip_attendee_is_user (registry, comp, sel_data->client) &&
					   !e_cal_dialogs_cancel_component ((GtkWindow *) pcd->top_level, sel_data->client, comp, FALSE, organizer_is_user)) {
					op_flags = E_CAL_OPERATION_FLAG_DISABLE_ITIP_MESSAGE;
				}

				uid = e_cal_component_get_uid (comp);
				if (e_cal_component_is_instance (comp)) {
					gchar *rid = NULL;

					/* when cutting detached instances, only cut that instance */
					rid = e_cal_component_get_recurid_as_string (comp);
					e_cal_ops_remove_component (data_model, sel_data->client, uid, rid, E_CAL_OBJ_MOD_THIS, TRUE, op_flags);
					g_free (rid);
				} else {
					e_cal_ops_remove_component (data_model, sel_data->client, uid, NULL, E_CAL_OBJ_MOD_ALL, FALSE, op_flags);
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
		g_slist_free_full (pcd->selected_cut_list, e_calendar_view_selection_data_free);
		g_slist_free_full (pcd->copied_uids, g_free);
		g_free (pcd->ical_str);
		g_slice_free (PasteClipboardData, pcd);
	}
}

static gboolean
paste_recurring_component (ECalModel *model,
			   ECalClient *client,
			   ICalComponent *icomp,
			   const gchar *extension_name,
			   GHashTable *covered_comps, /* source_uid+icomp_uid ~> 1 */
			   gboolean *out_did_cover,
			   GCancellable *cancellable,
			   GError **error)
{
	ESource *src_source;
	EClient *src_client;
	EClientCache *client_cache;
	gchar *src_client_uid;
	gchar *key = NULL;
	gboolean success;
	GError *local_error = NULL;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), FALSE);
	g_return_val_if_fail (E_IS_CAL_CLIENT (client), FALSE);
	g_return_val_if_fail (I_CAL_IS_COMPONENT (icomp), FALSE);
	g_return_val_if_fail (out_did_cover != NULL, FALSE);

	*out_did_cover = FALSE;

	if (!e_cal_util_component_has_recurrences (icomp) &&
	    !e_cal_util_component_is_instance (icomp)) {
		e_cal_util_component_remove_x_property (icomp, X_EVOLUTION_CLIENT_UID);
		return TRUE;
	}

	if (e_cal_util_component_has_recurrences (icomp)) {
		ICalProperty *prop = i_cal_component_get_first_property (icomp, I_CAL_RRULE_PROPERTY);
		if (prop) {
			i_cal_property_remove_parameter_by_name (prop, "X-EVOLUTION-ENDDATE");
			g_object_unref (prop);
		}
	}

	src_client_uid = e_cal_util_component_dup_x_property (icomp, X_EVOLUTION_CLIENT_UID);
	if (!src_client_uid)
		return TRUE;

	e_cal_util_component_remove_x_property (icomp, X_EVOLUTION_CLIENT_UID);

	/* Let it move an instance only within the same client, otherwise transfer the whole series */
	if (g_strcmp0 (src_client_uid, e_source_get_uid (e_client_get_source (E_CLIENT (client)))) == 0) {
		g_free (src_client_uid);
		return TRUE;
	}

	*out_did_cover = TRUE;

	if (covered_comps) {
		key = g_strconcat (src_client_uid, ":", i_cal_component_get_uid (icomp), NULL);
		if (g_hash_table_contains (covered_comps, key)) {
			g_free (src_client_uid);
			g_free (key);
			return TRUE;
		}
	}

	client_cache = e_cal_model_get_client_cache (model);
	src_source = e_source_registry_ref_source (e_cal_model_get_registry (model), src_client_uid);
	src_client = src_source ? e_client_cache_get_client_sync (client_cache, src_source, extension_name, (guint32) -1, cancellable, error) : NULL;

	success = src_client != NULL;

	if (success && !cal_comp_transfer_item_to_sync (E_CAL_CLIENT (src_client), client, icomp, TRUE, cancellable, &local_error)) {
		if (g_error_matches (local_error, E_CAL_CLIENT_ERROR, E_CAL_CLIENT_ERROR_OBJECT_NOT_FOUND)) {
			/* The event could be removed from the source calendar, then use what is stored in the clipboard */
			*out_did_cover = FALSE;
			g_clear_error (&local_error);
		} else {
			g_propagate_error (error, local_error);
			success = FALSE;
		}
	} else if (!success && !src_client) {
		/* ... similarly when the source client cannot be found */
		*out_did_cover = FALSE;
		success = TRUE;
	}

	if (success && *out_did_cover && key)
		g_hash_table_insert (covered_comps, key, GINT_TO_POINTER (1));
	else
		g_free (key);

	g_clear_object (&src_client);
	g_clear_object (&src_source);
	g_free (src_client_uid);

	return success;
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
	gboolean success = TRUE;
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

	e_client = e_client_cache_get_client_sync (client_cache, source, extension_name, E_DEFAULT_WAIT_FOR_CONNECTED_SECONDS, cancellable, &local_error);
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
		GHashTable *covered_comps;
		ICalComponent *subcomp;

		/* add timezones first, to have them ready */
		for (subcomp = i_cal_component_get_first_component (icomp, I_CAL_VTIMEZONE_COMPONENT);
		     subcomp;
		     g_object_unref (subcomp), subcomp = i_cal_component_get_next_component (icomp, I_CAL_VTIMEZONE_COMPONENT)) {
			ICalTimezone *zone;

			zone = i_cal_timezone_new ();
			i_cal_timezone_set_component (zone, i_cal_component_clone (subcomp));

			if (!e_cal_client_add_timezone_sync (client, zone, cancellable, error)) {
				g_object_unref (subcomp);
				g_object_unref (zone);
				goto out;
			}

			g_object_unref (zone);
		}

		covered_comps = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

		for (subcomp = i_cal_component_get_first_component (icomp, I_CAL_VEVENT_COMPONENT);
		     subcomp;
		     g_object_unref (subcomp), subcomp = i_cal_component_get_next_component (icomp, I_CAL_VEVENT_COMPONENT)) {
			gboolean did_cover = FALSE;

			if (!paste_recurring_component (model, client, subcomp, extension_name, covered_comps, &did_cover, cancellable, error)) {
				g_object_unref (subcomp);
				success = FALSE;
				break;
			}

			if (!did_cover) {
				e_calendar_view_add_event_sync (model, client, pcd->selection_start, default_zone, subcomp, all_day,
					pcd->is_day_view, pcd->time_division, pcd->top_level);
			}

			copied_components++;
			if (pcd->selected_cut_list)
				pcd->copied_uids = g_slist_prepend (pcd->copied_uids, g_strdup (i_cal_component_get_uid (subcomp)));
		}

		g_hash_table_destroy (covered_comps);
	} else if (kind == e_cal_model_get_component_kind (model)) {
		gboolean did_cover = FALSE;

		if (!paste_recurring_component (model, client, icomp, extension_name, NULL, &did_cover, cancellable, error)) {
			success = FALSE;
		} else {
			if (!did_cover) {
				e_calendar_view_add_event_sync (model, client, pcd->selection_start, default_zone, icomp, all_day,
					pcd->is_day_view, pcd->time_division, pcd->top_level);
			}

			copied_components++;
			if (pcd->selected_cut_list)
				pcd->copied_uids = g_slist_prepend (pcd->copied_uids, g_strdup (i_cal_component_get_uid (icomp)));
		}
	}

	pcd->success = success && !g_cancellable_is_cancelled (cancellable);
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
		e_calendar_view_paste_text (cal_view);

	/* Paste iCalendar data into the view. */
	} else if (e_clipboard_wait_is_calendar_available (clipboard)) {
		PasteClipboardData *pcd;
		ECalDataModel *data_model;
		GCancellable *cancellable;
		const gchar *alert_ident = NULL;

		switch (e_cal_model_get_component_kind (model)) {
			case I_CAL_VEVENT_COMPONENT:
				alert_ident = "calendar:failed-create-event";
				break;
			case I_CAL_VJOURNAL_COMPONENT:
				alert_ident = "calendar:failed-create-memo";
				break;
			case I_CAL_VTODO_COMPONENT:
				alert_ident = "calendar:failed-create-task";
				break;
			default:
				g_warn_if_reached ();
				return;
		}

		pcd = g_slice_new0 (PasteClipboardData);
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
	GSList *selected, *link;

	cal_view = E_CALENDAR_VIEW (selectable);

	selected = e_calendar_view_get_selected_events (cal_view);

	for (link = selected; link; link = g_slist_next (link)) {
		ECalendarViewSelectionData *sel_data = link->data;

		calendar_view_delete_event (cal_view, sel_data, E_CAL_OBJ_MOD_ALL);
	}

	g_slist_free_full (selected, e_calendar_view_selection_data_free);
}

static gchar *
calendar_view_get_description_text (ECalendarView *cal_view)
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

	tt = i_cal_time_new_from_timet_with_zone (start_time, FALSE, zone);
	start_tm = e_cal_util_icaltime_to_tm (tt);
	g_clear_object (&tt);

	/* Subtract one from end_time so we don't get an extra day. */
	tt = i_cal_time_new_from_timet_with_zone (end_time - 1, FALSE, zone);
	end_tm = e_cal_util_icaltime_to_tm (tt);
	g_clear_object (&tt);

	if (E_IS_MONTH_VIEW (cal_view) || E_IS_CAL_LIST_VIEW (cal_view)) {
		if (start_tm.tm_year == end_tm.tm_year) {
			if (start_tm.tm_mon == end_tm.tm_mon) {
				e_utf8_strftime (start_buffer, sizeof (start_buffer), "%d", &start_tm);
				e_utf8_strftime (end_buffer, sizeof (end_buffer), _("%d %b %Y"), &end_tm);
			} else {
				/* xgettext:no-c-format */
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
			/* xgettext:no-c-format */
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

static void
e_calendar_view_class_init (ECalendarViewClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GtkBindingSet *binding_set;

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
	class->get_description_text = calendar_view_get_description_text;

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

	g_object_class_install_property (
		object_class,
		PROP_ALLOW_EVENT_DND,
		g_param_spec_boolean (
			"allow-event-dnd",
			"Whether can drag-and-drop events",
			NULL,
			TRUE,
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

	calendar_view->priv = e_calendar_view_get_instance_private (calendar_view);

	/* Set this early to avoid a divide-by-zero during init. */
	calendar_view->priv->time_divisions = 30;

	target_list = gtk_target_list_new (NULL, 0);
	e_target_list_add_calendar_targets (target_list, 0);
	calendar_view->priv->copy_target_list = target_list;

	target_list = gtk_target_list_new (NULL, 0);
	e_target_list_add_calendar_targets (target_list, 0);
	calendar_view->priv->paste_target_list = target_list;

	calendar_view->priv->allow_event_dnd = TRUE;
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
	g_return_val_if_fail (E_IS_CALENDAR_VIEW (cal_view), 30);

	return cal_view->priv->time_divisions;
}

void
e_calendar_view_set_time_divisions (ECalendarView *cal_view,
                                    gint time_divisions)
{
	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	/* To avoid division-by-zero and negative values on places where this is used */
	if (time_divisions <= 0)
		time_divisions = 30;

	if (cal_view->priv->time_divisions == time_divisions)
		return;

	cal_view->priv->time_divisions = time_divisions;

	g_object_notify (G_OBJECT (cal_view), "time-divisions");
}

/* (transfer full) (element-type ECalendarViewSelectionData):
   free with g_slist_free_full (selection, e_calendar_view_selection_data_free); */
GSList *
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
e_calendar_view_paste_text (ECalendarView *cal_view)
{
	ECalendarViewClass *class;

	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	class = E_CALENDAR_VIEW_GET_CLASS (cal_view);
	g_return_if_fail (class->paste_text != NULL);

	class->paste_text (cal_view);
}

void
e_calendar_view_delete_selected_occurrence (ECalendarView *cal_view,
					    ECalObjModType mod)
{
	ECalendarViewSelectionData *sel_data;
	GSList *selected;

	g_return_if_fail (mod == E_CAL_OBJ_MOD_THIS || mod == E_CAL_OBJ_MOD_THIS_AND_FUTURE);

	selected = e_calendar_view_get_selected_events (cal_view);
	if (!selected)
		return;

	sel_data = selected->data;
	calendar_view_delete_event (cal_view, sel_data, mod);

	g_slist_free_full (selected, e_calendar_view_selection_data_free);
}

void
e_calendar_view_open_event (ECalendarView *cal_view)
{
	GSList *selected;

	selected = e_calendar_view_get_selected_events (cal_view);
	if (selected) {
		ECalendarViewSelectionData *sel_data = selected->data;
		e_calendar_view_edit_appointment (cal_view, sel_data->client, sel_data->icalcomp, EDIT_EVENT_AUTODETECT);

		g_slist_free_full (selected, e_calendar_view_selection_data_free);
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
		ECalComponent *comp = e_cal_component_new_from_icalcomponent (i_cal_component_clone (icomp));
		flags |= E_COMP_EDITOR_FLAG_WITH_ATTENDEES;
		if (itip_organizer_is_user (registry, comp, client) ||
		    itip_sentby_is_user (registry, comp, client) ||
		    !e_cal_component_has_attendees (comp))
			flags |= E_COMP_EDITOR_FLAG_ORGANIZER_IS_USER;
		g_object_unref (comp);
	}

	e_calendar_view_open_event_with_flags (cal_view, client, icomp, flags);
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
	ICalProperty *prop;
	gchar *res = NULL;

	g_return_val_if_fail (icomp != NULL, NULL);

	prop = e_cal_util_component_find_property_for_locale (icomp, I_CAL_SUMMARY_PROPERTY, NULL);
	summary = prop ? i_cal_property_get_summary (prop) : NULL;

	if (icomp_contains_category (icomp, _("Birthday")) ||
	    icomp_contains_category (icomp, _("Anniversary")) ||
	    icomp_contains_category (icomp, _("Deathday"))) {
		gchar *since_year_str;

		since_year_str = e_cal_util_component_dup_x_property (icomp, "X-EVOLUTION-SINCE-YEAR");

		if (since_year_str) {
			ICalTime *dtstart;
			gint since_year;

			since_year = atoi (since_year_str);

			dtstart = i_cal_component_get_dtstart (icomp);

			if (since_year > 0 && dtstart && i_cal_time_is_valid_time (dtstart) &&
			    i_cal_time_get_year (dtstart) - since_year > 0) {
				/* Translators: the '%s' stands for a component summary, the '%d' for the years.
				   The string is used for Birthday & Anniversary events where the first year is
				   know, constructing a summary which also shows how many years the birthday or
				   anniversary is for. Example: "Birthday: John Doe (13)" */
				res = g_strdup_printf (C_("BirthdaySummary", "%s (%d)"), summary ? summary : "", i_cal_time_get_year (dtstart) - since_year);
			}

			g_clear_object (&dtstart);
			g_free (since_year_str);
		}
	}

	if (!res)
		res = g_strdup (summary ? summary : "");

	g_clear_object (&prop);

	e_cal_model_until_sanitize_text_value (res, -1);

	return res;
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
GdkRGBA
get_today_background (const GdkRGBA base_background)
{
	GdkRGBA res = base_background;

	if (res.red > 0.5) {
		/* light yellow for a light theme */
		res.red = 1.0;
		res.green = 1.0;
		res.blue = 0.75;
		res.alpha = 1.0;
	} else {
		/* dark yellow for a dark theme */
		res.red = 0.25;
		res.green = 0.25;
		res.blue = 0.0;
		res.alpha = 1.0;
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
	ECalendarViewClass *klass;

	g_return_val_if_fail (E_IS_CALENDAR_VIEW (cal_view), NULL);

	klass = E_CALENDAR_VIEW_GET_CLASS (cal_view);
	g_return_val_if_fail (klass != NULL, NULL);

	if (klass->get_description_text)
		return klass->get_description_text (cal_view);

	return NULL;
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

gboolean
e_calendar_view_get_allow_event_dnd (ECalendarView *cal_view)
{
	g_return_val_if_fail (E_IS_CALENDAR_VIEW (cal_view), FALSE);

	return cal_view->priv->allow_event_dnd;
}

void
e_calendar_view_set_allow_event_dnd (ECalendarView *cal_view,
				     gboolean allow)
{
	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	if ((cal_view->priv->allow_event_dnd ? 1 : 0) == (allow ? 1 : 0))
		return;

	cal_view->priv->allow_event_dnd = allow;

	g_object_notify (G_OBJECT (cal_view), "allow-event-dnd");
}
