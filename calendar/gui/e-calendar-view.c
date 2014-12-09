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
#include "e-cal-model-calendar.h"
#include "e-calendar-view.h"
#include "ea-cal-view.h"
#include "itip-utils.h"
#include "dialogs/comp-editor-util.h"
#include "dialogs/delete-comp.h"
#include "dialogs/delete-error.h"
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
	/* The GnomeCalendar we are associated to */
	GnomeCalendar *calendar;

	/* The calendar model we are monitoring */
	ECalModel *model;

	gchar *default_category;
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
	USER_CREATED,
	OPEN_EVENT,
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
                                CalObjModType mod)
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

	if (mod == CALOBJ_MOD_ALL)
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
                            ECalendarViewEvent *event)
{
	ECalModel *model;
	ECalComponent *comp;
	ECalComponentVType vtype;
	ESourceRegistry *registry;
	gboolean delete = TRUE;
	GError *error = NULL;

	if (!is_comp_data_valid (event))
		return;

	model = e_calendar_view_get_model (cal_view);
	registry = e_cal_model_get_registry (model);

	comp = e_cal_component_new ();
	e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (event->comp_data->icalcomp));
	vtype = e_cal_component_get_vtype (comp);

	/*FIXME remove it once the we dont set the recurrence id for all the generated instances */
	if (!e_cal_client_check_recurrences_no_master (event->comp_data->client))
		e_cal_component_set_recurid (comp, NULL);

	/*FIXME Retract should be moved to Groupwise features plugin */
	if (calendar_view_check_for_retract (comp, event->comp_data->client)) {
		gchar *retract_comment = NULL;
		gboolean retract = FALSE;

		delete = prompt_retract_dialog (comp, &retract_comment, GTK_WIDGET (cal_view), &retract);
		if (retract) {
			GSList *users = NULL;
			icalcomponent *icalcomp = NULL, *mod_comp = NULL;

			calendar_view_add_retract_data (
				comp, retract_comment, CALOBJ_MOD_ALL);
			icalcomp = e_cal_component_get_icalcomponent (comp);
			icalcomponent_set_method (icalcomp, ICAL_METHOD_CANCEL);
			e_cal_client_send_objects_sync (
				event->comp_data->client, icalcomp,
				&users, &mod_comp, NULL, &error);
			if (error != NULL) {
				delete_error_dialog (error, E_CAL_COMPONENT_EVENT);
				g_clear_error (&error);
			} else {

				if (mod_comp)
					icalcomponent_free (mod_comp);

				if (users) {
					g_slist_foreach (users, (GFunc) g_free, NULL);
					g_slist_free (users);
				}
			}
		}
	} else if (e_cal_model_get_confirm_delete (model))
		delete = delete_component_dialog (
			comp, FALSE, 1, vtype, GTK_WIDGET (cal_view));

	if (delete) {
		const gchar *uid;
		gchar *rid = NULL;

		if ((itip_organizer_is_user (registry, comp, event->comp_data->client) ||
		     itip_sentby_is_user (registry, comp, event->comp_data->client))
		    && cancel_component_dialog ((GtkWindow *) gtk_widget_get_toplevel (GTK_WIDGET (cal_view)),
						event->comp_data->client,
						comp, TRUE))
			itip_send_comp (
				registry, E_CAL_COMPONENT_METHOD_CANCEL,
				comp, event->comp_data->client, NULL, NULL,
				NULL, TRUE, FALSE);

		e_cal_component_get_uid (comp, &uid);
		if (!uid || !*uid) {
			g_object_unref (comp);
			return;
		}
		rid = e_cal_component_get_recurid_as_string (comp);
		if (e_cal_util_component_is_instance (event->comp_data->icalcomp) || e_cal_util_component_has_recurrences (event->comp_data->icalcomp))
			e_cal_client_remove_object_sync (
				event->comp_data->client, uid,
				rid, CALOBJ_MOD_ALL, NULL, &error);
		else
			e_cal_client_remove_object_sync (event->comp_data->client, uid, NULL, CALOBJ_MOD_THIS, NULL, &error);

		delete_error_dialog (error, E_CAL_COMPONENT_EVENT);
		g_clear_error (&error);
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

	g_clear_object (&priv->calendar);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_calendar_view_parent_class)->dispose (object);
}

