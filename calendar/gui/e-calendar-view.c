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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <time.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkstock.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkbindings.h>
#include <libgnome/gnome-i18n.h>
#include <e-util/e-dialog-utils.h>
#include <e-util/e-icon-factory.h>
#include "e-calendar-marshal.h"
#include <libecal/e-cal-time-util.h>
#include <libecal/e-cal-component.h>

#include "common/authentication.h"
#include "calendar-commands.h"
#include "calendar-component.h"
#include "calendar-config.h"
#include "comp-util.h"
#include "e-cal-model-calendar.h"
#include "e-calendar-view.h"
#include "e-comp-editor-registry.h"
#include "itip-utils.h"
#include "e-pub-utils.h"
#include "dialogs/delete-comp.h"
#include "dialogs/delete-error.h"
#include "dialogs/event-editor.h"
#include "dialogs/send-comp.h"
#include "dialogs/cancel-comp.h"
#include "dialogs/recur-comp.h"
#include "dialogs/select-source-dialog.h"
#include "print.h"
#include "goto.h"
#include "ea-calendar.h"
#include "e-cal-popup.h"

/* Used for the status bar messages */
#define EVOLUTION_CALENDAR_PROGRESS_IMAGE "stock_calendar"
static GdkPixbuf *progress_icon = NULL;

struct _ECalendarViewPrivate {
	/* The GnomeCalendar we are associated to */
	GnomeCalendar *calendar;

	/* The calendar model we are monitoring */
	ECalModel *model;

	/* Current activity (for the EActivityHandler, i.e. the status bar).  */
	EActivityHandler *activity_handler;
	guint activity_id;

	/* The default category */
	char *default_category;
};

static void e_calendar_view_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void e_calendar_view_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void e_calendar_view_destroy (GtkObject *object);

static GdkAtom clipboard_atom = GDK_NONE;
extern ECompEditorRegistry *comp_editor_registry;

/* Property IDs */
enum props {
	PROP_0,
	PROP_MODEL,
};

/* FIXME Why are we emitting these event signals here? Can't the model just be listened to? */
/* Signal IDs */
enum {
	SELECTION_CHANGED,
	SELECTED_TIME_CHANGED,
	TIMEZONE_CHANGED,
	EVENT_CHANGED,
	EVENT_ADDED,
	USER_CREATED,
	OPEN_EVENT,
	LAST_SIGNAL
};

static guint e_calendar_view_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (ECalendarView, e_calendar_view, GTK_TYPE_TABLE);

