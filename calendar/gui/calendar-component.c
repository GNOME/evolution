/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>  
 *
 *
 * Authors:
 *		Ettore Perazzoli <ettore@ximian.com>
 *      Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <errno.h>
#include <glib/gi18n.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-exception.h>
#include <libical/icalvcal.h>
#include <libedataserver/e-data-server-util.h>
#include <libedataserver/e-url.h>
#include <libecal/e-cal-time-util.h>
#include <libedataserverui/e-source-selector.h>
#include <shell/e-user-creatable-items-handler.h>
#include <shell/e-component-view.h>
#include "e-calendar-view.h"
#include "calendar-config-keys.h"
#include "calendar-config.h"
#include "calendar-component.h"
#include "calendar-commands.h"
#include "control-factory.h"
#include "gnome-cal.h"
#include "migration.h"
#include "e-comp-editor-registry.h"
#include "comp-util.h"
#include "common/authentication.h"
#include "dialogs/calendar-setup.h"
#include "dialogs/comp-editor.h"
#include "dialogs/copy-source-dialog.h"
#include "dialogs/event-editor.h"
#include "misc/e-info-label.h"
#include "e-util/e-non-intrusive-error-dialog.h"
#include "e-util/gconf-bridge.h"
#include "e-util/e-error.h"
#include "e-cal-menu.h"
#include "e-cal-popup.h"

/* IDs for user creatable items */
#define CREATE_EVENT_ID        "event"
#define CREATE_MEETING_ID      "meeting"
#define CREATE_ALLDAY_EVENT_ID "allday-event"
#define CREATE_CALENDAR_ID      "calendar"
#define CALENDAR_ERROR_LEVEL_KEY "/apps/evolution/calendar/display/error_level"
#define CALENDAR_ERROR_TIME_OUT_KEY "/apps/evolution/calendar/display/error_timeout" 

static BonoboObjectClass *parent_class = NULL;

typedef struct
{
	ESourceList *source_list;
	ESourceList *task_source_list;
	ESourceList *memo_source_list;

	GSList *source_selection;
	GSList *task_source_selection;
	GSList *memo_source_selection;

	GnomeCalendar *calendar;

	GtkWidget *source_selector;

	BonoboControl *view_control;

	GList *notifications;

	float	     vpane_pos;
} CalendarComponentView;

struct _CalendarComponentPrivate {

	int gconf_notify_id;

	ESourceList *source_list;
	ESourceList *task_source_list;
	ESourceList *memo_source_list;

	EActivityHandler *activity_handler;
        ELogger *logger;

	GList *views;

	ECal *create_ecal;

	GList *notifications;
};

static void
calcomp_vpane_realized (GtkWidget *vpane, CalendarComponentView *view)
{
	gtk_paned_set_position (GTK_PANED (vpane), (int)(view->vpane_pos*vpane->allocation.height));

}

static gboolean
calcomp_vpane_resized (GtkWidget *vpane, GdkEventButton *e, CalendarComponentView *view)
{

	view->vpane_pos = gtk_paned_get_position (GTK_PANED (vpane));
	calendar_config_set_tag_vpane_pos (view->vpane_pos/(float)vpane->allocation.height);

	return FALSE;
}

/* Utility functions.  */

static gboolean
is_in_selection (GSList *selection, ESource *source)
{
	GSList *l;

	for (l = selection; l; l = l->next) {
		ESource *selected_source = l->data;

		if (!strcmp (e_source_peek_uid (selected_source), e_source_peek_uid (source)))
			return TRUE;
	}

	return FALSE;
}

static gboolean
is_in_uids (GSList *uids, ESource *source)
{
	GSList *l;

	for (l = uids; l; l = l->next) {
		const char *uid = l->data;

		if (!strcmp (uid, e_source_peek_uid (source)))
			return TRUE;
	}

	return FALSE;
}

