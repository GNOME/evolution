/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * Author : 
 *  Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright 2003, Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of version 2 of the GNU General Public 
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <config.h>
#include <string.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkinvisible.h>
#include <gtk/gtkstock.h>
#include <libgnome/gnome-i18n.h>
#include <gal/util/e-util.h>
#include "e-util/e-dialog-utils.h"
#include "cal-util/cal-util-marshal.h"
#include "cal-util/timeutil.h"
#include "evolution-activity-client.h"
#include "calendar-commands.h"
#include "calendar-config.h"
#include "comp-util.h"
#include "e-cal-model-calendar.h"
#include "e-cal-view.h"
#include "itip-utils.h"
#include "dialogs/delete-comp.h"
#include "dialogs/delete-error.h"
#include "dialogs/send-comp.h"
#include "dialogs/cancel-comp.h"
#include "dialogs/recur-comp.h"
#include "print.h"
#include "goto.h"
#include "ea-calendar.h"

/* Used for the status bar messages */
#define EVOLUTION_CALENDAR_PROGRESS_IMAGE "evolution-calendar-mini.png"
static GdkPixbuf *progress_icon[2] = { NULL, NULL };

struct _ECalViewPrivate {
	/* The GnomeCalendar we are associated to */
	GnomeCalendar *calendar;

	/* The calendar model we are monitoring */
	ECalModel *model;

	/* The activity client used to show messages on the status bar. */
	EvolutionActivityClient *activity;

	/* the invisible widget to manage the clipboard selections */
	GtkWidget *invisible;
	gchar *clipboard_selection;

	/* The popup menu */
	EPopupMenu *view_menu;

	/* The timezone. */
	icaltimezone *zone;
};

static void e_cal_view_class_init (ECalViewClass *klass);
static void e_cal_view_init (ECalView *cal_view, ECalViewClass *klass);
static void e_cal_view_destroy (GtkObject *object);

static GObjectClass *parent_class = NULL;
static GdkAtom clipboard_atom = GDK_NONE;

/* Signal IDs */
enum {
	SELECTION_CHANGED,
	TIMEZONE_CHANGED,
	EVENT_CHANGED,
	EVENT_ADDED,
	LAST_SIGNAL
};

static guint e_cal_view_signals[LAST_SIGNAL] = { 0 };