static void
e_calendar_view_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	ECalendarView *cal_view;
	ECalendarViewPrivate *priv;

	cal_view = E_CALENDAR_VIEW (object);
	priv = cal_view->priv;
	
	switch (property_id) {
	case PROP_MODEL:
		e_calendar_view_set_model (cal_view, E_CAL_MODEL (g_value_get_object (value)));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
e_calendar_view_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	ECalendarView *cal_view;
	ECalendarViewPrivate *priv;

	cal_view = E_CALENDAR_VIEW (object);
	priv = cal_view->priv;

	switch (property_id) {
	case PROP_MODEL:
		g_value_set_object (value, e_calendar_view_get_model (cal_view));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
e_calendar_view_class_init (ECalendarViewClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);

	GtkBindingSet *binding_set;

	/* Method override */
	gobject_class->set_property = e_calendar_view_set_property;
	gobject_class->get_property = e_calendar_view_get_property;
	object_class->destroy = e_calendar_view_destroy;

	klass->selection_changed = NULL;
 	klass->selected_time_changed = NULL;
	klass->event_changed = NULL;
	klass->event_added = NULL;
	klass->user_created = NULL;

	klass->get_selected_events = NULL;
	klass->get_selected_time_range = NULL;
	klass->set_selected_time_range = NULL;
	klass->get_visible_time_range = NULL;
	klass->update_query = NULL;
	klass->open_event = e_calendar_view_open_event;

	g_object_class_install_property (gobject_class, PROP_MODEL, 
					 g_param_spec_object ("model", NULL, NULL, E_TYPE_CAL_MODEL,
							      G_PARAM_READABLE | G_PARAM_WRITABLE));

	/* Create class' signals */
	e_calendar_view_signals[SELECTION_CHANGED] =
		g_signal_new ("selection_changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ECalendarViewClass, selection_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	e_calendar_view_signals[SELECTED_TIME_CHANGED] =
		g_signal_new ("selected_time_changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ECalendarViewClass, selected_time_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	e_calendar_view_signals[TIMEZONE_CHANGED] =
		g_signal_new ("timezone_changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ECalendarViewClass, timezone_changed),
			      NULL, NULL,
			      e_calendar_marshal_VOID__POINTER_POINTER,
			      G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_POINTER);

	e_calendar_view_signals[EVENT_CHANGED] =
		g_signal_new ("event_changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (ECalendarViewClass, event_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1,
			      G_TYPE_POINTER);

	e_calendar_view_signals[EVENT_ADDED] =
		g_signal_new ("event_added",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (ECalendarViewClass, event_added),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1,
			      G_TYPE_POINTER);

	e_calendar_view_signals[USER_CREATED] =
		g_signal_new ("user_created",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ECalendarViewClass, user_created),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	e_calendar_view_signals[OPEN_EVENT] =
		g_signal_new ("open_event",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (ECalendarViewClass, open_event),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	/* clipboard atom */
	if (!clipboard_atom)
		clipboard_atom = gdk_atom_intern ("CLIPBOARD", FALSE);


        /*
         * Key bindings
         */

	binding_set = gtk_binding_set_by_class (klass);

	gtk_binding_entry_add_signal (binding_set, GDK_o,
                                      GDK_CONTROL_MASK,
                                      "open_event", 0);
       
	/* init the accessibility support for e_day_view */
 	e_cal_view_a11y_init ();
}


void
e_calendar_view_add_event (ECalendarView *cal_view, ECal *client, time_t dtstart, 
		      icaltimezone *default_zone, icalcomponent *icalcomp, gboolean in_top_canvas) 
{
	ECalComponent *comp;
	struct icaltimetype itime, old_dtstart, old_dtend;
	time_t tt_start, tt_end, new_dtstart;
	struct icaldurationtype ic_dur, ic_oneday;
	char *uid;
	gint start_offset, end_offset;
	gboolean all_day_event = FALSE;
	GnomeCalendarViewType view_type;
	ECalComponentDateTime dt;

	start_offset = 0;
	end_offset = 0;

	old_dtstart = icalcomponent_get_dtstart (icalcomp);
	tt_start = icaltime_as_timet (old_dtstart);
	old_dtend = icalcomponent_get_dtend (icalcomp);
	tt_end = icaltime_as_timet (old_dtend);
	ic_dur = icaldurationtype_from_int (tt_end - tt_start);

	if (icaldurationtype_as_int (ic_dur) > 60*60*24) {
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
		} else if (icaldurationtype_as_int (ic_dur) >= 60*60*24
				&& !in_top_canvas) {
			/* copy & paste from top canvas to main canvas */
			int time_divisions;

			time_divisions = calendar_config_get_time_divisions ();
			ic_dur = icaldurationtype_from_int (time_divisions * 60);
		}
		break;
	case GNOME_CAL_WEEK_VIEW:
	case GNOME_CAL_MONTH_VIEW:
	case GNOME_CAL_LIST_VIEW:
		if (old_dtstart.is_date && old_dtend.is_date
			&& memcmp (&ic_dur, &ic_oneday, sizeof(ic_dur)) == 0)
			all_day_event = TRUE;
		break;
	default:
		g_assert_not_reached ();
		return;
	}
	
	if (in_top_canvas)
		new_dtstart = dtstart + start_offset * 60;
	else
		new_dtstart = dtstart;

	itime = icaltime_from_timet_with_zone (new_dtstart, FALSE, default_zone);
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

	/* set the timezone properly */
	dt.value = &itime;
	e_cal_component_get_dtstart (comp, &dt);
	dt.tzid = icaltimezone_get_tzid (default_zone);
	e_cal_component_set_dtstart (comp, &dt);
	e_cal_component_get_dtend (comp, &dt);
	dt.tzid = icaltimezone_get_tzid (default_zone);
	e_cal_component_set_dtend (comp, &dt);
	e_cal_component_commit_sequence (comp);

	/* FIXME Error handling */
	uid = NULL;
	if (e_cal_create_object (client, e_cal_component_get_icalcomponent (comp), &uid, NULL)) {
		if (uid) {
			e_cal_component_set_uid (comp, uid);
			g_free (uid);
		}

		if (itip_organizer_is_user (comp, client) &&
		    send_component_dialog ((GtkWindow *) gtk_widget_get_toplevel (GTK_WIDGET (cal_view)),
					   client, comp, TRUE)) {
			itip_send_comp (E_CAL_COMPONENT_METHOD_REQUEST, comp,
				client, NULL);
		}
	} else {
		g_message (G_STRLOC ": Could not create the object!");
	}

	g_object_unref (comp);
}

static void
e_calendar_view_init (ECalendarView *cal_view)
{
	cal_view->priv = g_new0 (ECalendarViewPrivate, 1);

	cal_view->priv->model = (ECalModel *) e_cal_model_calendar_new ();
}

static void
e_calendar_view_destroy (GtkObject *object)
{
	ECalendarView *cal_view = (ECalendarView *) object;

	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	if (cal_view->priv) {
		if (cal_view->priv->model) {
			g_signal_handlers_disconnect_matched (cal_view->priv->model,
							      G_SIGNAL_MATCH_DATA,
							      0, 0, NULL, NULL, cal_view);
			g_object_unref (cal_view->priv->model);
			cal_view->priv->model = NULL;
		}

		if (cal_view->priv->default_category) {
			g_free (cal_view->priv->default_category);
			cal_view->priv->default_category = NULL;
		}

		g_free (cal_view->priv);
		cal_view->priv = NULL;
	}

	if (GTK_OBJECT_CLASS (e_calendar_view_parent_class)->destroy)
		GTK_OBJECT_CLASS (e_calendar_view_parent_class)->destroy (object);
}

GnomeCalendar *
e_calendar_view_get_calendar (ECalendarView *cal_view)
{
	g_return_val_if_fail (E_IS_CALENDAR_VIEW (cal_view), NULL);

	return cal_view->priv->calendar;
}

void
e_calendar_view_set_calendar (ECalendarView *cal_view, GnomeCalendar *calendar)
{
	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	cal_view->priv->calendar = calendar;
}

ECalModel *
e_calendar_view_get_model (ECalendarView *cal_view)
{
	g_return_val_if_fail (E_IS_CALENDAR_VIEW (cal_view), NULL);

	return cal_view->priv->model;
}

void
e_calendar_view_set_model (ECalendarView *cal_view, ECalModel *model)
{
	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));
	g_return_if_fail (E_IS_CAL_MODEL (model));

	if (cal_view->priv->model) {
		g_signal_handlers_disconnect_matched (cal_view->priv->model, G_SIGNAL_MATCH_DATA,
						      0, 0, 0, NULL, cal_view);
		g_object_unref (cal_view->priv->model);
	}

	cal_view->priv->model = g_object_ref (model);
	e_calendar_view_update_query (cal_view);
}

icaltimezone *
e_calendar_view_get_timezone (ECalendarView *cal_view)
{
	g_return_val_if_fail (E_IS_CALENDAR_VIEW (cal_view), NULL);
	return e_cal_model_get_timezone (cal_view->priv->model);
}

void
e_calendar_view_set_timezone (ECalendarView *cal_view, icaltimezone *zone)
{
	icaltimezone *old_zone;

	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	old_zone = e_cal_model_get_timezone (cal_view->priv->model);
	if (old_zone == zone)
		return;

	e_cal_model_set_timezone (cal_view->priv->model, zone);
	g_signal_emit (G_OBJECT (cal_view), e_calendar_view_signals[TIMEZONE_CHANGED], 0,
		       old_zone, zone);
}

const char *
e_calendar_view_get_default_category (ECalendarView *cal_view)
{
	g_return_val_if_fail (E_IS_CALENDAR_VIEW (cal_view), NULL);
	return (const char *) cal_view->priv->default_category;
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
e_calendar_view_set_default_category (ECalendarView *cal_view, const char *category)
{
	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	if (cal_view->priv->default_category)
		g_free (cal_view->priv->default_category);

	cal_view->priv->default_category = g_strdup (category);
}

/**
 * e_calendar_view_get_use_24_hour_format:
 * @cal_view: A calendar view.
 *
 * Gets whether the view is using 24 hour times or not.
 *
 * Returns: the 24 hour setting.
 */
gboolean
e_calendar_view_get_use_24_hour_format (ECalendarView *cal_view)
{
	g_return_val_if_fail (E_IS_CALENDAR_VIEW (cal_view), FALSE);

	return e_cal_model_get_use_24_hour_format (cal_view->priv->model);
}

/**
 * e_calendar_view_set_use_24_hour_format
 * @cal_view: A calendar view.
 * @use_24_hour: Whether to use 24 hour times or not.
 *
 * Sets the 12/24 hour times setting for the given view.
 */
void
e_calendar_view_set_use_24_hour_format (ECalendarView *cal_view, gboolean use_24_hour)
{
	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	e_cal_model_set_use_24_hour_format (cal_view->priv->model, use_24_hour);
}

void
e_calendar_view_set_activity_handler (ECalendarView *cal_view, EActivityHandler *activity_handler)
{
	ECalendarViewPrivate *priv;
	
	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	priv = cal_view->priv;

	priv->activity_handler = activity_handler;
}

void
e_calendar_view_set_status_message (ECalendarView *cal_view, const gchar *message)
{
	ECalendarViewPrivate *priv;
	
	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	priv = cal_view->priv;

	if (!priv->activity_handler)
		return;
	
	if (!message || !*message) {
		if (priv->activity_id != 0) {
			e_activity_handler_operation_finished (priv->activity_handler, priv->activity_id);
			priv->activity_id = 0;
		}
	} else if (priv->activity_id == 0) {
		char *client_id = g_strdup_printf ("%p", cal_view);

		if (progress_icon == NULL)
			progress_icon = e_icon_factory_get_icon (EVOLUTION_CALENDAR_PROGRESS_IMAGE, E_ICON_SIZE_STATUS);

		priv->activity_id = e_activity_handler_operation_started (priv->activity_handler, client_id, progress_icon, message, TRUE);

		g_free (client_id);
	} else {
		e_activity_handler_operation_progressing (priv->activity_handler, priv->activity_id, message, -1.0);
	}
}

GList *
e_calendar_view_get_selected_events (ECalendarView *cal_view)
{
	g_return_val_if_fail (E_IS_CALENDAR_VIEW (cal_view), NULL);

	if (E_CALENDAR_VIEW_CLASS (G_OBJECT_GET_CLASS (cal_view))->get_selected_events)
		return E_CALENDAR_VIEW_CLASS (G_OBJECT_GET_CLASS (cal_view))->get_selected_events (cal_view);

	return NULL;
}

gboolean
e_calendar_view_get_selected_time_range (ECalendarView *cal_view, time_t *start_time, time_t *end_time)
{
	g_return_val_if_fail (E_IS_CALENDAR_VIEW (cal_view), FALSE);

	if (E_CALENDAR_VIEW_CLASS (G_OBJECT_GET_CLASS (cal_view))->get_selected_time_range) {
		return E_CALENDAR_VIEW_CLASS (G_OBJECT_GET_CLASS (cal_view))->get_selected_time_range (
			cal_view, start_time, end_time);
	}

	return FALSE;
}

void
e_calendar_view_set_selected_time_range (ECalendarView *cal_view, time_t start_time, time_t end_time)
{
	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	if (E_CALENDAR_VIEW_CLASS (G_OBJECT_GET_CLASS (cal_view))->set_selected_time_range) {
		E_CALENDAR_VIEW_CLASS (G_OBJECT_GET_CLASS (cal_view))->set_selected_time_range (
			cal_view, start_time, end_time);
	}
}

gboolean
e_calendar_view_get_visible_time_range (ECalendarView *cal_view, time_t *start_time, time_t *end_time)
{
	g_return_val_if_fail (E_IS_CALENDAR_VIEW (cal_view), FALSE);

	if (E_CALENDAR_VIEW_CLASS (G_OBJECT_GET_CLASS (cal_view))->get_visible_time_range) {
		return E_CALENDAR_VIEW_CLASS (G_OBJECT_GET_CLASS (cal_view))->get_visible_time_range (
			cal_view, start_time, end_time);
	}

	return FALSE;
}

void
e_calendar_view_update_query (ECalendarView *cal_view)
{
	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	if (E_CALENDAR_VIEW_CLASS (G_OBJECT_GET_CLASS (cal_view))->update_query) {
		E_CALENDAR_VIEW_CLASS (G_OBJECT_GET_CLASS (cal_view))->update_query (cal_view);
	}
}

void
e_calendar_view_cut_clipboard (ECalendarView *cal_view)
{
	GList *selected, *l;
	const char *uid;

	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	selected = e_calendar_view_get_selected_events (cal_view);
	if (!selected)
		return;

	e_calendar_view_set_status_message (cal_view, _("Deleting selected objects"));

	e_calendar_view_copy_clipboard (cal_view);
	for (l = selected; l != NULL; l = l->next) {
		ECalComponent *comp;
		ECalendarViewEvent *event = (ECalendarViewEvent *) l->data;
		GError *error = NULL;
		
		if (!event)
			continue;

		comp = e_cal_component_new ();
		e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (event->comp_data->icalcomp));

		if (itip_organizer_is_user (comp, event->comp_data->client) 
		    && cancel_component_dialog ((GtkWindow *) gtk_widget_get_toplevel (GTK_WIDGET (cal_view)),
						event->comp_data->client, comp, TRUE))
			itip_send_comp (E_CAL_COMPONENT_METHOD_CANCEL, comp,
					event->comp_data->client, NULL);

		e_cal_component_get_uid (comp, &uid);
		e_cal_remove_object (event->comp_data->client, uid, &error);
		delete_error_dialog (error, E_CAL_COMPONENT_EVENT);

		g_clear_error (&error);
		
		g_object_unref (comp);
	}

	e_calendar_view_set_status_message (cal_view, NULL);

	g_list_free (selected);
}

void
e_calendar_view_copy_clipboard (ECalendarView *cal_view)
{
	GList *selected, *l;
	gchar *comp_str;
	icalcomponent *vcal_comp;
	icalcomponent *new_icalcomp;
	ECalendarViewEvent *event;

	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	selected = e_calendar_view_get_selected_events (cal_view);
	if (!selected)
		return;

	/* create top-level VCALENDAR component and add VTIMEZONE's */
	vcal_comp = e_cal_util_new_top_level ();
	for (l = selected; l != NULL; l = l->next) {
		event = (ECalendarViewEvent *) l->data;

		if (event)
			e_cal_util_add_timezones_from_component (vcal_comp, event->comp_data->icalcomp);
	}

	for (l = selected; l != NULL; l = l->next) {
		event = (ECalendarViewEvent *) l->data;

		new_icalcomp = icalcomponent_new_clone (event->comp_data->icalcomp);
		icalcomponent_add_component (vcal_comp, new_icalcomp);
	}

	/* copy the VCALENDAR to the clipboard */
	comp_str = icalcomponent_as_ical_string (vcal_comp);
	gtk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (cal_view), clipboard_atom),
				(const gchar *) comp_str,
				g_utf8_strlen (comp_str, -1));

	/* free memory */
	icalcomponent_free (vcal_comp);
	g_list_free (selected);
}