static void
update_task_memo_selection (CalendarComponentView *component_view, ECalSourceType type)
{
	GSList *uids_selected, *l, *source_selection;
	ESourceList *source_list = NULL;

	if (type == E_CAL_SOURCE_TYPE_TODO) {
		/* Get the selection in gconf */
		uids_selected = calendar_config_get_tasks_selected ();
		source_list = component_view->task_source_list;
		source_selection = component_view->task_source_selection;
	} else {
		uids_selected = calendar_config_get_memos_selected ();
		source_list = component_view->memo_source_list;
		source_selection = component_view->memo_source_selection;
	}

	/* Remove any that aren't there any more */
	for (l = source_selection; l; l = l->next) {
		char *uid = l->data;
		ESource *source;

		source = e_source_list_peek_source_by_uid (component_view->source_list, uid);
		if (!source)
			gnome_calendar_remove_source_by_uid (component_view->calendar, type, uid);
		else if (!is_in_uids (uids_selected, source))
			gnome_calendar_remove_source (component_view->calendar, type, source);

		g_free (uid);
	}
	g_slist_free (source_selection);

	/* Make sure the whole selection is there */
	for (l = uids_selected; l; l = l->next) {
		char *uid = l->data;
		ESource *source;

		source = e_source_list_peek_source_by_uid (source_list, uid);
		if (source && !gnome_calendar_add_source (component_view->calendar, type, source))
			/* FIXME do something */;
	}

	if (type == E_CAL_SOURCE_TYPE_TODO)
		component_view->task_source_selection = uids_selected;
	else
		component_view->memo_source_selection = uids_selected;
}

static void
update_primary_task_memo_selection (CalendarComponentView *component_view, ECalSourceType type)
{
	ESource *source = NULL;
	char *uid;
	ESourceList *source_list = NULL;

	if (type == E_CAL_SOURCE_TYPE_TODO) {
		uid = calendar_config_get_primary_tasks ();
		source_list = component_view->task_source_list;
	} else {
		uid = calendar_config_get_primary_memos ();
		source_list = component_view->memo_source_list;
	}

	if (uid) {
		source = e_source_list_peek_source_by_uid (source_list, uid);

		g_free (uid);
	}

	if (source)
		gnome_calendar_set_default_source (component_view->calendar, type, source);
}

static void
config_primary_selection_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	CalendarComponent *calendar_component = data;
	CalendarComponentPrivate *priv = calendar_component->priv;

	if (priv->create_ecal) {
		g_object_unref (priv->create_ecal);
		priv->create_ecal = NULL;
	}
}

static void
config_tasks_selection_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	update_task_memo_selection (data, E_CAL_SOURCE_TYPE_TODO);
}


static void
config_primary_tasks_selection_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	update_primary_task_memo_selection (data, E_CAL_SOURCE_TYPE_TODO);
}

static void
config_memos_selection_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	update_task_memo_selection (data, E_CAL_SOURCE_TYPE_JOURNAL);
}


static void
config_primary_memos_selection_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	update_primary_task_memo_selection (data, E_CAL_SOURCE_TYPE_JOURNAL);
}

/* Evolution::Component CORBA methods.  */
static void
impl_handleURI (PortableServer_Servant servant, const char *uri, CORBA_Environment *ev)
{
	CalendarComponent *calendar_component = CALENDAR_COMPONENT (bonobo_object_from_servant (servant));
	CalendarComponentPrivate *priv;
	GList *l;
	CalendarComponentView *view = NULL;
	char *src_uid = NULL;
	char *uid = NULL;
	char *rid = NULL;

	priv = calendar_component->priv;

	l = g_list_last (priv->views);
	if (!l)
		return;

	view = l->data;

	if (!strncmp (uri, "calendar:", 9)) {
		EUri *euri = e_uri_new (uri);
		const char *p;
		char *header, *content;
		size_t len, clen;
		time_t start = -1, end = -1;

		p = euri->query;
		if (p) {
			while (*p) {
				len = strcspn (p, "=&");

				/* If it's malformed, give up. */
				if (p[len] != '=')
					break;

				header = (char *) p;
				header[len] = '\0';
				p += len + 1;

				clen = strcspn (p, "&");

				content = g_strndup (p, clen);

				if (!g_ascii_strcasecmp (header, "startdate")) {
					start = time_from_isodate (content);
				} else if (!g_ascii_strcasecmp (header, "enddate")) {
					end = time_from_isodate (content);
				} else if (!g_ascii_strcasecmp (header, "source-uid")) {
					src_uid = g_strdup (content);
				} else if (!g_ascii_strcasecmp (header, "comp-uid")) {
					uid = g_strdup (content);
				} else if (!g_ascii_strcasecmp (header, "comp-rid")) {
					rid = g_strdup (content);
				}

				g_free (content);

				p += clen;
				if (*p == '&') {
					p++;
					if (!strcmp (p, "amp;"))
						p += 4;
				}
			}

			if (start != -1) {

				if (end == -1)
					end = start;
					gnome_calendar_set_selected_time_range (view->calendar, start, end);
			}
			if (src_uid && uid)
				gnome_calendar_edit_appointment (view->calendar, src_uid, uid, rid);

			g_free (src_uid);
			g_free (uid);
			g_free (rid);
		}
		e_uri_free (euri);
	}
}