static void
e_cal_view_class_init (ECalViewClass *klass)
{
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	/* Create class' signals */
	e_cal_view_signals[SELECTION_CHANGED] =
		g_signal_new ("selection_changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ECalViewClass, selection_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	e_cal_view_signals[TIMEZONE_CHANGED] =
		g_signal_new ("timezone_changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ECalViewClass, timezone_changed),
			      NULL, NULL,
			      cal_util_marshal_VOID__POINTER_POINTER,
			      G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_POINTER);

	e_cal_view_signals[EVENT_CHANGED] =
		g_signal_new ("event_changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (ECalViewClass, event_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1,
			      G_TYPE_POINTER);

	e_cal_view_signals[EVENT_ADDED] =
		g_signal_new ("event_added",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (ECalViewClass, event_added),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1,
			      G_TYPE_POINTER);

	/* Method override */
	object_class->destroy = e_cal_view_destroy;

	klass->selection_changed = NULL;
	klass->event_changed = NULL;
	klass->event_added = NULL;

	klass->get_selected_events = NULL;
	klass->get_selected_time_range = NULL;
	klass->set_selected_time_range = NULL;
	klass->get_visible_time_range = NULL;
	klass->update_query = NULL;

	/* clipboard atom */
	if (!clipboard_atom)
		clipboard_atom = gdk_atom_intern ("CLIPBOARD", FALSE);

	/* init the accessibility support for e_day_view */
 	e_cal_view_a11y_init ();
}

static void
model_changed_cb (ETableModel *etm, gpointer user_data)
{
	ECalView *cal_view = E_CAL_VIEW (user_data);

	e_cal_view_update_query (cal_view);
}

static void
model_row_changed_cb (ETableModel *etm, int row, gpointer user_data)
{
	ECalView *cal_view = E_CAL_VIEW (user_data);

	e_cal_view_update_query (cal_view);
}

static void
model_rows_changed_cb (ETableModel *etm, int row, int count, gpointer user_data)
{
	ECalView *cal_view = E_CAL_VIEW (user_data);

	e_cal_view_update_query (cal_view);
}

static void
selection_get (GtkWidget *invisible,
	       GtkSelectionData *selection_data,
	       guint info,
	       guint time_stamp,
	       ECalView *cal_view)
{
	if (cal_view->priv->clipboard_selection != NULL) {
		gtk_selection_data_set (selection_data,
					GDK_SELECTION_TYPE_STRING,
					8,
					cal_view->priv->clipboard_selection,
					strlen (cal_view->priv->clipboard_selection));
	}
}

static void
selection_clear_event (GtkWidget *invisible,
		       GdkEventSelection *event,
		       ECalView *cal_view)
{
	if (cal_view->priv->clipboard_selection != NULL) {
		g_free (cal_view->priv->clipboard_selection);
		cal_view->priv->clipboard_selection = NULL;
	}
}

static void
selection_received (GtkWidget *invisible,
		    GtkSelectionData *selection_data,
		    guint time,
		    ECalView *cal_view)
{
	char *comp_str, *default_tzid;
	icalcomponent *icalcomp;
	icalcomponent_kind kind;
	CalComponent *comp;
	time_t selected_time_start, selected_time_end;
	struct icaltimetype itime;
	time_t tt_start, tt_end;
	struct icaldurationtype ic_dur;
	char *uid;
	icaltimezone *default_zone;
	CalClient *client;

	g_return_if_fail (E_IS_CAL_VIEW (cal_view));

	if (selection_data->length < 0 ||
	    selection_data->type != GDK_SELECTION_TYPE_STRING) {
		return;
	}

	comp_str = (char *) selection_data->data;
	icalcomp = icalparser_parse_string ((const char *) comp_str);
	if (!icalcomp)
		return;

	default_tzid = calendar_config_get_timezone ();
	client = e_cal_model_get_default_client (cal_view->priv->model);
	cal_client_get_timezone (client, default_tzid, &default_zone);

	/* check the type of the component */
	kind = icalcomponent_isa (icalcomp);
	if (kind != ICAL_VCALENDAR_COMPONENT &&
	    kind != ICAL_VEVENT_COMPONENT &&
	    kind != ICAL_VTODO_COMPONENT &&
	    kind != ICAL_VJOURNAL_COMPONENT) {
		return;
	}

	e_cal_view_set_status_message (cal_view, _("Updating objects"));
	e_cal_view_get_selected_time_range (cal_view, &selected_time_start, &selected_time_end);

	if (kind == ICAL_VCALENDAR_COMPONENT) {
		icalcomponent_kind child_kind;
		icalcomponent *subcomp;

		subcomp = icalcomponent_get_first_component (icalcomp, ICAL_ANY_COMPONENT);
		while (subcomp) {
			child_kind = icalcomponent_isa (subcomp);
			if (child_kind == ICAL_VEVENT_COMPONENT ||
			    child_kind == ICAL_VTODO_COMPONENT ||
			    child_kind == ICAL_VJOURNAL_COMPONENT) {
				tt_start = icaltime_as_timet (icalcomponent_get_dtstart (subcomp));
				tt_end = icaltime_as_timet (icalcomponent_get_dtend (subcomp));
				ic_dur = icaldurationtype_from_int (tt_end - tt_start);
				itime = icaltime_from_timet_with_zone (selected_time_start,
								       FALSE, default_zone);

				icalcomponent_set_dtstart (subcomp, itime);
				itime = icaltime_add (itime, ic_dur);
				icalcomponent_set_dtend (subcomp, itime);

				uid = cal_component_gen_uid ();
				comp = cal_component_new ();
				cal_component_set_icalcomponent (
					comp, icalcomponent_new_clone (subcomp));
				cal_component_set_uid (comp, uid);

				cal_client_update_object (client, comp);
				if (itip_organizer_is_user (comp, client) &&
				    send_component_dialog ((GtkWindow *) gtk_widget_get_toplevel (GTK_WIDGET (cal_view)),
							   client, comp, TRUE)) {
					itip_send_comp (CAL_COMPONENT_METHOD_REQUEST, comp,
							client, NULL);
				}

				free (uid);
				g_object_unref (comp);
			}
			subcomp = icalcomponent_get_next_component (
				icalcomp, ICAL_ANY_COMPONENT);
		}

		icalcomponent_free (icalcomp);

	}
	else {
		tt_start = icaltime_as_timet (icalcomponent_get_dtstart (icalcomp));
		tt_end = icaltime_as_timet (icalcomponent_get_dtend (icalcomp));
		ic_dur = icaldurationtype_from_int (tt_end - tt_start);
		itime = icaltime_from_timet_with_zone (selected_time_start, FALSE, default_zone);

		icalcomponent_set_dtstart (icalcomp, itime);
		itime = icaltime_add (itime, ic_dur);
		icalcomponent_set_dtend (icalcomp, itime);

		uid = cal_component_gen_uid ();
		comp = cal_component_new ();
		cal_component_set_icalcomponent (
			comp, icalcomponent_new_clone (icalcomp));
		cal_component_set_uid (comp, uid);

		cal_client_update_object (client, comp);
		if (itip_organizer_is_user (comp, client) &&
		    send_component_dialog ((GtkWindow *) gtk_widget_get_toplevel (GTK_WIDGET (cal_view)),
					   client, comp, TRUE)) {
			itip_send_comp (CAL_COMPONENT_METHOD_REQUEST, comp,
					client, NULL);
		}

		free (uid);
		g_object_unref (comp);
	}

	e_cal_view_set_status_message (cal_view, NULL);
}

static void
e_cal_view_init (ECalView *cal_view, ECalViewClass *klass)
{
	cal_view->priv = g_new0 (ECalViewPrivate, 1);

	cal_view->priv->model = (ECalModel *) e_cal_model_calendar_new ();

	/* Set up the invisible widget for the clipboard selections */
	cal_view->priv->invisible = gtk_invisible_new ();
	gtk_selection_add_target (cal_view->priv->invisible,
				  clipboard_atom,
				  GDK_SELECTION_TYPE_STRING,
				  0);
	g_signal_connect (cal_view->priv->invisible, "selection_get",
			  G_CALLBACK (selection_get), (gpointer) cal_view);
	g_signal_connect (cal_view->priv->invisible, "selection_clear_event",
			  G_CALLBACK (selection_clear_event), (gpointer) cal_view);
	g_signal_connect (cal_view->priv->invisible, "selection_received",
			  G_CALLBACK (selection_received), (gpointer) cal_view);

	cal_view->priv->clipboard_selection = NULL;
}

static void
e_cal_view_destroy (GtkObject *object)
{
	ECalView *cal_view = (ECalView *) object;

	g_return_if_fail (E_IS_CAL_VIEW (cal_view));

	if (cal_view->priv) {
		if (cal_view->priv->model) {
			g_signal_handlers_disconnect_by_func (cal_view->priv->model, model_changed_cb, cal_view);
			g_object_unref (cal_view->priv->model);
			cal_view->priv->model = NULL;
		}

		if (cal_view->priv->activity) {
			g_object_unref (cal_view->priv->activity);
			cal_view->priv->activity = NULL;
		}

		if (cal_view->priv->invisible) {
			gtk_widget_destroy (cal_view->priv->invisible);
			cal_view->priv->invisible = NULL;
		}

		if (cal_view->priv->clipboard_selection) {
			g_free (cal_view->priv->clipboard_selection);
			cal_view->priv->clipboard_selection = NULL;
		}

		g_free (cal_view->priv);
		cal_view->priv = NULL;
	}

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

E_MAKE_TYPE (e_cal_view, "ECalView", ECalView, e_cal_view_class_init,
	     e_cal_view_init, GTK_TYPE_TABLE);

GnomeCalendar *
e_cal_view_get_calendar (ECalView *cal_view)
{
	g_return_val_if_fail (E_IS_CAL_VIEW (cal_view), NULL);

	return cal_view->priv->calendar;
}

void
e_cal_view_set_calendar (ECalView *cal_view, GnomeCalendar *calendar)
{
	g_return_if_fail (E_IS_CAL_VIEW (cal_view));

	cal_view->priv->calendar = calendar;
}

ECalModel *
e_cal_view_get_model (ECalView *cal_view)
{
	g_return_val_if_fail (E_IS_CAL_VIEW (cal_view), NULL);

	return cal_view->priv->model;
}

void
e_cal_view_set_model (ECalView *cal_view, ECalModel *model)
{
	g_return_if_fail (E_IS_CAL_VIEW (cal_view));
	g_return_if_fail (E_IS_CAL_MODEL (model));

	if (cal_view->priv->model) {
		g_signal_handlers_disconnect_matched (cal_view->priv->model, G_SIGNAL_MATCH_DATA,
						      0, 0, 0, NULL, cal_view);
		g_object_unref (cal_view->priv->model);
	}

	cal_view->priv->model = model;
	g_object_ref (cal_view->priv->model);
	g_signal_connect (G_OBJECT (cal_view->priv->model), "model_changed", G_CALLBACK (model_changed_cb), cal_view);
	g_signal_connect (G_OBJECT (cal_view->priv->model), "model_row_changed", G_CALLBACK (model_row_changed_cb), cal_view);
	g_signal_connect (G_OBJECT (cal_view->priv->model), "model_rows_inserted", G_CALLBACK (model_rows_changed_cb), cal_view);
	g_signal_connect (G_OBJECT (cal_view->priv->model), "model_rows_deleted", G_CALLBACK (model_rows_changed_cb), cal_view);

	e_cal_view_update_query (cal_view);
}

icaltimezone *
e_cal_view_get_timezone (ECalView *cal_view)
{
	g_return_val_if_fail (E_IS_CAL_VIEW (cal_view), NULL);
	return cal_view->priv->zone;
}

void
e_cal_view_set_timezone (ECalView *cal_view, icaltimezone *zone)
{
	icaltimezone *old_zone;

	g_return_if_fail (E_IS_CAL_VIEW (cal_view));

	if (zone == cal_view->priv->zone)
		return;

	old_zone = cal_view->priv->zone;
	cal_view->priv->zone = zone;
	g_signal_emit (G_OBJECT (cal_view), e_cal_view_signals[TIMEZONE_CHANGED], 0,
		       old_zone, cal_view->priv->zone);
}

void
e_cal_view_set_status_message (ECalView *cal_view, const gchar *message)
{
	g_return_if_fail (E_IS_CAL_VIEW (cal_view));

	if (!message || !*message) {
		if (cal_view->priv->activity) {
			g_object_unref (cal_view->priv->activity);
			cal_view->priv->activity = NULL;
		}
	} else if (!cal_view->priv->activity) {
		int display;
		char *client_id = g_strdup_printf ("%p", cal_view);

		if (progress_icon[0] == NULL)
			progress_icon[0] = gdk_pixbuf_new_from_file (EVOLUTION_IMAGESDIR "/" EVOLUTION_CALENDAR_PROGRESS_IMAGE, NULL);

#if 0
		cal_view->priv->activity = evolution_activity_client_new (
			global_shell_client, client_id,
			progress_icon, message, TRUE, &display);
#endif

		g_free (client_id);
	} else
		evolution_activity_client_update (cal_view->priv->activity, message, -1.0);
}

GList *
e_cal_view_get_selected_events (ECalView *cal_view)
{
	g_return_val_if_fail (E_IS_CAL_VIEW (cal_view), NULL);

	if (E_CAL_VIEW_CLASS (G_OBJECT_GET_CLASS (cal_view))->get_selected_events)
		return E_CAL_VIEW_CLASS (G_OBJECT_GET_CLASS (cal_view))->get_selected_events (cal_view);

	return NULL;
}

void
e_cal_view_get_selected_time_range (ECalView *cal_view, time_t *start_time, time_t *end_time)
{
	g_return_if_fail (E_IS_CAL_VIEW (cal_view));

	if (E_CAL_VIEW_CLASS (G_OBJECT_GET_CLASS (cal_view))->get_selected_time_range) {
		E_CAL_VIEW_CLASS (G_OBJECT_GET_CLASS (cal_view))->get_selected_time_range (
			cal_view, start_time, end_time);
	}
}

void
e_cal_view_set_selected_time_range (ECalView *cal_view, time_t start_time, time_t end_time)
{
	g_return_if_fail (E_IS_CAL_VIEW (cal_view));

	if (E_CAL_VIEW_CLASS (G_OBJECT_GET_CLASS (cal_view))->set_selected_time_range) {
		E_CAL_VIEW_CLASS (G_OBJECT_GET_CLASS (cal_view))->set_selected_time_range (
			cal_view, start_time, end_time);
	}
}

gboolean
e_cal_view_get_visible_time_range (ECalView *cal_view, time_t *start_time, time_t *end_time)
{
	g_return_val_if_fail (E_IS_CAL_VIEW (cal_view), FALSE);

	if (E_CAL_VIEW_CLASS (G_OBJECT_GET_CLASS (cal_view))->get_visible_time_range) {
		return E_CAL_VIEW_CLASS (G_OBJECT_GET_CLASS (cal_view))->get_visible_time_range (
			cal_view, start_time, end_time);
	}

	return FALSE;
}

void
e_cal_view_update_query (ECalView *cal_view)
{
	g_return_if_fail (E_IS_CAL_VIEW (cal_view));

	e_cal_view_set_status_message (cal_view, _("Searching"));

	if (E_CAL_VIEW_CLASS (G_OBJECT_GET_CLASS (cal_view))->update_query) {
		E_CAL_VIEW_CLASS (G_OBJECT_GET_CLASS (cal_view))->update_query (cal_view);
	}

	e_cal_view_set_status_message (cal_view, NULL);
}

void
e_cal_view_cut_clipboard (ECalView *cal_view)
{
	GList *selected, *l;
	const char *uid;

	g_return_if_fail (E_IS_CAL_VIEW (cal_view));

	selected = e_cal_view_get_selected_events (cal_view);
	if (!selected)
		return;

	e_cal_view_set_status_message (cal_view, _("Deleting selected objects"));

	e_cal_view_copy_clipboard (cal_view);
	for (l = selected; l != NULL; l = l->next) {
		CalComponent *comp;

		ECalViewEvent *event = (ECalViewEvent *) l->data;

		if (!event)
			continue;

		comp = cal_component_new ();
		cal_component_set_icalcomponent (comp, icalcomponent_new_clone (event->comp_data->icalcomp));

		if (itip_organizer_is_user (comp, event->comp_data->client) 
		    && cancel_component_dialog ((GtkWindow *) gtk_widget_get_toplevel (GTK_WIDGET (cal_view)),
						event->comp_data->client, comp, TRUE))
			itip_send_comp (CAL_COMPONENT_METHOD_CANCEL, comp,
					event->comp_data->client, NULL);

		cal_component_get_uid (comp, &uid);
		delete_error_dialog (cal_client_remove_object (event->comp_data->client, uid),
				     CAL_COMPONENT_EVENT);

		g_object_unref (comp);
	}

	e_cal_view_set_status_message (cal_view, NULL);

	g_list_free (selected);
}

void
e_cal_view_copy_clipboard (ECalView *cal_view)
{
	GList *selected, *l;
	gchar *comp_str;
	icalcomponent *vcal_comp;
	icalcomponent *new_icalcomp;
	ECalViewEvent *event;

	g_return_if_fail (E_IS_CAL_VIEW (cal_view));

	selected = e_cal_view_get_selected_events (cal_view);
	if (!selected)
		return;

	/* create top-level VCALENDAR component and add VTIMEZONE's */
	vcal_comp = cal_util_new_top_level ();
	for (l = selected; l != NULL; l = l->next) {
		event = (ECalViewEvent *) l->data;

		if (event)
			cal_util_add_timezones_from_component (vcal_comp, event->comp_data->icalcomp);
	}

	for (l = selected; l != NULL; l = l->next) {
		event = (ECalViewEvent *) l->data;

		new_icalcomp = icalcomponent_new_clone (event->comp_data->icalcomp);
		icalcomponent_add_component (vcal_comp, new_icalcomp);
	}

	/* copy the VCALENDAR to the clipboard */
	comp_str = icalcomponent_as_ical_string (vcal_comp);
	if (cal_view->priv->clipboard_selection != NULL)
		g_free (cal_view->priv->clipboard_selection);
	cal_view->priv->clipboard_selection = g_strdup (comp_str);
	gtk_selection_owner_set (cal_view->priv->invisible, clipboard_atom, GDK_CURRENT_TIME);

	/* free memory */
	icalcomponent_free (vcal_comp);
	g_list_free (selected);
}

void
e_cal_view_paste_clipboard (ECalView *cal_view)
{
	g_return_if_fail (E_IS_CAL_VIEW (cal_view));

	gtk_selection_convert (cal_view->priv->invisible,
			       clipboard_atom,
			       GDK_SELECTION_TYPE_STRING,
			       GDK_CURRENT_TIME);
}

static void
delete_event (ECalView *cal_view, ECalViewEvent *event)
{
	CalComponent *comp;
	CalComponentVType vtype;

	comp = cal_component_new ();
	cal_component_set_icalcomponent (comp, icalcomponent_new_clone (event->comp_data->icalcomp));
	vtype = cal_component_get_vtype (comp);

	if (delete_component_dialog (comp, FALSE, 1, vtype, GTK_WIDGET (cal_view))) {
		const char *uid;

		if (itip_organizer_is_user (comp, event->comp_data->client) 
		    && cancel_component_dialog ((GtkWindow *) gtk_widget_get_toplevel (GTK_WIDGET (cal_view)),
						event->comp_data->client,
						comp, TRUE))
			itip_send_comp (CAL_COMPONENT_METHOD_CANCEL, comp,
					event->comp_data->client, NULL);

		cal_component_get_uid (comp, &uid);
		if (!uid || !*uid) {
			g_object_unref (comp);
			return;
		}

		delete_error_dialog (
			cal_client_remove_object (event->comp_data->client, uid), CAL_COMPONENT_EVENT);
	}

	g_object_unref (comp);
}

void
e_cal_view_delete_selected_event (ECalView *cal_view)
{
	GList *selected;
	ECalViewEvent *event;

	selected = e_cal_view_get_selected_events (cal_view);
	if (!selected)
		return;

	event = (ECalViewEvent *) selected->data;
	if (event)
		delete_event (cal_view, event);

	g_list_free (selected);
}

void
e_cal_view_delete_selected_events (ECalView *cal_view)
{
	GList *selected, *l;
	ECalViewEvent *event;

	selected = e_cal_view_get_selected_events (cal_view);
	if (!selected)
		return;

	for (l = selected; l != NULL; l = l->next) {
		event = (ECalViewEvent *) l->data;
		if (event)
			delete_event (cal_view, event);
	}

	g_list_free (selected);
}

void
e_cal_view_delete_selected_occurrence (ECalView *cal_view)
{
	ECalViewEvent *event;
	GList *selected;

	selected = e_cal_view_get_selected_events (cal_view);
	if (!selected)
		return;

	event = (ECalViewEvent *) selected->data;

	if (cal_util_component_is_instance (event->comp_data->icalcomp)) {
		const char *uid;

		uid = icalcomponent_get_uid (event->comp_data->icalcomp);
		delete_error_dialog (
			cal_client_remove_object_with_mod (event->comp_data->client, uid, CALOBJ_MOD_THIS),
			CAL_COMPONENT_EVENT);
	} else {
		CalComponent *comp;

		/* we must duplicate the CalComponent, or we won't know it has changed
		   when we get the "update_event" signal */
		comp = cal_component_new ();
		cal_component_set_icalcomponent (comp, icalcomponent_new_clone (event->comp_data->icalcomp));
		cal_comp_util_add_exdate (comp, event->start, cal_view->priv->zone);

		if (cal_client_update_object (event->comp_data->client, comp)
		    != CAL_CLIENT_RESULT_SUCCESS)
			g_message ("e_cal_view_delete_selected_occurrence(): Could not update the object!");

		g_object_unref (comp);
	}

	/* free memory */
	g_list_free (selected);
}

static void
on_new_appointment (GtkWidget *widget, gpointer user_data)
{
	time_t dtstart, dtend;
	ECalView *cal_view = (ECalView *) user_data;

	e_cal_view_get_selected_time_range (cal_view, &dtstart, &dtend);
	gnome_calendar_new_appointment_for (cal_view->priv->calendar, dtstart, dtend, FALSE, FALSE);
}

static void
on_new_event (GtkWidget *widget, gpointer user_data)
{
	time_t dtstart, dtend;
	ECalView *cal_view = (ECalView *) user_data;

	e_cal_view_get_selected_time_range (cal_view, &dtstart, &dtend);
	gnome_calendar_new_appointment_for (cal_view->priv->calendar, dtstart, dtend, TRUE, FALSE);
}

static void
on_new_meeting (GtkWidget *widget, gpointer user_data)
{
	time_t dtstart, dtend;
	ECalView *cal_view = (ECalView *) user_data;

	e_cal_view_get_selected_time_range (cal_view, &dtstart, &dtend);
	gnome_calendar_new_appointment_for (cal_view->priv->calendar, dtstart, dtend, FALSE, TRUE);
}

static void
on_new_task (GtkWidget *widget, gpointer user_data)
{
	ECalView *cal_view = (ECalView *) user_data;
	gnome_calendar_new_task (cal_view->priv->calendar);
}

static void
on_goto_date (GtkWidget *widget, gpointer user_data)
{
	ECalView *cal_view = E_CAL_VIEW (user_data);

	goto_dialog (cal_view->priv->calendar);
}

static void
on_goto_today (GtkWidget *widget, gpointer user_data)
{
	ECalView *cal_view = E_CAL_VIEW (user_data);

	calendar_goto_today (cal_view->priv->calendar);
}

static void
on_edit_appointment (GtkWidget *widget, gpointer user_data)
{
	GList *selected;
	ECalView *cal_view = E_CAL_VIEW (user_data);

	selected = e_cal_view_get_selected_events (cal_view);
	if (selected) {
		ECalViewEvent *event = (ECalViewEvent *) selected->data;

		if (event)
			gnome_calendar_edit_object (cal_view->priv->calendar, event->comp_data->client,
						    event->comp_data->icalcomp, FALSE);

		g_list_free (selected);
	}
}

static void
on_print (GtkWidget *widget, gpointer user_data)
{
	ECalView *cal_view;
	time_t start;
	GnomeCalendarViewType view_type;
	PrintView print_view;

	cal_view = E_CAL_VIEW (user_data);

	gnome_calendar_get_current_time_range (cal_view->priv->calendar, &start, NULL);
	view_type = gnome_calendar_get_view (cal_view->priv->calendar);

	switch (view_type) {
	case GNOME_CAL_DAY_VIEW :
		print_view = PRINT_VIEW_DAY;
		break;

	case GNOME_CAL_WORK_WEEK_VIEW :
	case GNOME_CAL_WEEK_VIEW:
		print_view = PRINT_VIEW_WEEK;
		break;

	case GNOME_CAL_MONTH_VIEW:
		print_view = PRINT_VIEW_MONTH;
		break;

	default:
		g_assert_not_reached ();
		return;
	}

	print_calendar (cal_view->priv->calendar, FALSE, start, print_view);
}

static void
on_save_as (GtkWidget *widget, gpointer user_data)
{
	ECalView *cal_view;
	GList *selected;
	char *filename;
	char *ical_string;
	FILE *file;
	ECalViewEvent *event;

	cal_view = E_CAL_VIEW (user_data);

	selected = e_cal_view_get_selected_events (cal_view);
	if (!selected)
		return;

	filename = e_file_dialog_save (_("Save as..."));
	if (filename == NULL)
		return;
	
	event = (ECalViewEvent *) selected->data;
	ical_string = cal_client_get_component_as_string (event->comp_data->client, event->comp_data->icalcomp);
	if (ical_string == NULL) {
		g_warning ("Couldn't convert item to a string");
		return;
	}
	
	file = fopen (filename, "w");
	if (file == NULL) {
		g_warning ("Couldn't save item");
		return;
	}
	
	fprintf (file, ical_string);
	g_free (ical_string);
	fclose (file);

	g_list_free (selected);
}

static void
on_print_event (GtkWidget *widget, gpointer user_data)
{
	ECalView *cal_view;
	GList *selected;
	ECalViewEvent *event;
	CalComponent *comp;

	cal_view = E_CAL_VIEW (user_data);
	selected = e_cal_view_get_selected_events (cal_view);
	if (!selected)
		return;

	event = (ECalViewEvent *) selected->data;

	comp = cal_component_new ();
	cal_component_set_icalcomponent (comp, icalcomponent_new_clone (event->comp_data->icalcomp));
	print_comp (comp, event->comp_data->client, FALSE);

	g_object_unref (comp);
}

static void
on_meeting (GtkWidget *widget, gpointer user_data)
{
	GList *selected;
	ECalView *cal_view = E_CAL_VIEW (user_data);

	selected = e_cal_view_get_selected_events (cal_view);
	if (selected) {
		ECalViewEvent *event = (ECalViewEvent *) selected->data;
		gnome_calendar_edit_object (cal_view->priv->calendar, event->comp_data->client, event->comp_data->icalcomp, TRUE);

		g_list_free (selected);
	}
}

static void
on_forward (GtkWidget *widget, gpointer user_data)
{
	GList *selected;
	ECalView *cal_view = E_CAL_VIEW (user_data);

	selected = e_cal_view_get_selected_events (cal_view);
	if (selected) {
		CalComponent *comp;
		ECalViewEvent *event = (ECalViewEvent *) selected->data;

		comp = cal_component_new ();
		cal_component_set_icalcomponent (comp, icalcomponent_new_clone (event->comp_data->icalcomp));
		itip_send_comp (CAL_COMPONENT_METHOD_PUBLISH, comp, event->comp_data->client, NULL);

		g_list_free (selected);
		g_object_unref (comp);
	}
}

static void
on_publish (GtkWidget *widget, gpointer user_data)
{
	ECalView *cal_view;
	icaltimezone *utc;
	time_t start = time (NULL), end;
	GList *comp_list, *client_list, *cl;

	cal_view = E_CAL_VIEW (user_data);

	utc = icaltimezone_get_utc_timezone ();
	start = time_day_begin_with_zone (start, utc);
	end = time_add_week_with_zone (start, 6, utc);

	client_list = e_cal_model_get_client_list (cal_view->priv->model);
	for (cl = client_list; cl != NULL; cl = cl->next) {
		comp_list = cal_client_get_free_busy ((CalClient *) cl->data, NULL, start, end);
		if (comp_list) {
			GList *l;

			for (l = comp_list; l; l = l->next) {
				CalComponent *comp = CAL_COMPONENT (l->data);
				itip_send_comp (CAL_COMPONENT_METHOD_PUBLISH, comp, (CalClient *) cl->data, NULL);

				g_object_unref (comp);
			}

			g_list_free (comp_list);
		}
	}

	g_list_free (client_list);
}

static void
on_settings (GtkWidget *widget, gpointer user_data)
{
	ECalView *cal_view;

	cal_view = E_CAL_VIEW (user_data);
	control_util_show_settings (cal_view->priv->calendar);
}

static void
on_delete_appointment (GtkWidget *widget, gpointer user_data)
{
	ECalView *cal_view;

	cal_view = E_CAL_VIEW (user_data);
	e_cal_view_delete_selected_event (cal_view);
}

static void
on_delete_occurrence (GtkWidget *widget, gpointer user_data)
{
	ECalView *cal_view;

	cal_view = E_CAL_VIEW (user_data);
	e_cal_view_delete_selected_occurrence (cal_view);
}

static void
on_cut (GtkWidget *widget, gpointer user_data)
{
	ECalView *cal_view = E_CAL_VIEW (user_data);

	e_cal_view_cut_clipboard (cal_view);
}

static void
on_copy (GtkWidget *widget, gpointer user_data)
{
	ECalView *cal_view = E_CAL_VIEW (user_data);

	e_cal_view_copy_clipboard (cal_view);
}

static void
on_paste (GtkWidget *widget, gpointer user_data)
{
	ECalView *cal_view = E_CAL_VIEW (user_data);

	e_cal_view_paste_clipboard (cal_view);
}

static void
on_unrecur_appointment (GtkWidget *widget, gpointer user_data)
{
	ECalView *cal_view = E_CAL_VIEW (user_data);

	gnome_calendar_unrecur_selection (cal_view->priv->calendar);
}

enum {
	/*
	 * This is used to "flag" events that can not be editted
	 */
	MASK_EDITABLE = 1,

	/*
	 * To disable recurring actions to be displayed
	 */
	MASK_RECURRING = 2,

	/*
	 * To disable actions for non-recurring items to be displayed
	 */
	MASK_SINGLE   = 4,

	/*
	 * This is used to when an event is currently being edited
	 * in another window and we want to disable the event
	 * from being edited twice
	 */
	MASK_EDITING  = 8,

	/*
	 * This is used to when an event is already a meeting and
	 * we want to disable the schedule meeting command
	 */
	MASK_MEETING  = 16,

	/*
	 * To disable cut and copy for meetings the user is not the
	 * organizer of
	 */
	MASK_MEETING_ORGANIZER = 32,

	/*
	 * To disable things not valid for instances
	 */
	MASK_INSTANCE = 64
};

static EPopupMenu main_items [] = {
	E_POPUP_ITEM (N_("New _Appointment..."), GTK_SIGNAL_FUNC (on_new_appointment), MASK_EDITABLE),
	E_POPUP_ITEM (N_("New All Day _Event"), GTK_SIGNAL_FUNC (on_new_event), MASK_EDITABLE),
	E_POPUP_ITEM (N_("New Meeting"), GTK_SIGNAL_FUNC (on_new_meeting), MASK_EDITABLE),
	E_POPUP_ITEM (N_("New Task"), GTK_SIGNAL_FUNC (on_new_task), MASK_EDITABLE),

	E_POPUP_SEPARATOR,

	E_POPUP_ITEM (N_("_Print..."), GTK_SIGNAL_FUNC (on_print), 0),

	E_POPUP_SEPARATOR,

	E_POPUP_ITEM (N_("_Paste"), GTK_SIGNAL_FUNC (on_paste), MASK_EDITABLE),

	E_POPUP_SEPARATOR,

	E_POPUP_SUBMENU (N_("Current View"), NULL, 0),
	
	E_POPUP_ITEM (N_("Go to _Today"), GTK_SIGNAL_FUNC (on_goto_today), 0),
	E_POPUP_ITEM (N_("_Go to Date..."), GTK_SIGNAL_FUNC (on_goto_date), 0),

	E_POPUP_SEPARATOR,

	E_POPUP_ITEM (N_("_Publish Free/Busy Information"), GTK_SIGNAL_FUNC (on_publish), 0),

	E_POPUP_SEPARATOR,

	E_POPUP_ITEM (N_("_Settings..."), GTK_SIGNAL_FUNC (on_settings), 0),

	E_POPUP_TERMINATOR
};

static EPopupMenu child_items [] = {
	E_POPUP_ITEM (N_("_Open"), GTK_SIGNAL_FUNC (on_edit_appointment), MASK_EDITING),
	E_POPUP_ITEM (N_("_Save As..."), GTK_SIGNAL_FUNC (on_save_as), MASK_EDITING),
	E_POPUP_ITEM (N_("_Print..."), GTK_SIGNAL_FUNC (on_print_event), MASK_EDITING),

	/* Only show this separator if one of the above is shown. */
	E_POPUP_SEPARATOR,

	E_POPUP_ITEM (N_("C_ut"), GTK_SIGNAL_FUNC (on_cut), MASK_EDITING | MASK_EDITABLE | MASK_MEETING_ORGANIZER),
	E_POPUP_ITEM (N_("_Copy"), GTK_SIGNAL_FUNC (on_copy), MASK_EDITING | MASK_MEETING_ORGANIZER),
	E_POPUP_ITEM (N_("_Paste"), GTK_SIGNAL_FUNC (on_paste), MASK_EDITABLE),

	E_POPUP_SEPARATOR,

	E_POPUP_ITEM (N_("_Schedule Meeting..."), GTK_SIGNAL_FUNC (on_meeting), MASK_EDITABLE | MASK_EDITING | MASK_MEETING),
	E_POPUP_ITEM (N_("_Forward as iCalendar..."), GTK_SIGNAL_FUNC (on_forward), MASK_EDITING),

	E_POPUP_SEPARATOR,

	E_POPUP_ITEM (N_("_Delete"), GTK_SIGNAL_FUNC (on_delete_appointment), MASK_EDITABLE | MASK_SINGLE | MASK_EDITING),
	E_POPUP_ITEM (N_("Make this Occurrence _Movable"), GTK_SIGNAL_FUNC (on_unrecur_appointment), MASK_RECURRING | MASK_EDITING | MASK_EDITABLE | MASK_INSTANCE),
	E_POPUP_ITEM (N_("Delete this _Occurrence"), GTK_SIGNAL_FUNC (on_delete_occurrence), MASK_RECURRING | MASK_EDITING | MASK_EDITABLE),
	E_POPUP_ITEM (N_("Delete _All Occurrences"), GTK_SIGNAL_FUNC (on_delete_appointment), MASK_RECURRING | MASK_EDITING | MASK_EDITABLE),

	E_POPUP_TERMINATOR
};

static void
free_view_popup (GtkWidget *widget, gpointer data)
{
	ECalView *cal_view = E_CAL_VIEW (data);

	if (cal_view->priv->view_menu == NULL)
		return;
	
	gnome_calendar_discard_view_popup (cal_view->priv->calendar, cal_view->priv->view_menu);
	cal_view->priv->view_menu = NULL;
}

static void
setup_popup_icons (EPopupMenu *context_menu)
{
	gint i;

	for (i = 0; context_menu[i].name; i++) {
		GtkWidget *pixmap_widget = NULL;

		if (!strcmp (context_menu[i].name, _("_Copy")))
			pixmap_widget = gtk_image_new_from_stock (GTK_STOCK_COPY, GTK_ICON_SIZE_MENU);
		else if (!strcmp (context_menu[i].name, _("C_ut")))
			pixmap_widget = gtk_image_new_from_stock (GTK_STOCK_CUT, GTK_ICON_SIZE_MENU);
		else if (!strcmp (context_menu[i].name, _("_Delete")) ||
			 !strcmp (context_menu[i].name, _("Delete this _Occurrence")) ||
			 !strcmp (context_menu[i].name, _("Delete _All Occurrences")))
			pixmap_widget = gtk_image_new_from_stock (GTK_STOCK_DELETE, GTK_ICON_SIZE_MENU);
		else if (!strcmp (context_menu[i].name, _("Go to _Today")))
			pixmap_widget = gtk_image_new_from_stock (GTK_STOCK_HOME, GTK_ICON_SIZE_MENU);
		else if (!strcmp (context_menu[i].name, _("_Go to Date...")))
			pixmap_widget = gtk_image_new_from_stock (GTK_STOCK_JUMP_TO, GTK_ICON_SIZE_MENU);
		else if (!strcmp (context_menu[i].name, _("New _Appointment...")))
			pixmap_widget = gtk_image_new_from_file (EVOLUTION_IMAGESDIR "/new_appointment.xpm");
		else if (!strcmp (context_menu[i].name, _("New All Day _Event")))
			pixmap_widget = gtk_image_new_from_file (EVOLUTION_IMAGESDIR "/new_all_day_event.png");
		else if (!strcmp (context_menu[i].name, _("New Meeting")))
			pixmap_widget = gtk_image_new_from_file (EVOLUTION_IMAGESDIR "/meeting-request-16.png");
		else if (!strcmp (context_menu[i].name, _("New Task")))
			pixmap_widget = gtk_image_new_from_file (EVOLUTION_IMAGESDIR "/new_task-16.png");
		else if (!strcmp (context_menu[i].name, _("_Open")))
			pixmap_widget = gtk_image_new_from_stock (GTK_STOCK_OPEN, GTK_ICON_SIZE_MENU);
		else if (!strcmp (context_menu[i].name, _("_Paste")))
			pixmap_widget = gtk_image_new_from_stock (GTK_STOCK_PASTE, GTK_ICON_SIZE_MENU);
		else if (!strcmp (context_menu[i].name, _("_Print...")))
			pixmap_widget = gtk_image_new_from_stock (GTK_STOCK_PRINT, GTK_ICON_SIZE_MENU);
		else if (!strcmp (context_menu[i].name, _("_Save As...")))
			pixmap_widget = gtk_image_new_from_stock (GTK_STOCK_SAVE_AS, GTK_ICON_SIZE_MENU);
		else if (!strcmp (context_menu[i].name, _("_Settings...")))
			pixmap_widget = gtk_image_new_from_stock (GTK_STOCK_PREFERENCES, GTK_ICON_SIZE_MENU);

		if (pixmap_widget)
			gtk_widget_show (pixmap_widget);
		context_menu[i].pixmap_widget = pixmap_widget;
	}
}

GtkMenu *
e_cal_view_create_popup_menu (ECalView *cal_view)
{
	gboolean being_edited, have_selection;
	GList *selected;
	EPopupMenu *context_menu;
	guint32 disable_mask = 0, hide_mask = 0;
	GtkMenu *popup;
	CalClient *client = NULL;

	g_return_val_if_fail (E_IS_CAL_VIEW (cal_view), NULL);

	/* get the selection */
	being_edited = FALSE;
	selected = e_cal_view_get_selected_events (cal_view);

	have_selection = GTK_WIDGET_HAS_FOCUS (cal_view) && selected != NULL;

	if (selected == NULL) {
		cal_view->priv->view_menu = gnome_calendar_setup_view_popup (cal_view->priv->calendar);
		main_items[9].submenu = cal_view->priv->view_menu;
		context_menu = main_items;

		client = e_cal_model_get_default_client (cal_view->priv->model);
	} else {
		ECalViewEvent *event;

		context_menu = child_items;

		event = (ECalViewEvent *) selected->data;
		if (cal_util_component_has_recurrences (event->comp_data->icalcomp))
			hide_mask |= MASK_SINGLE;
		else
			hide_mask |= MASK_RECURRING;

		if (cal_util_component_is_instance (event->comp_data->icalcomp))
			hide_mask |= MASK_INSTANCE;

		if (cal_util_component_has_organizer (event->comp_data->icalcomp)) {
			CalComponent *comp;

			disable_mask |= MASK_MEETING;

			comp = cal_component_new ();
			cal_component_set_icalcomponent (comp, icalcomponent_new_clone (event->comp_data->icalcomp));
			if (!itip_organizer_is_user (comp, event->comp_data->client))
				disable_mask |= MASK_MEETING_ORGANIZER;

			g_object_unref (comp);
		}

		client = event->comp_data->client;
	}

	if (cal_client_is_read_only (client))
		disable_mask |= MASK_EDITABLE;

	if (being_edited)
		disable_mask |= MASK_EDITING;

	setup_popup_icons (context_menu);
	popup = e_popup_menu_create (context_menu, disable_mask, hide_mask, cal_view);
	g_signal_connect (popup, "selection-done", G_CALLBACK (free_view_popup), cal_view);

	return popup;
}