static void
clipboard_get_text_cb (GtkClipboard *clipboard, const gchar *text, ECalendarView *cal_view)
{
	icalcomponent *icalcomp;
	icalcomponent_kind kind;
	time_t selected_time_start, selected_time_end;
	icaltimezone *default_zone;
	ECal *client;
	gboolean in_top_canvas;

	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	if (!text || !*text)
		return;

	icalcomp = icalparser_parse_string ((const char *) text);
	if (!icalcomp)
		return;

	default_zone = calendar_config_get_icaltimezone ();
	client = e_cal_model_get_default_client (cal_view->priv->model);

	/* check the type of the component */
	/* FIXME An error dialog if we return? */
	kind = icalcomponent_isa (icalcomp);
	if (kind != ICAL_VCALENDAR_COMPONENT && kind != ICAL_VEVENT_COMPONENT)
		return;

	e_calendar_view_set_status_message (cal_view, _("Updating objects"));
	e_calendar_view_get_selected_time_range (cal_view, &selected_time_start, &selected_time_end);

	if ((selected_time_end - selected_time_start) == 60 * 60 * 24)
		in_top_canvas = TRUE;
	else
		in_top_canvas = FALSE;

	/* FIXME Timezone handling */
	if (kind == ICAL_VCALENDAR_COMPONENT) {
		icalcomponent_kind child_kind;
		icalcomponent *subcomp;

		subcomp = icalcomponent_get_first_component (icalcomp, ICAL_ANY_COMPONENT);
		while (subcomp) {
			child_kind = icalcomponent_isa (subcomp);
			if (child_kind == ICAL_VEVENT_COMPONENT)
				e_calendar_view_add_event (cal_view, client, selected_time_start, 
							   default_zone, subcomp, in_top_canvas);
			else if (child_kind == ICAL_VTIMEZONE_COMPONENT) {
				icaltimezone *zone;

				zone = icaltimezone_new ();
				icaltimezone_set_component (zone, subcomp);
				e_cal_add_timezone (client, zone, NULL);
				
				icaltimezone_free (zone, 1);
			}
			
			subcomp = icalcomponent_get_next_component (
				icalcomp, ICAL_ANY_COMPONENT);
		}

		icalcomponent_free (icalcomp);

	} else {
		e_calendar_view_add_event (cal_view, client, selected_time_start, default_zone, icalcomp, in_top_canvas);
	}

	e_calendar_view_set_status_message (cal_view, NULL);
}