static void
config_create_ecal_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	CalendarComponent *calendar_component = data;
	CalendarComponentPrivate *priv;

	priv = calendar_component->priv;

	g_object_unref (priv->create_ecal);
	priv->create_ecal = NULL;

	priv->notifications = g_list_remove (priv->notifications, GUINT_TO_POINTER (id));
}

static ECal *
setup_create_ecal (CalendarComponent *calendar_component, CalendarComponentView *component_view)
{
	CalendarComponentPrivate *priv;
	ESource *source = NULL;
	char *uid;
	guint not;

	priv = calendar_component->priv;

	/* Try to use the client from the calendar first to avoid re-opening things */
	if (component_view) {
		ECal *default_ecal;

		default_ecal = gnome_calendar_get_default_client (component_view->calendar);
		if (default_ecal)
			return default_ecal;
	}

	/* If there is an existing fall back, use that */
	if (priv->create_ecal)
		return priv->create_ecal;

	/* Get the current primary calendar, or try to set one if it doesn't already exist */
	uid = calendar_config_get_primary_calendar ();
	if (uid) {
		source = e_source_list_peek_source_by_uid (priv->source_list, uid);
		g_free (uid);

		priv->create_ecal = auth_new_cal_from_source (source, E_CAL_SOURCE_TYPE_EVENT);
	}

	if (!priv->create_ecal) {
		/* Try to create a default if there isn't one */
		source = e_source_list_peek_source_any (priv->source_list);
		if (source)
			priv->create_ecal = auth_new_cal_from_source (source, E_CAL_SOURCE_TYPE_EVENT);
	}

	if (priv->create_ecal) {
		icaltimezone *zone;

		zone = calendar_config_get_icaltimezone ();
		e_cal_set_default_timezone (priv->create_ecal, zone, NULL);

		if (!e_cal_open (priv->create_ecal, FALSE, NULL)) {
			GtkWidget *dialog;

			dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
							 GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
							 _("Unable to open the calendar '%s' for creating events and meetings"),
							 e_source_peek_name (source));

			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);
			g_object_unref (priv->create_ecal);
			priv->create_ecal = NULL;

			return NULL;
		}

	} else {
		GtkWidget *dialog;

		dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
						 GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
						 _("There is no calendar available for creating events and meetings"));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		return NULL;
	}

	/* Handle the fact it may change on us */
	not = calendar_config_add_notification_primary_calendar (config_create_ecal_changed_cb,
								 calendar_component);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));

	/* Save the primary source for use elsewhere */
	calendar_config_set_primary_calendar (e_source_peek_uid (source));

	return priv->create_ecal;
}

static CalendarComponentView *
create_component_view (CalendarComponent *calendar_component)
{
	CalendarComponentPrivate *priv;
	CalendarComponentView *component_view;
	GtkWidget **vpane;
	guint not;

	priv = calendar_component->priv;

	/* Create the calendar component view */
	component_view = g_new0 (CalendarComponentView, 1);

	vpane = gtk_vpaned_new ();
	g_signal_connect_after (vpane, "realize",
				G_CALLBACK(calcomp_vpane_realized), component_view);
	g_signal_connect (vpane, "button_release_event",
			  G_CALLBACK (calcomp_vpane_resized), component_view);
	gtk_widget_show (vpane);
	/* Add the source lists */
	component_view->source_list = g_object_ref (priv->source_list);
	component_view->task_source_list = g_object_ref (priv->task_source_list);
	component_view->memo_source_list = g_object_ref (priv->memo_source_list);
	/* Create sidebar selector */
	component_view->source_selector = e_source_selector_new (calendar_component->priv->source_list);

	gtk_widget_show (component_view->source_selector);

	/* Set up the "new" item handler */
	g_signal_connect (component_view->view_control, "activate", G_CALLBACK (control_activate_cb), component_view);

	/* Load the selection from the last run */
	update_task_memo_selection (component_view, E_CAL_SOURCE_TYPE_TODO);
	update_primary_task_memo_selection (component_view, E_CAL_SOURCE_TYPE_TODO);
	update_task_memo_selection (component_view, E_CAL_SOURCE_TYPE_JOURNAL);
	update_primary_task_memo_selection (component_view, E_CAL_SOURCE_TYPE_JOURNAL);

	/* If the tasks/memos selection changes elsewhere, update it for the mini
	   mini tasks view sidebar */
	not = calendar_config_add_notification_tasks_selected (config_tasks_selection_changed_cb,
							       component_view);
	component_view->notifications = g_list_prepend (component_view->notifications, GUINT_TO_POINTER (not));

	not = calendar_config_add_notification_memos_selected (config_memos_selection_changed_cb,
							       component_view);
	component_view->notifications = g_list_prepend (component_view->notifications, GUINT_TO_POINTER (not));

	not = calendar_config_add_notification_primary_tasks (config_primary_tasks_selection_changed_cb,
							      component_view);
	component_view->notifications = g_list_prepend (component_view->notifications, GUINT_TO_POINTER (not));

	not = calendar_config_add_notification_primary_memos (config_primary_memos_selection_changed_cb,
							      component_view);
	component_view->notifications = g_list_prepend (component_view->notifications, GUINT_TO_POINTER (not));

	return component_view;
}

