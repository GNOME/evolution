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
#include <gtk/gtkinvisible.h>
#include <libgnome/gnome-i18n.h>
#include <gal/util/e-util.h>
#include "evolution-activity-client.h"
#include "calendar-config.h"
#include "e-cal-view.h"
#include "itip-utils.h"
#include "dialogs/cancel-comp.h"
#include "dialogs/delete-error.h"
#include "dialogs/send-comp.h"

/* Used for the status bar messages */
#define EVOLUTION_CALENDAR_PROGRESS_IMAGE "evolution-calendar-mini.png"
static GdkPixbuf *progress_icon[2] = { NULL, NULL };

struct _ECalViewPrivate {
	/* The GnomeCalendar we are associated to */
	GnomeCalendar *calendar;

	/* Calendar client we are monitoring */
	CalClient *client;

	/* Search expression */
	gchar *sexp;

	/* The activity client used to show messages on the status bar. */
	EvolutionActivityClient *activity;

	/* the invisible widget to manage the clipboard selections */
	GtkWidget *invisible;
	gchar *clipboard_selection;
};

static void e_cal_view_class_init (ECalViewClass *klass);
static void e_cal_view_init (ECalView *cal_view, ECalViewClass *klass);
static void e_cal_view_destroy (GtkObject *object);

static GObjectClass *parent_class = NULL;
static GdkAtom clipboard_atom = GDK_NONE;

/* Signal IDs */
enum {
	SELECTION_CHANGED,
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

	/* Method override */
	object_class->destroy = e_cal_view_destroy;

	klass->selection_changed = NULL;
	klass->get_selected_events = NULL;
	klass->get_selected_time_range = NULL;
	klass->set_selected_time_range = NULL;
	klass->get_visible_time_range = NULL;
	klass->update_query = NULL;

	/* clipboard atom */
	if (!clipboard_atom)
		clipboard_atom = gdk_atom_intern ("CLIPBOARD", FALSE);
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
	cal_client_get_timezone (cal_view->priv->client, default_tzid, &default_zone);

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

				cal_client_update_object (cal_view->priv->client, comp);
				if (itip_organizer_is_user (comp, cal_view->priv->client) &&
				    send_component_dialog (gtk_widget_get_toplevel (cal_view),
							   cal_view->priv->client, comp, TRUE)) {
					itip_send_comp (CAL_COMPONENT_METHOD_REQUEST, comp,
							cal_view->priv->client, NULL);
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

		cal_client_update_object (cal_view->priv->client, comp);
		if (itip_organizer_is_user (comp, cal_view->priv->client) &&
		    send_component_dialog (gtk_widget_get_toplevel (cal_view),
					   cal_view->priv->client, comp, TRUE)) {
			itip_send_comp (CAL_COMPONENT_METHOD_REQUEST, comp,
					cal_view->priv->client, NULL);
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

	cal_view->priv->sexp = g_strdup ("#t"); /* match all by default */

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
		if (cal_view->priv->client) {
			g_object_unref (cal_view->priv->client);
			cal_view->priv->client = NULL;
		}

		if (cal_view->priv->sexp) {
			g_free (cal_view->priv->sexp);
			cal_view->priv->sexp = NULL;
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

CalClient *
e_cal_view_get_cal_client (ECalView *cal_view)
{
	g_return_val_if_fail (E_IS_CAL_VIEW (cal_view), NULL);

	return cal_view->priv->client;
}

static void
cal_opened_cb (CalClient *client, CalClientOpenStatus status, gpointer user_data)
{
	ECalView *cal_view = (ECalView *) user_data;

	if (status != CAL_CLIENT_OPEN_SUCCESS)
		return;

	e_cal_view_update_query (cal_view);
}

void
e_cal_view_set_cal_client (ECalView *cal_view, CalClient *client)
{
	g_return_if_fail (E_IS_CAL_VIEW (cal_view));

	if (client == cal_view->priv->client)
		return;

	if (IS_CAL_CLIENT (client))
		g_object_ref (client);

	if (cal_view->priv->client) {
		g_signal_handlers_disconnect_matched (cal_view->priv->client, G_SIGNAL_MATCH_DATA, 0, 0,
						      NULL, NULL, cal_view);
		g_object_unref (cal_view->priv->client);
	}

	cal_view->priv->client = client;
	if (cal_view->priv->client) {
		if (cal_client_get_load_state (cal_view->priv->client) == CAL_CLIENT_LOAD_LOADED)
			e_cal_view_update_query (cal_view);
		else
			g_signal_connect (cal_view->priv->client, "cal_opened",
					  G_CALLBACK (cal_opened_cb), cal_view);
	}
}

const gchar *
e_cal_view_get_query (ECalView *cal_view)
{
	g_return_val_if_fail (E_IS_CAL_VIEW (cal_view), NULL);

	return (const gchar *) cal_view->priv->sexp;
}

void
e_cal_view_set_query (ECalView *cal_view, const gchar *sexp)
{
	g_return_if_fail (E_IS_CAL_VIEW (cal_view));

	if (cal_view->priv->sexp)
		g_free (cal_view->priv->sexp);

	cal_view->priv->sexp = g_strdup (sexp);
	e_cal_view_update_query (cal_view);
}

void
e_cal_view_set_status_message (ECalView *cal_view, const gchar *message)
{
	extern EvolutionShellClient *global_shell_client; /* ugly */

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
		cal_view->priv->activity = evolution_activity_client_new (
			global_shell_client, client_id,
			progress_icon, message, TRUE, &display);

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
	g_return_if_fail (E_IS_CAL_VIEW (cal_view));

	if (E_CAL_VIEW_CLASS (G_OBJECT_GET_CLASS (cal_view))->get_visible_time_range) {
		E_CAL_VIEW_CLASS (G_OBJECT_GET_CLASS (cal_view))->get_visible_time_range (
			cal_view, start_time, end_time);
	}
}

void
e_cal_view_update_query (ECalView *cal_view)
{
	g_return_if_fail (E_IS_CAL_VIEW (cal_view));

	if (E_CAL_VIEW_CLASS (G_OBJECT_GET_CLASS (cal_view))->update_query) {
		E_CAL_VIEW_CLASS (G_OBJECT_GET_CLASS (cal_view))->update_query (cal_view);
	}
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
		CalComponent *comp = l->data;

		if (itip_organizer_is_user (comp, cal_view->priv->client) 
		    && cancel_component_dialog ((GtkWindow *) gtk_widget_get_toplevel (cal_view),
						cal_view->priv->client, comp, TRUE))
			itip_send_comp (CAL_COMPONENT_METHOD_CANCEL, comp, cal_view->priv->client, NULL);

		cal_component_get_uid (comp, &uid);
		delete_error_dialog (cal_client_remove_object (cal_view->priv->client, uid),
				     CAL_COMPONENT_EVENT);
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

	g_return_if_fail (E_IS_CAL_VIEW (cal_view));

	selected = e_cal_view_get_selected_events (cal_view);
	if (!selected)
		return;

	/* create top-level VCALENDAR component and add VTIMEZONE's */
	vcal_comp = cal_util_new_top_level ();
	for (l = selected; l != NULL; l = l->next)
		cal_util_add_timezones_from_component (vcal_comp, (CalComponent *) l->data);

	for (l = selected; l != NULL; l = l->next) {
		CalComponent *comp = (CalComponent *) l->data;

		new_icalcomp = icalcomponent_new_clone (cal_component_get_icalcomponent (comp));
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