void
e_calendar_view_paste_clipboard (ECalendarView *cal_view)
{
	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	gtk_clipboard_request_text (gtk_widget_get_clipboard (GTK_WIDGET (cal_view), clipboard_atom),
				    (GtkClipboardTextReceivedFunc) clipboard_get_text_cb, cal_view);
}

static void
delete_event (ECalendarView *cal_view, ECalendarViewEvent *event)
{
	ECalComponent *comp;
	ECalComponentVType vtype;

	comp = e_cal_component_new ();
	e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (event->comp_data->icalcomp));
	vtype = e_cal_component_get_vtype (comp);

	if (delete_component_dialog (comp, FALSE, 1, vtype, GTK_WIDGET (cal_view))) {
		const char *uid;
		GError *error = NULL;
		
		if (itip_organizer_is_user (comp, event->comp_data->client) 
		    && cancel_component_dialog ((GtkWindow *) gtk_widget_get_toplevel (GTK_WIDGET (cal_view)),
						event->comp_data->client,
						comp, TRUE))
			itip_send_comp (E_CAL_COMPONENT_METHOD_CANCEL, comp,
					event->comp_data->client, NULL);

		e_cal_component_get_uid (comp, &uid);
		if (!uid || !*uid) {
			g_object_unref (comp);
			return;
		}
		
		e_cal_remove_object (event->comp_data->client, uid, &error);
		delete_error_dialog (error, E_CAL_COMPONENT_EVENT);
		g_clear_error (&error);
	}

	g_object_unref (comp);
}

void
e_calendar_view_delete_selected_event (ECalendarView *cal_view)
{
	GList *selected;
	ECalendarViewEvent *event;

	selected = e_calendar_view_get_selected_events (cal_view);
	if (!selected)
		return;

	event = (ECalendarViewEvent *) selected->data;
	if (event)
		delete_event (cal_view, event);

	g_list_free (selected);
}