static void
destroy_component_view (CalendarComponentView *component_view)
{
	GList *l;

	if (component_view->source_list)
		g_object_unref (component_view->source_list);

	if (component_view->task_source_list)
		g_object_unref (component_view->task_source_list);

	if (component_view->memo_source_list)
		g_object_unref (component_view->memo_source_list);

	if (component_view->source_selection)
		e_source_selector_free_selection (component_view->source_selection);

	for (l = component_view->notifications; l; l = l->next)
		calendar_config_remove_notification (GPOINTER_TO_UINT (l->data));
	g_list_free (component_view->notifications);

	if (component_view->task_source_selection) {
		g_slist_foreach (component_view->task_source_selection, (GFunc) g_free, NULL);
		g_slist_free (component_view->task_source_selection);
	}

	if (component_view->memo_source_selection) {
		g_slist_foreach (component_view->memo_source_selection, (GFunc) g_free, NULL);
		g_slist_free (component_view->memo_source_selection);
	}

	g_free (component_view);
}

static void
view_destroyed_cb (gpointer data, GObject *where_the_object_was)
{
	CalendarComponent *calendar_component = data;
	CalendarComponentPrivate *priv;
	GList *l;

	priv = calendar_component->priv;

	for (l = priv->views; l; l = l->next) {
		CalendarComponentView *component_view = l->data;

		if (G_OBJECT (component_view->view_control) == where_the_object_was) {
			priv->views = g_list_remove (priv->views, component_view);
			destroy_component_view (component_view);

			break;
		}
	}
}


/* GObject methods.  */

static void
impl_dispose (GObject *object)
{
	CalendarComponent *calendar_component = CALENDAR_COMPONENT (object);
	CalendarComponentPrivate *priv = calendar_component->priv;
	GList *l;

	if (priv->source_list != NULL) {
		g_object_unref (priv->source_list);
		priv->source_list = NULL;
	}

	if (priv->activity_handler != NULL) {
		g_object_unref (priv->activity_handler);
		priv->activity_handler = NULL;
	}

	if (priv->create_ecal) {
		g_object_unref (priv->create_ecal);
		priv->create_ecal = NULL;
	}

	for (l = priv->views; l; l = l->next) {
		CalendarComponentView *component_view = l->data;

		g_object_weak_unref (G_OBJECT (component_view->view_control), view_destroyed_cb, calendar_component);
	}
	g_list_free (priv->views);
	priv->views = NULL;

	for (l = priv->notifications; l; l = l->next)
		calendar_config_remove_notification (GPOINTER_TO_UINT (l->data));
	g_list_free (priv->notifications);
	priv->notifications = NULL;

	(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
calendar_component_class_init (CalendarComponentClass *class)
{
	POA_GNOME_Evolution_Component__epv *epv = &class->epv;
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	epv->handleURI               = impl_handleURI;

	object_class->dispose  = impl_dispose;
}

static void
calendar_component_init (CalendarComponent *component)
{
	CalendarComponentPrivate *priv;
	guint not;

	not = calendar_config_add_notification_primary_calendar (config_primary_selection_changed_cb,
								 component);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));

	priv->logger = e_logger_create ("calendar");
	priv->activity_handler = e_activity_handler_new ();
	e_activity_handler_set_logger (priv->activity_handler, priv->logger);
	e_activity_handler_set_error_flush_time (priv->activity_handler,eni_config_get_error_timeout (CALENDAR_ERROR_TIME_OUT_KEY)*1000);

	component->priv = priv;

	if (!e_cal_get_sources (&priv->task_source_list, E_CAL_SOURCE_TYPE_TODO, NULL))
		;
	if (!e_cal_get_sources (&priv->memo_source_list, E_CAL_SOURCE_TYPE_JOURNAL, NULL))
		;
}