static void
calendar_view_finalize (GObject *object)
{
	ECalendarViewPrivate *priv;

	priv = E_CALENDAR_VIEW_GET_PRIVATE (object);

	g_free (priv->default_category);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_calendar_view_parent_class)->finalize (object);
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

#if 0  /* KILL-BONOBO */
	e_calendar_view_set_status_message (cal_view, _("Deleting selected objects"), -1);
#endif

	e_selectable_copy_clipboard (selectable);

	for (l = selected; l != NULL; l = g_list_next (l)) {
		ECalendarViewEvent *event = (ECalendarViewEvent *) l->data;

		priv->selected_cut_list = g_slist_prepend (priv->selected_cut_list, g_object_ref (event->comp_data));
	}

#if 0  /* KILL-BONOBO */
	e_calendar_view_set_status_message (cal_view, NULL, -1);
#endif

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

static gboolean
clipboard_get_calendar_data (ECalendarView *cal_view,
                             const gchar *text,
                             GSList **copied_list)
{
	icalcomponent *icalcomp;
	icalcomponent_kind kind;
	time_t selected_time_start, selected_time_end;
	icaltimezone *default_zone;
	ECalClient *client;
	gboolean in_top_canvas, ret = FALSE;

	g_return_val_if_fail (E_IS_CALENDAR_VIEW (cal_view), FALSE);

	if (!text || !*text)
		return FALSE;

	icalcomp = icalparser_parse_string (text);
	if (!icalcomp)
		return FALSE;

	/* check the type of the component */
	/* FIXME An error dialog if we return? */
	kind = icalcomponent_isa (icalcomp);
	if (kind != ICAL_VCALENDAR_COMPONENT && kind != ICAL_VEVENT_COMPONENT)
		return FALSE;

	default_zone = e_cal_model_get_timezone (cal_view->priv->model);
	client = e_cal_model_ref_default_client (cal_view->priv->model);

#if 0  /* KILL-BONOBO */
	e_calendar_view_set_status_message (cal_view, _("Updating objects"), -1);
#endif
	e_calendar_view_get_selected_time_range (cal_view, &selected_time_start, &selected_time_end);

	if ((selected_time_end - selected_time_start) == 60 * 60 * 24)
		in_top_canvas = TRUE;
	else
		in_top_canvas = FALSE;

	if (kind == ICAL_VCALENDAR_COMPONENT) {
		icalcomponent *subcomp;

		/* add timezones first, to have them ready */
		for (subcomp = icalcomponent_get_first_component (icalcomp, ICAL_VTIMEZONE_COMPONENT);
		     subcomp;
		     subcomp = icalcomponent_get_next_component (icalcomp, ICAL_VTIMEZONE_COMPONENT)) {
			icaltimezone *zone;
			GError *error = NULL;

			zone = icaltimezone_new ();
			icaltimezone_set_component (zone, subcomp);
			e_cal_client_add_timezone_sync (
				client, zone, NULL, &error);
			if (error != NULL) {
				icalproperty *tzidprop;
				const gchar *tzid;

				tzidprop = icalcomponent_get_first_property (
					subcomp, ICAL_TZID_PROPERTY);
				if (tzidprop != NULL)
					tzid = icalproperty_get_tzid (tzidprop);
				else
					tzid = "???";

				g_warning (
					"%s: Add zone '%s' failed. %s",
					G_STRFUNC, tzid, error->message);

				g_error_free (error);
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

			ret = e_calendar_view_add_event (cal_view, client, selected_time_start, default_zone, subcomp, in_top_canvas);
			if (!ret)
				break;

			if (copied_list)
				*copied_list = g_slist_prepend (*copied_list, g_strdup (icalcomponent_get_uid (subcomp)));
		}

		icalcomponent_free (icalcomp);
	} else {
		ret = e_calendar_view_add_event (cal_view, client, selected_time_start, default_zone, icalcomp, in_top_canvas);
		if (ret && copied_list)
			*copied_list = g_slist_prepend (*copied_list, g_strdup (icalcomponent_get_uid (icalcomp)));
	}

	g_object_unref (client);

	return ret;

#if 0  /* KILL-BONOBO */
	e_calendar_view_set_status_message (cal_view, NULL, -1);
#endif
}

static void
calendar_view_paste_clipboard (ESelectable *selectable)
{
	ECalModel *model;
	ECalendarView *cal_view;
	ECalendarViewPrivate *priv;
	ESourceRegistry *registry;
	GtkClipboard *clipboard;

	cal_view = E_CALENDAR_VIEW (selectable);
	priv = cal_view->priv;

	model = e_calendar_view_get_model (cal_view);
	registry = e_cal_model_get_registry (model);

	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);

	/* Paste text into an event being edited. */
	if (gtk_clipboard_wait_is_text_available (clipboard)) {
		ECalendarViewClass *class;

		class = E_CALENDAR_VIEW_GET_CLASS (cal_view);
		g_return_if_fail (class->paste_text != NULL);

		class->paste_text (cal_view);

	/* Paste iCalendar data into the view. */
	} else if (e_clipboard_wait_is_calendar_available (clipboard)) {
		gchar *calendar_source;
		GSList *copied_list = NULL, *l;

		calendar_source = e_clipboard_wait_for_calendar (clipboard);

		if (priv->selected_cut_list)
			clipboard_get_calendar_data (cal_view, calendar_source, &copied_list);
		else
			clipboard_get_calendar_data (cal_view, calendar_source, NULL);

		if (copied_list && priv->selected_cut_list) {
			for (l = priv->selected_cut_list; l != NULL; l = l->next) {
				ECalComponent *comp;
				ECalModelComponent *comp_data = (ECalModelComponent *) l->data;
				const gchar *uid;
				GError *error = NULL;
				GSList *found = NULL;

				/* Remove them one by one after ensuring it has been copied to the destination successfully */
				found = g_slist_find_custom (copied_list, icalcomponent_get_uid (comp_data->icalcomp), (GCompareFunc) strcmp);
				if (!found)
					continue;

				g_free (found->data);
				copied_list = g_slist_delete_link (copied_list, found);

				comp = e_cal_component_new ();
				e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (comp_data->icalcomp));

				if ((itip_organizer_is_user (registry, comp, comp_data->client) ||
							itip_sentby_is_user (registry, comp, comp_data->client))
						&& cancel_component_dialog ((GtkWindow *) gtk_widget_get_toplevel (GTK_WIDGET (cal_view)),
							comp_data->client, comp, TRUE))
					itip_send_comp (
						registry,
						E_CAL_COMPONENT_METHOD_CANCEL,
						comp, comp_data->client,
						NULL, NULL, NULL, TRUE, FALSE);

				e_cal_component_get_uid (comp, &uid);
				if (e_cal_component_is_instance (comp)) {
					gchar *rid = NULL;
					icalcomponent *icalcomp;

					/* when cutting detached instances, only cut that instance */
					rid = e_cal_component_get_recurid_as_string (comp);
					e_cal_client_get_object_sync (
						comp_data->client, uid, rid,
						&icalcomp, NULL, NULL);
					if (icalcomp != NULL) {
						e_cal_client_remove_object_sync (
							comp_data->client, uid, rid,
							CALOBJ_MOD_THIS, NULL, &error);
						icalcomponent_free (icalcomp);
					} else {
						e_cal_client_remove_object_sync (
							comp_data->client, uid, NULL,
							CALOBJ_MOD_ALL, NULL, &error);
					}
					g_free (rid);
				} else {
					e_cal_client_remove_object_sync (
						comp_data->client, uid, NULL,
						CALOBJ_MOD_ALL, NULL, &error);
				}
				delete_error_dialog (error, E_CAL_COMPONENT_EVENT);

				g_clear_error (&error);
				g_object_unref (comp);
			}
		}

		if (priv->selected_cut_list) {
			g_slist_foreach (priv->selected_cut_list, (GFunc) g_object_unref, NULL);
			g_slist_free (priv->selected_cut_list);
		}
		priv->selected_cut_list = NULL;

		g_free (calendar_source);

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

		calendar_view_delete_event (cal_view, event);
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
	object_class->finalize = calendar_view_finalize;
	object_class->constructed = calendar_view_constructed;

	class->selection_changed = NULL;
	class->selected_time_changed = NULL;
	class->event_changed = NULL;
	class->event_added = NULL;
	class->user_created = NULL;

	class->get_selected_events = NULL;
	class->get_selected_time_range = NULL;
	class->set_selected_time_range = NULL;
	class->get_visible_time_range = NULL;
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

	signals[USER_CREATED] = g_signal_new (
		"user-created",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ECalendarViewClass, user_created),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1, G_TYPE_OBJECT);

	signals[OPEN_EVENT] = g_signal_new (
		"open-event",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (ECalendarViewClass, open_event),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

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

gboolean
e_calendar_view_add_event (ECalendarView *cal_view,
                           ECalClient *client,
                           time_t dtstart,
                           icaltimezone *default_zone,
                           icalcomponent *icalcomp,
                           gboolean in_top_canvas)
{
	ECalModel *model;
	ECalComponent *comp;
	ESourceRegistry *registry;
	struct icaltimetype itime, old_dtstart, old_dtend;
	time_t tt_start, tt_end, new_dtstart = 0;
	struct icaldurationtype ic_dur, ic_oneday;
	gchar *uid;
	gint start_offset, end_offset;
	gboolean all_day_event = FALSE;
	GnomeCalendarViewType view_type;
	gboolean ret = TRUE;
	GError *error = NULL;

	model = e_calendar_view_get_model (cal_view);
	registry = e_cal_model_get_registry (model);

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

	view_type = gnome_calendar_get_view (cal_view->priv->calendar);

	switch (view_type) {
	case GNOME_CAL_DAY_VIEW:
	case GNOME_CAL_WORK_WEEK_VIEW:
		if (start_offset == 0 && end_offset == 0 && in_top_canvas)
			all_day_event = TRUE;

		if (all_day_event) {
			ic_dur = ic_oneday;
		} else if (icaldurationtype_as_int (ic_dur) >= 60 *60 *24
				&& !in_top_canvas) {
			/* copy & paste from top canvas to main canvas */
			gint time_divisions;

			time_divisions = e_calendar_view_get_time_divisions (cal_view);
			ic_dur = icaldurationtype_from_int (time_divisions * 60);
		}

		if (in_top_canvas)
			new_dtstart = dtstart + start_offset * 60;
		else
			new_dtstart = dtstart;
		break;
	case GNOME_CAL_WEEK_VIEW:
	case GNOME_CAL_MONTH_VIEW:
	case GNOME_CAL_LIST_VIEW:
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
		break;
	default:
		g_return_val_if_reached (FALSE);
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

	/* FIXME The new uid stuff can go away once we actually set it in the backend */
	uid = e_cal_component_gen_uid ();
	comp = e_cal_component_new ();
	e_cal_component_set_icalcomponent (
		comp, icalcomponent_new_clone (icalcomp));
	e_cal_component_set_uid (comp, uid);
	g_free (uid);

	e_cal_component_commit_sequence (comp);

	uid = NULL;
	e_cal_client_create_object_sync (
		client, e_cal_component_get_icalcomponent (comp),
		&uid, NULL, &error);
	if (error == NULL) {
		gboolean strip_alarms = TRUE;

		if (uid) {
			e_cal_component_set_uid (comp, uid);
			g_free (uid);
		}

		if ((itip_organizer_is_user (registry, comp, client) ||
		     itip_sentby_is_user (registry, comp, client)) &&
			send_component_dialog (
				(GtkWindow *) gtk_widget_get_toplevel (
					GTK_WIDGET (cal_view)),
				client, comp, TRUE, &strip_alarms, NULL)) {
			itip_send_comp (
				registry, E_CAL_COMPONENT_METHOD_REQUEST,
				comp, client, NULL, NULL, NULL, strip_alarms,
				FALSE);
		}
	} else {
		g_message (
			"%s: Could not create the object! %s",
			G_STRFUNC, error->message);
		g_error_free (error);
		ret = FALSE;
	}

	g_object_unref (comp);
	return ret;
}

GnomeCalendar *
e_calendar_view_get_calendar (ECalendarView *cal_view)
{
	g_return_val_if_fail (E_IS_CALENDAR_VIEW (cal_view), NULL);

	return cal_view->priv->calendar;
}

void
e_calendar_view_set_calendar (ECalendarView *cal_view,
                              GnomeCalendar *calendar)
{
	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	if (calendar)
		g_object_ref (calendar);

	if (cal_view->priv->calendar)
		g_object_unref (cal_view->priv->calendar);

	cal_view->priv->calendar = calendar;
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

const gchar *
e_calendar_view_get_default_category (ECalendarView *cal_view)
{
	g_return_val_if_fail (E_IS_CALENDAR_VIEW (cal_view), NULL);

	return cal_view->priv->default_category;
}

/**
 * e_calendar_view_set_default_category
 * @cal_view: A calendar view.
 * @category: Default category name or NULL for no category.
 *
 * Sets the default category that will be used when creating new calendar
 * components from the given calendar view.
 */
void
e_calendar_view_set_default_category (ECalendarView *cal_view,
                                      const gchar *category)
{
	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	g_free (cal_view->priv->default_category);
	cal_view->priv->default_category = g_strdup (category);
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
	GList *selected;
	ECalModel *model;
	ECalComponent *comp;
	ECalendarViewEvent *event;
	ECalComponentVType vtype;
	ESourceRegistry *registry;
	gboolean delete = TRUE;
	GError *error = NULL;

	model = e_calendar_view_get_model (cal_view);
	registry = e_cal_model_get_registry (model);

	selected = e_calendar_view_get_selected_events (cal_view);
	if (!selected)
		return;
	event = (ECalendarViewEvent *) selected->data;
	if (!is_comp_data_valid (event))
		return;

	comp = e_cal_component_new ();
	e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (event->comp_data->icalcomp));
	vtype = e_cal_component_get_vtype (comp);

	/*FIXME Retract should be moved to Groupwise features plugin */
	if (calendar_view_check_for_retract (comp, event->comp_data->client)) {
		gchar *retract_comment = NULL;
		gboolean retract = FALSE;

		delete = prompt_retract_dialog (comp, &retract_comment, GTK_WIDGET (cal_view), &retract);
		if (retract) {
			GSList *users = NULL;
			icalcomponent *icalcomp = NULL, *mod_comp = NULL;

			calendar_view_add_retract_data (
				comp, retract_comment, CALOBJ_MOD_THIS);
			icalcomp = e_cal_component_get_icalcomponent (comp);
			icalcomponent_set_method (icalcomp, ICAL_METHOD_CANCEL);
			e_cal_client_send_objects_sync (
				event->comp_data->client, icalcomp,
				&users, &mod_comp, NULL, &error);
			if (error != NULL) {
				delete_error_dialog (error, E_CAL_COMPONENT_EVENT);
				g_clear_error (&error);
			} else {
				if (mod_comp)
					icalcomponent_free (mod_comp);
				if (users) {
					g_slist_foreach (users, (GFunc) g_free, NULL);
					g_slist_free (users);
				}
			}
		}
	} else if (e_cal_model_get_confirm_delete (model))
		delete = delete_component_dialog (
			comp, FALSE, 1, vtype, GTK_WIDGET (cal_view));

	if (delete) {
		const gchar *uid;
		gchar *rid = NULL;
		ECalComponentDateTime dt;
		icaltimezone *zone = NULL;
		gboolean is_instance = FALSE;

		e_cal_component_get_uid (comp, &uid);
		e_cal_component_get_dtstart (comp, &dt);
		is_instance = e_cal_component_is_instance (comp);

		if (dt.tzid) {
			GError *error = NULL;

			e_cal_client_get_timezone_sync (event->comp_data->client, dt.tzid, &zone, NULL, &error);
			if (error != NULL) {
				zone = e_calendar_view_get_timezone (cal_view);
				g_clear_error (&error);
			}
		} else
			zone = e_calendar_view_get_timezone (cal_view);

		if (is_instance)
			rid = e_cal_component_get_recurid_as_string (comp);

		e_cal_component_free_datetime (&dt);

		if ((itip_organizer_is_user (registry, comp, event->comp_data->client) ||
		     itip_sentby_is_user (registry, comp, event->comp_data->client))
				&& cancel_component_dialog ((GtkWindow *) gtk_widget_get_toplevel (GTK_WIDGET (cal_view)),
					event->comp_data->client,
					comp, TRUE) && !e_cal_client_check_save_schedules (event->comp_data->client)) {
			if (!e_cal_component_is_instance (comp)) {
				ECalComponentRange range;

				/* set the recurrence ID of the object we send */
				range.type = E_CAL_COMPONENT_RANGE_SINGLE;
				e_cal_component_get_dtstart (comp, &range.datetime);
				range.datetime.value->is_date = 1;
				e_cal_component_set_recurid (comp, &range);

				e_cal_component_free_datetime (&range.datetime);
			}

			itip_send_comp (
				registry, E_CAL_COMPONENT_METHOD_CANCEL,
				comp, event->comp_data->client, NULL, NULL,
				NULL, TRUE, FALSE);
		}

		if (is_instance)
			e_cal_client_remove_object_sync (event->comp_data->client, uid, rid, CALOBJ_MOD_THIS, NULL, &error);
		else {
			struct icaltimetype instance_rid;

			instance_rid = icaltime_from_timet_with_zone (
				event->comp_data->instance_start,
				TRUE, zone ? zone : icaltimezone_get_utc_timezone ());
			e_cal_util_remove_instances (event->comp_data->icalcomp, instance_rid, CALOBJ_MOD_THIS);
			e_cal_client_modify_object_sync (event->comp_data->client, event->comp_data->icalcomp, CALOBJ_MOD_THIS, NULL, &error);
		}

		delete_error_dialog (error, E_CAL_COMPONENT_EVENT);
		g_clear_error (&error);
		g_free (rid);
	}

	/* free memory */
	g_list_free (selected);
	g_object_unref (comp);
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
 * e_calendar_view_new_appointment_for
 * @cal_view: A calendar view.
 * @dtstart: A Unix time_t that marks the beginning of the appointment.
 * @dtend: A Unix time_t that marks the end of the appointment.
 * @all_day: If TRUE, the dtstart and dtend are expanded to cover
 * the entire day, and the event is set to TRANSPARENT.
 * @meeting: Whether the appointment is a meeting or not.
 *
 * Opens an event editor dialog for a new appointment.
 */
void
e_calendar_view_new_appointment_for (ECalendarView *cal_view,
                                     time_t dtstart,
                                     time_t dtend,
                                     gboolean all_day,
                                     gboolean meeting)
{
	ECalendarViewPrivate *priv;
	struct icaltimetype itt;
	ECalComponentDateTime dt;
	ECalComponent *comp;
	icalcomponent *icalcomp;
	ECalComponentTransparency transparency;
	ECalClient *default_client = NULL;
	gpointer parent;
	guint32 flags = 0;

	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	parent = gtk_widget_get_toplevel (GTK_WIDGET (cal_view));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	priv = cal_view->priv;

	default_client = e_cal_model_ref_default_client (priv->model);
	g_return_if_fail (default_client != NULL);

	dt.value = &itt;
	if (all_day)
		dt.tzid = NULL;
	else
		dt.tzid = icaltimezone_get_tzid (e_cal_model_get_timezone (cal_view->priv->model));

	icalcomp = e_cal_model_create_component_with_defaults (priv->model, all_day);
	comp = e_cal_component_new ();
	e_cal_component_set_icalcomponent (comp, icalcomp);

	/* DTSTART, DTEND */
	itt = icaltime_from_timet_with_zone (dtstart, FALSE, e_cal_model_get_timezone (cal_view->priv->model));
	if (all_day) {
		itt.hour = itt.minute = itt.second = 0;
		itt.is_date = TRUE;
	}
	e_cal_component_set_dtstart (comp, &dt);

	itt = icaltime_from_timet_with_zone (dtend, FALSE, e_cal_model_get_timezone (cal_view->priv->model));
	if (all_day) {
		/* We round it up to the end of the day, unless it is
		 * already set to midnight */
		if (itt.hour != 0 || itt.minute != 0 || itt.second != 0) {
			icaltime_adjust (&itt, 1, 0, 0, 0);
		}
		itt.hour = itt.minute = itt.second = 0;
		itt.is_date = TRUE;
	}
	e_cal_component_set_dtend (comp, &dt);

	/* TRANSPARENCY */
	transparency = all_day ? E_CAL_COMPONENT_TRANSP_TRANSPARENT
		: E_CAL_COMPONENT_TRANSP_OPAQUE;
	e_cal_component_set_transparency (comp, transparency);

	/* CATEGORY */
	e_cal_component_set_categories (comp, priv->default_category);

	/* edit the object */
	e_cal_component_commit_sequence (comp);

	flags |= COMP_EDITOR_NEW_ITEM;
	if (meeting) {
		flags |= COMP_EDITOR_MEETING;
		flags |= COMP_EDITOR_USER_ORG;
	}

	e_calendar_view_open_event_with_flags (
		cal_view, default_client, icalcomp, flags);

	g_object_unref (comp);

	g_object_unref (default_client);
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

	e_calendar_view_new_appointment_for (cal_view, dtstart, dtend, all_day, meeting);
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
	e_calendar_view_emit_user_created (cal_view, comp_editor_get_client (ce));
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

void
e_calendar_view_modify_and_send (ECalendarView *cal_view,
                                 ECalComponent *comp,
                                 ECalClient *client,
                                 CalObjModType mod,
                                 GtkWindow *toplevel,
                                 gboolean new)
{
	ECalModel *model;
	ESourceRegistry *registry;
	gboolean only_new_attendees = FALSE;
	gboolean strip_alarms = TRUE;

	if (e_calendar_view_modify (cal_view, comp, client, mod)) {
		model = e_calendar_view_get_model (cal_view);
		registry = e_cal_model_get_registry (model);

		if ((itip_organizer_is_user (registry, comp, client) ||
		     itip_sentby_is_user (registry, comp, client)) &&
		    send_component_dialog (toplevel, client, comp, new, &strip_alarms, &only_new_attendees))
			e_calendar_view_send (cal_view, comp, client, mod, toplevel, strip_alarms, only_new_attendees);
	}
}

gboolean
e_calendar_view_modify (ECalendarView *cal_view,
                        ECalComponent *comp,
                        ECalClient *client,
                        CalObjModType mod)
{
	GError *error = NULL;
	gboolean ret;

	g_return_val_if_fail (E_IS_CALENDAR_VIEW (cal_view), FALSE);

	e_cal_component_commit_sequence (comp);

	ret = e_cal_client_modify_object_sync (
		client, e_cal_component_get_icalcomponent (comp),
		mod, NULL, &error);

	if (error != NULL) {
		g_message (
			G_STRLOC ": Could not update the object! %s",
			error->message);

		g_error_free (error);
	}

	return ret;
}

void
e_calendar_view_send (ECalendarView *cal_view,
                      ECalComponent *comp,
                      ECalClient *client,
                      CalObjModType mod,
                      GtkWindow *toplevel,
                      gboolean strip_alarms,
                      gboolean only_new_attendees)
{
	ESourceRegistry *registry;
	ECalModel *model;
	ECalComponent *send_comp = NULL;

	if (!itip_component_has_recipients (comp))
		return;

	if (mod == CALOBJ_MOD_ALL && e_cal_component_is_instance (comp)) {
		/* Ensure we send the master object, not the instance only */
		icalcomponent *icalcomp = NULL;
		const gchar *uid = NULL;

		e_cal_component_get_uid (comp, &uid);
		e_cal_client_get_object_sync (
			client, uid, NULL, &icalcomp, NULL, NULL);
		if (icalcomp != NULL) {
			send_comp = e_cal_component_new ();
			if (!e_cal_component_set_icalcomponent (send_comp, icalcomp)) {
				icalcomponent_free (icalcomp);
				g_object_unref (send_comp);
				send_comp = NULL;
			} else if (only_new_attendees) {
				/* copy new-attendees information too if required for later use */
				comp_editor_copy_new_attendees (send_comp, comp);
			}
		}
	}

	model = e_calendar_view_get_model (cal_view);
	registry = e_cal_model_get_registry (model);
	itip_send_comp (
		registry, E_CAL_COMPONENT_METHOD_REQUEST,
		send_comp ? send_comp : comp, client, NULL,
		NULL, NULL, strip_alarms, only_new_attendees);

	if (send_comp)
		g_object_unref (send_comp);
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

	return e_datetime_format_format_tm ("calendar", "table", DTFormatKindDateTime, &tmp_tm);
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
	gchar *tmp, *tmp1, *tmp2;
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

void
e_calendar_view_emit_user_created (ECalendarView *cal_view,
                                   ECalClient *where_was_created)
{
	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	g_signal_emit (cal_view, signals[USER_CREATED], 0, where_was_created);
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