void
e_calendar_view_delete_selected_events (ECalendarView *cal_view)
{
	GList *selected, *l;
	ECalendarViewEvent *event;

	selected = e_calendar_view_get_selected_events (cal_view);
	if (!selected)
		return;

	for (l = selected; l != NULL; l = l->next) {
		event = (ECalendarViewEvent *) l->data;
		if (event)
			delete_event (cal_view, event);
	}

	g_list_free (selected);
}

void
e_calendar_view_delete_selected_occurrence (ECalendarView *cal_view)
{
	ECalendarViewEvent *event;
	GList *selected;
	const char *uid, *rid = NULL;
	GError *error = NULL;
	ECalComponent *comp;
		
	selected = e_calendar_view_get_selected_events (cal_view);
	if (!selected)
		return;

	event = (ECalendarViewEvent *) selected->data;

	comp = e_cal_component_new ();
	e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (event->comp_data->icalcomp));
	e_cal_component_get_uid (comp, &uid);
	if (e_cal_component_is_instance (comp))
		rid = e_cal_component_get_recurid_as_string (comp);
	else {
		ECalComponentDateTime dt;
		icaltimezone *zone;

		if (!e_cal_component_has_recurrences (comp)) {
			g_object_unref (comp);
			return;
		}

		/* get the RECUR-ID from the instance start date */
		e_cal_component_get_dtstart (comp, &dt);
		if (e_cal_get_timezone (event->comp_data->client, dt.tzid, &zone, NULL)) {
			rid = icaltime_as_ical_string (
				icaltime_from_timet_with_zone (event->comp_data->instance_start, TRUE, zone));
		} else
			rid = icaltime_as_ical_string (icaltime_from_timet (event->comp_data->instance_start, TRUE));

		e_cal_component_free_datetime (&dt);
	}

	if (rid) {
		if (delete_component_dialog (comp, FALSE, 1, e_cal_component_get_vtype (comp), GTK_WIDGET (cal_view))) {

			if (itip_organizer_is_user (comp, event->comp_data->client)
			    && cancel_component_dialog ((GtkWindow *) gtk_widget_get_toplevel (GTK_WIDGET (cal_view)),
							event->comp_data->client,
							comp, TRUE))
				itip_send_comp (E_CAL_COMPONENT_METHOD_CANCEL, comp, event->comp_data->client, NULL);

			e_cal_remove_object_with_mod (event->comp_data->client, uid, rid, CALOBJ_MOD_THIS, &error);
			delete_error_dialog (error, E_CAL_COMPONENT_EVENT);
			g_clear_error (&error);
		}
	}

	/* free memory */
	g_list_free (selected);
	g_object_unref (comp);
}

static void
on_new_appointment (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarView *cal_view = data;

	e_calendar_view_new_appointment (cal_view);
}

static void
on_new_event (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarView *cal_view = data;

	e_calendar_view_new_appointment_full (cal_view, TRUE, FALSE);
}

static void
on_new_meeting (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarView *cal_view = data;

	e_calendar_view_new_appointment_full (cal_view, FALSE, TRUE);
}

static void
on_new_task (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarView *cal_view = data;

	gnome_calendar_new_task (cal_view->priv->calendar);
}

static void
on_goto_date (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarView *cal_view = data;

	goto_dialog (cal_view->priv->calendar);
}

static void
on_goto_today (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarView *cal_view = data;

	calendar_goto_today (cal_view->priv->calendar);
}

static void
on_edit_appointment (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarView *cal_view = data;
	GList *selected;

	selected = e_calendar_view_get_selected_events (cal_view);
	if (selected) {
		ECalendarViewEvent *event = (ECalendarViewEvent *) selected->data;

		if (event)
			e_calendar_view_edit_appointment (cal_view, event->comp_data->client,
						     event->comp_data->icalcomp, FALSE);

		g_list_free (selected);
	}
}

static void
on_print (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarView *cal_view = data;
	time_t start, end;
	GnomeCalendarViewType view_type;
	PrintView print_view;

	e_calendar_view_get_visible_time_range (cal_view, &start, &end);
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
on_save_as (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarView *cal_view = data;
	GList *selected;
	char *filename;
	char *ical_string;
	FILE *file;
	ECalendarViewEvent *event;

	selected = e_calendar_view_get_selected_events (cal_view);
	if (!selected)
		return;

	filename = e_file_dialog_save (_("Save as..."));
	if (filename == NULL)
		return;
	
	event = (ECalendarViewEvent *) selected->data;
	ical_string = e_cal_get_component_as_string (event->comp_data->client, event->comp_data->icalcomp);
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
on_print_event (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarView *cal_view = data;
	GList *selected;
	ECalendarViewEvent *event;
	ECalComponent *comp;

	selected = e_calendar_view_get_selected_events (cal_view);
	if (!selected)
		return;

	event = (ECalendarViewEvent *) selected->data;

	comp = e_cal_component_new ();
	e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (event->comp_data->icalcomp));
	print_comp (comp, event->comp_data->client, FALSE);

	g_object_unref (comp);
	g_list_free (selected);
}

static void
transfer_item_to (ECalendarViewEvent *event, ECal *dest_client, gboolean remove_item)
{
	const char *uid;
	char *new_uid;
	icalcomponent *orig_icalcomp;

	uid = icalcomponent_get_uid (event->comp_data->icalcomp);

	/* put the new object into the destination calendar */
	if (e_cal_get_object (dest_client, uid, NULL, &orig_icalcomp, NULL)) {
		icalcomponent_free (orig_icalcomp);

		if (!e_cal_modify_object (dest_client, event->comp_data->icalcomp, CALOBJ_MOD_ALL, NULL))
			return;
	} else {
		orig_icalcomp = icalcomponent_new_clone (event->comp_data->icalcomp);

		if (!remove_item) {
			/* change the UID to avoid problems with duplicated UIDs */
			new_uid = e_cal_component_gen_uid ();
			icalcomponent_set_uid (orig_icalcomp, new_uid);

			g_free (new_uid);
		}

		new_uid = NULL;
		if (!e_cal_create_object (dest_client, orig_icalcomp, &new_uid, NULL)) {
			icalcomponent_free (orig_icalcomp);
			return;
		}

		if (new_uid)
			g_free (new_uid);
		icalcomponent_free (orig_icalcomp);
	}

	/* remove the item from the source calendar */
	if (remove_item)
		e_cal_remove_object (event->comp_data->client, uid, NULL);
}

static void
transfer_selected_items (ECalendarView *cal_view, gboolean remove_item)
{
	GList *selected, *l;
	ESource *destination_source;
	ECal *dest_client;

	selected = e_calendar_view_get_selected_events (cal_view);
	if (!selected)
		return;

	/* prompt the user for destination source */
	destination_source = select_source_dialog ((GtkWindow *) gtk_widget_get_toplevel (cal_view), E_CAL_SOURCE_TYPE_EVENT);
	if (!destination_source)
		return;

	/* open the destination calendar */
	dest_client = auth_new_cal_from_source (destination_source, E_CAL_SOURCE_TYPE_EVENT);
	if (!dest_client || !e_cal_open (dest_client, FALSE, NULL)) {
		if (dest_client)
			g_object_unref (dest_client);
		g_object_unref (destination_source);
		return;
	}

	/* process all selected events */
	if (remove_item)
		e_calendar_view_set_status_message (cal_view, _("Moving items"));
	else
		e_calendar_view_set_status_message (cal_view, _("Copying items"));

	for (l = selected; l != NULL; l = l->next)
		transfer_item_to ((ECalendarViewEvent *) l->data, dest_client, remove_item);

	e_calendar_view_set_status_message (cal_view, NULL);

	/* free memory */
	g_object_unref (destination_source);
	g_object_unref (dest_client);
	g_list_free (selected);
}

static void
on_copy_to (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarView *cal_view = data;

	transfer_selected_items (cal_view, FALSE);
}

static void
on_move_to (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarView *cal_view = data;

	transfer_selected_items (cal_view, TRUE);
}

static void
on_meeting (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarView *cal_view = data;
	GList *selected;

	selected = e_calendar_view_get_selected_events (cal_view);
	if (selected) {
		ECalendarViewEvent *event = (ECalendarViewEvent *) selected->data;
		e_calendar_view_edit_appointment (cal_view, event->comp_data->client, event->comp_data->icalcomp, TRUE);

		g_list_free (selected);
	}
}

static void
on_forward (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarView *cal_view = data;
	GList *selected;

	selected = e_calendar_view_get_selected_events (cal_view);
	if (selected) {
		ECalComponent *comp;
		ECalendarViewEvent *event = (ECalendarViewEvent *) selected->data;

		comp = e_cal_component_new ();
		e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (event->comp_data->icalcomp));
		itip_send_comp (E_CAL_COMPONENT_METHOD_PUBLISH, comp, event->comp_data->client, NULL);

		g_list_free (selected);
		g_object_unref (comp);
	}
}

static void
on_publish (EPopup *ep, EPopupItem *pitem, void *data)
{
	e_pub_publish (TRUE);
}

static void
on_delete_appointment (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarView *cal_view = data;

	e_calendar_view_delete_selected_event (cal_view);
}

static void
on_unrecur_appointment (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarView *cal_view = data;
	ECalendarViewEvent *event;
	ECalComponent *comp, *new_comp;
	ECalComponentDateTime date;
	struct icaltimetype itt;
	GList *selected;
	char *new_uid;

	selected = e_calendar_view_get_selected_events (cal_view);
	if (!selected)
		return;

	event = (ECalendarViewEvent *) selected->data;

	date.value = &itt;
	date.tzid = NULL;

	/* For the recurring object, we add an exception to get rid of the
	   instance. */

	comp = e_cal_component_new ();
	e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (event->comp_data->icalcomp));
	cal_comp_util_add_exdate (comp, event->comp_data->instance_start, e_calendar_view_get_timezone (cal_view));
	e_cal_component_commit_sequence (comp);

	/* For the unrecurred instance we duplicate the original object,
	   create a new uid for it, get rid of the recurrence rules, and set
	   the start & end times to the instances times. */
	new_comp = e_cal_component_new ();
	e_cal_component_set_icalcomponent (new_comp, icalcomponent_new_clone (event->comp_data->icalcomp));
	new_uid = e_cal_component_gen_uid ();
	e_cal_component_set_uid (new_comp, new_uid);
	g_free (new_uid);
	e_cal_component_set_rdate_list (new_comp, NULL);
	e_cal_component_set_rrule_list (new_comp, NULL);
	e_cal_component_set_exdate_list (new_comp, NULL);
	e_cal_component_set_exrule_list (new_comp, NULL);

	date.value = &itt;
	date.tzid = icaltimezone_get_tzid (e_calendar_view_get_timezone (cal_view));

	*date.value = icaltime_from_timet_with_zone (event->comp_data->instance_start, FALSE,
						     e_calendar_view_get_timezone (cal_view));
	e_cal_component_set_dtstart (new_comp, &date);
	*date.value = icaltime_from_timet_with_zone (event->comp_data->instance_end, FALSE,
						     e_calendar_view_get_timezone (cal_view));
	e_cal_component_set_dtend (new_comp, &date);
	e_cal_component_commit_sequence (new_comp);

	/* Now update both ECalComponents. Note that we do this last since at
	 * present the updates happen synchronously so our event may disappear.
	 */
	if (!e_cal_modify_object (event->comp_data->client, e_cal_component_get_icalcomponent (comp), CALOBJ_MOD_THIS, NULL))
		g_message ("e_day_view_on_unrecur_appointment(): Could not update the object!");

	g_object_unref (comp);

	if (!e_cal_create_object (event->comp_data->client, e_cal_component_get_icalcomponent (new_comp), &new_uid, NULL))
		g_message ("e_day_view_on_unrecur_appointment(): Could not update the object!");
	else
		g_free (new_uid);

	g_object_unref (new_comp);
	g_list_free (selected);
}

static void
on_delete_occurrence (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarView *cal_view = data;

	e_calendar_view_delete_selected_occurrence (cal_view);
}

static void
on_cut (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarView *cal_view = data;

	e_calendar_view_cut_clipboard (cal_view);
}

static void
on_copy (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarView *cal_view = data;

	e_calendar_view_copy_clipboard (cal_view);
}

static void
on_paste (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarView *cal_view = data;

	e_calendar_view_paste_clipboard (cal_view);
}

static EPopupItem ecv_main_items [] = {
	{ E_POPUP_ITEM, "00.new", N_("New _Appointment..."), on_new_appointment, NULL, "stock_new-appointment", 0, 0 },
	{ E_POPUP_ITEM, "10.newallday", N_("New All Day _Event"), on_new_event, NULL, "stock_new-24h-appointment", 0, 0},
	{ E_POPUP_ITEM, "20.meeting", N_("New Meeting"), on_new_meeting, NULL, "stock_new-meeting", 0, 0},
	{ E_POPUP_ITEM, "30.task", N_("New Task"), on_new_task, NULL, "stock_task", 0, 0},

	{ E_POPUP_BAR, "40."},
	{ E_POPUP_ITEM, "40.print", N_("_Print..."), on_print, NULL, GTK_STOCK_PRINT, 0, 0 },

	{ E_POPUP_BAR, "50." },
	{ E_POPUP_ITEM, "50.paste", N_("_Paste"), on_paste, NULL, GTK_STOCK_PASTE, 0, E_CAL_POPUP_SELECT_EDITABLE },

	{ E_POPUP_BAR, "60." },
	/* FIXME: hook in this somehow */
	{ E_POPUP_SUBMENU, "60.view", N_("Current View") },
	
	{ E_POPUP_ITEM, "61.today", N_("Select _Today"), on_goto_today, NULL, GTK_STOCK_HOME },
	{ E_POPUP_ITEM, "62.todate", N_("_Select Date..."), on_goto_date, NULL, GTK_STOCK_JUMP_TO },

	{ E_POPUP_BAR, "70." },

	/* TODO: Why is this in a context menu when it applies globally? */
	{ E_POPUP_ITEM, "70.publish", N_("_Publish Free/Busy Information"), on_publish, },
};

static EPopupItem ecv_child_items [] = {
	{ E_POPUP_ITEM, "00.open", N_("_Open"), on_edit_appointment, NULL, GTK_STOCK_OPEN, 0, E_CAL_POPUP_SELECT_NOTEDITING },
	{ E_POPUP_ITEM, "10.saveas", N_("_Save As..."), on_save_as, NULL, GTK_STOCK_SAVE_AS, 0, E_CAL_POPUP_SELECT_NOTEDITING },
	{ E_POPUP_ITEM, "20.print", N_("_Print..."), on_print_event, NULL, GTK_STOCK_PRINT, 0, E_CAL_POPUP_SELECT_NOTEDITING },

	{ E_POPUP_BAR, "30." },

	{ E_POPUP_ITEM, "31.cut", N_("C_ut"), on_cut, NULL, GTK_STOCK_CUT, 0, E_CAL_POPUP_SELECT_NOTEDITING|E_CAL_POPUP_SELECT_EDITABLE|E_CAL_POPUP_SELECT_ORGANIZER },
	{ E_POPUP_ITEM, "32.copy", N_("_Copy"), on_copy, NULL, GTK_STOCK_COPY, 0, E_CAL_POPUP_SELECT_NOTEDITING|E_CAL_POPUP_SELECT_ORGANIZER },
	{ E_POPUP_ITEM, "33.paste", N_("_Paste"), on_paste, NULL, GTK_STOCK_PASTE, 0, E_CAL_POPUP_SELECT_EDITABLE },

	{ E_POPUP_BAR, "40." },

	{ E_POPUP_ITEM, "41.copyto", N_("Cop_y to Calendar..."), on_copy_to, NULL, NULL, 0, E_CAL_POPUP_SELECT_NOTEDITING },
	{ E_POPUP_ITEM, "42.moveto", N_("Mo_ve to Calendar..."), on_move_to, NULL, NULL, 0, E_CAL_POPUP_SELECT_NOTEDITING | E_CAL_POPUP_SELECT_EDITABLE },
	{ E_POPUP_ITEM, "43.schedule", N_("_Schedule Meeting..."), on_meeting, NULL, NULL, 0, E_CAL_POPUP_SELECT_NOTEDITING | E_CAL_POPUP_SELECT_EDITABLE | E_CAL_POPUP_SELECT_NOTMEETING },
	{ E_POPUP_ITEM, "44.forward", N_("_Forward as iCalendar..."), on_forward, NULL, "stock_mail-forward", 0, E_CAL_POPUP_SELECT_NOTEDITING },

	{ E_POPUP_BAR, "50." },

	{ E_POPUP_ITEM, "51.delete", N_("_Delete"), on_delete_appointment, NULL, GTK_STOCK_DELETE, E_CAL_POPUP_SELECT_NONRECURRING, E_CAL_POPUP_SELECT_NOTEDITING | E_CAL_POPUP_SELECT_EDITABLE },
	{ E_POPUP_ITEM, "52.move", N_("Make this Occurrence _Movable"), on_unrecur_appointment, NULL, NULL, E_CAL_POPUP_SELECT_RECURRING | E_CAL_POPUP_SELECT_INSTANCE, E_CAL_POPUP_SELECT_NOTEDITING | E_CAL_POPUP_SELECT_EDITABLE },
	{ E_POPUP_ITEM, "53.delete", N_("Delete this _Occurrence"), on_delete_occurrence, NULL, GTK_STOCK_DELETE, E_CAL_POPUP_SELECT_RECURRING, E_CAL_POPUP_SELECT_NOTEDITING | E_CAL_POPUP_SELECT_EDITABLE },
	{ E_POPUP_ITEM, "54.delete", N_("Delete _All Occurrences"), on_delete_appointment, NULL, GTK_STOCK_DELETE, E_CAL_POPUP_SELECT_RECURRING, E_CAL_POPUP_SELECT_NOTEDITING | E_CAL_POPUP_SELECT_EDITABLE },
};

static void
ecv_popup_free (EPopup *ep, GSList *list, void *data)
{
	g_slist_free(list);
}

GtkMenu *
e_calendar_view_create_popup_menu (ECalendarView *cal_view)
{
	ECalPopup *ep;
	GSList *menus = NULL;
	GList *selected, *l;
	int i;
	ECalPopupTargetSelect *t;
	ECalModel *model;
	GPtrArray *events;

	g_return_val_if_fail (E_IS_CALENDAR_VIEW (cal_view), NULL);

	/* We could do this using a factory on the ECalPopup class,
	 * that way we would get called implicitly whenever a popup
	 * menu was created rather than everyone having to call us.
	 * We could also have a different menu id for each view */

	ep = e_cal_popup_new("com.novell.evolution.calendar.view.popup");

	model = e_calendar_view_get_model(cal_view);
	events = g_ptr_array_new();
	selected = e_calendar_view_get_selected_events(cal_view);
	for (l=selected;l;l=g_list_next(l)) {
		ECalendarViewEvent *event = l->data;

		if (event)
			g_ptr_array_add(events, e_cal_model_copy_component_data(event->comp_data));
	}
	g_list_free(selected);

	t = e_cal_popup_target_new_select(ep, model, events);
	t->target.widget = (GtkWidget *)cal_view;

	if (t->events->len == 0) {
		for (i=0;i<sizeof(ecv_main_items)/sizeof(ecv_main_items[0]);i++)
			menus = g_slist_prepend(menus, &ecv_main_items[i]);

		gnome_calendar_view_popup_factory(cal_view->priv->calendar, (EPopup *)ep, "60.view");
	} else {
		for (i=0;i<sizeof(ecv_child_items)/sizeof(ecv_child_items[0]);i++)
			menus = g_slist_prepend(menus, &ecv_child_items[i]);
	}

	e_popup_add_items((EPopup *)ep, menus, ecv_popup_free, cal_view);

	return e_popup_create_menu_once((EPopup *)ep, (EPopupTarget *)t, 0);
}

void 
e_calendar_view_open_event (ECalendarView *cal_view)
{
	GList *selected;

	selected = e_calendar_view_get_selected_events (cal_view);
	if (selected) {
		ECalendarViewEvent *event = (ECalendarViewEvent *) selected->data;

		if (event)
			e_calendar_view_edit_appointment (cal_view, event->comp_data->client,
						     event->comp_data->icalcomp, FALSE);

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
				     time_t dtstart, time_t dtend,
				     gboolean all_day,
				     gboolean meeting)
{
	ECalendarViewPrivate *priv;
	struct icaltimetype itt;
	ECalComponentDateTime dt;
	ECalComponent *comp;
	icalcomponent *icalcomp;
	ECalComponentTransparency transparency;

	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	priv = cal_view->priv;

	dt.value = &itt;
	if (all_day)
		dt.tzid = NULL;
	else
		dt.tzid = icaltimezone_get_tzid (e_cal_model_get_timezone (cal_view->priv->model));

	icalcomp = e_cal_model_create_component_with_defaults (priv->model);
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
		   already set to midnight */
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

	e_calendar_view_edit_appointment (cal_view,
				     e_cal_model_get_default_client (priv->model),
				     icalcomp, meeting);

	g_object_unref (comp);
}

/**
 * e_calendar_view_new_appointment
 * @cal_view: A calendar view.
 *
 * Opens an event editor dialog for a new appointment. The appointment's
 * start and end times are set to the currently selected time range in
 * the calendar view.
 */
void
e_calendar_view_new_appointment_full (ECalendarView *cal_view, gboolean all_day, gboolean meeting)
{
	time_t dtstart, dtend;

	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	if (!e_calendar_view_get_selected_time_range (cal_view, &dtstart, &dtend)) {
		dtstart = time (NULL);
		dtend = dtstart + 3600;
	}
	/* FIXME This is a rough hack to make sure "all day" is set for */
	if ((dtend - dtstart) % (60 * 60 * 24) == 0)
		all_day = TRUE;
	
	e_calendar_view_new_appointment_for (cal_view, dtstart, dtend, all_day, meeting);
}

void
e_calendar_view_new_appointment (ECalendarView *cal_view)
{
	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	e_calendar_view_new_appointment_full (cal_view, FALSE, FALSE);
}

/**
 * e_calendar_view_edit_appointment
 * @cal_view: A calendar view.
 * @client: Calendar client.
 * @icalcomp: The object to be edited.
 * @meeting: Whether the appointment is a meeting or not.
 *
 * Opens an editor window to allow the user to edit the selected
 * object.
 */
void
e_calendar_view_edit_appointment (ECalendarView *cal_view,
			     ECal *client,
			     icalcomponent *icalcomp,
			     gboolean meeting)
{
	ECalendarViewPrivate *priv;
	CompEditor *ce;
	const char *uid;
	ECalComponent *comp;

	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));
	g_return_if_fail (E_IS_CAL (client));
	g_return_if_fail (icalcomp != NULL);

	priv = cal_view->priv;

	uid = icalcomponent_get_uid (icalcomp);

	ce = e_comp_editor_registry_find (comp_editor_registry, uid);
	if (!ce) {
		EventEditor *ee;

		ee = event_editor_new (client);
		ce = COMP_EDITOR (ee);

		comp = e_cal_component_new ();
		e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (icalcomp));
		comp_editor_edit_comp (ce, comp);
		if (meeting)
			event_editor_show_meeting (ee);

		e_comp_editor_registry_add (comp_editor_registry, ce, FALSE);

		g_object_unref (comp);
	}

	comp_editor_focus (ce);
}

void
e_calendar_view_modify_and_send (ECalComponent *comp,
				 ECal *client,
				 CalObjModType mod,
				 GtkWindow *toplevel,
				 gboolean new)
{
	if (e_cal_modify_object (client, e_cal_component_get_icalcomponent (comp), mod, NULL)) {
		if (itip_organizer_is_user (comp, client) &&
		    send_component_dialog (toplevel, client, comp, new))
			itip_send_comp (E_CAL_COMPONENT_METHOD_REQUEST, comp, client, NULL);
	} else {
		g_message (G_STRLOC ": Could not update the object!");
	}
}
