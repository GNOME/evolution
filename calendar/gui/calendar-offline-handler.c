/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* calendar-offline-handler.c
 *
 * Copyright (C) 2001  Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 *   Ettore Perazzoli <ettore@ximian.com>
 *   Dan Winship <danw@ximian.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-i18n.h>
#include "e-util/e-url.h"
#include <libecal/e-cal.h>
#include "calendar-offline-handler.h"
#include "common/authentication.h"

#define PARENT_TYPE bonobo_object_get_type ()
static BonoboObjectClass *parent_class = NULL;

struct _CalendarOfflineHandlerPrivate {
	ECal *client;

	GNOME_Evolution_OfflineProgressListener listener_interface;

	gboolean is_offline;	
};

static void
add_connection (gpointer data, gpointer user_data)
{
	EUri *uri = e_uri_new (data);	
	GNOME_Evolution_ConnectionList *list = user_data;

	g_return_if_fail (uri != NULL);
	
	if (uri->host != NULL)
		list->_buffer[list->_length].hostName = CORBA_string_dup (uri->host);
	else
		list->_buffer[list->_length].hostName = CORBA_string_dup ("Unknown");
	if (uri->protocol != NULL)
		list->_buffer[list->_length].type = CORBA_string_dup (uri->protocol);
	else
		list->_buffer[list->_length].type = CORBA_string_dup ("Unknown");
	list->_length++;

	e_uri_free (uri);
}

static GNOME_Evolution_ConnectionList *
create_connection_list (CalendarOfflineHandler *offline_handler)
{
	CalendarOfflineHandlerPrivate *priv;
	GNOME_Evolution_ConnectionList *list;
	GList *uris;

	priv = offline_handler->priv;

 	uris = e_cal_uri_list (priv->client, CAL_MODE_REMOTE);	

	list = GNOME_Evolution_ConnectionList__alloc ();
	list->_length = 0;
	list->_maximum = g_list_length (uris);
	list->_buffer = CORBA_sequence_GNOME_Evolution_Connection_allocbuf (list->_maximum);

	g_list_foreach (uris, add_connection, list);

	return list;
}

/* GNOME::Evolution::Offline methods.  */
static CORBA_boolean
impl__get_isOffline (PortableServer_Servant servant,
		     CORBA_Environment *ev)
{
	CalendarOfflineHandler *offline_handler;
	CalendarOfflineHandlerPrivate *priv;
	
	offline_handler = CALENDAR_OFFLINE_HANDLER (bonobo_object_from_servant (servant));
	priv = offline_handler->priv;

	return priv->is_offline;
}

static void
impl_prepareForOffline (PortableServer_Servant servant,
			GNOME_Evolution_ConnectionList **active_connection_list,
			CORBA_Environment *ev)
{
	CalendarOfflineHandler *offline_handler;
	CalendarOfflineHandlerPrivate *priv;
	
	offline_handler = CALENDAR_OFFLINE_HANDLER (bonobo_object_from_servant (servant));
	priv = offline_handler->priv;

	*active_connection_list = create_connection_list (offline_handler);
}

static void
update_offline (CalendarOfflineHandler *offline_handler)
{
	CalendarOfflineHandlerPrivate *priv;
	GNOME_Evolution_ConnectionList *connection_list;
	CORBA_Environment ev;

	priv = offline_handler->priv;

	connection_list = create_connection_list (offline_handler);

	CORBA_exception_init (&ev);

	GNOME_Evolution_OfflineProgressListener_updateProgress (priv->listener_interface, 
								connection_list, &ev);

	if (BONOBO_EX (&ev))
		g_warning ("Error updating offline progress");

	CORBA_exception_free (&ev);
}

static void
backend_cal_set_mode (ECal *client, ECalSetModeStatus status, CalMode mode, gpointer data)
{
	CalendarOfflineHandler *offline_handler = data;

	update_offline (offline_handler);
	g_object_unref (client);
}

static void
backend_cal_opened_offline (ECal *client, ECalendarStatus status, gpointer data)
{
	CalendarOfflineHandler *offline_handler = data;

	if (status != E_CALENDAR_STATUS_OK) {
		update_offline (offline_handler);
		g_object_unref (client);
		return;
	}

	g_signal_connect (client, "cal_set_mode", G_CALLBACK (backend_cal_set_mode), offline_handler);
	e_cal_set_mode (client, CAL_MODE_LOCAL);
}

static void
backend_cal_opened_online (ECal *client, ECalendarStatus status, gpointer data)
{
	if (status != E_CALENDAR_STATUS_OK) {
		g_object_unref (client);
		return;
	}

	e_cal_set_mode (client, CAL_MODE_REMOTE);
	g_object_unref (client);
}

static void
backend_go_offline (gpointer data, gpointer user_data)
{
	CalendarOfflineHandler *offline_handler = user_data;
	char *uri = data;
	ECal *client;
	gboolean success;
	GError *error = NULL;

	/* FIXME This should not use LAST */
	client = auth_new_cal_from_uri (uri, E_CAL_SOURCE_TYPE_LAST);
	g_signal_connect (client, "cal_opened", G_CALLBACK (backend_cal_opened_offline), offline_handler);
	success = e_cal_open (client, TRUE, &error);
	if (!success) {
		g_warning (_("backend_go_offline(): %s"), error->message);
		update_offline (offline_handler);
		g_object_unref (client);
		g_error_free (error);
		return;		
	}
}

static void
backend_go_online (gpointer data, gpointer user_data)
{
	CalendarOfflineHandler *offline_handler = user_data;
	char *uri = data;
	ECal *client;
	gboolean success;
	GError *error = NULL;

	/* FIXME This should not use LAST */	
	client = auth_new_cal_from_uri (uri, E_CAL_SOURCE_TYPE_LAST);
	g_signal_connect (G_OBJECT (client), "cal_opened", 
			  G_CALLBACK (backend_cal_opened_online), offline_handler);
	success = e_cal_open (client, TRUE, &error);
	if (!success) {
		g_warning (_("backend_go_online(): %s"), error->message);
		g_object_unref (client);
		g_error_free (error);
		return;		
	}	
}

static void
impl_goOffline (PortableServer_Servant servant,
		const GNOME_Evolution_OfflineProgressListener progress_listener,
		CORBA_Environment *ev)
{
	CalendarOfflineHandler *offline_handler;
	CalendarOfflineHandlerPrivate *priv;
	GList *uris;
	
	offline_handler = CALENDAR_OFFLINE_HANDLER (bonobo_object_from_servant (servant));
	priv = offline_handler->priv;

	/* To update the status */
	priv->listener_interface = CORBA_Object_duplicate (progress_listener, ev);

	uris = e_cal_uri_list (priv->client, CAL_MODE_REMOTE);

	g_list_foreach (uris, backend_go_offline, offline_handler);	
}

static void
impl_goOnline (PortableServer_Servant servant,
	       CORBA_Environment *ev)
{
	CalendarOfflineHandler *offline_handler;
	CalendarOfflineHandlerPrivate *priv;
	GList *uris;

	offline_handler = CALENDAR_OFFLINE_HANDLER (bonobo_object_from_servant (servant));
	priv = offline_handler->priv;

	uris = e_cal_uri_list (priv->client, CAL_MODE_LOCAL);

	g_list_foreach (uris, backend_go_online, offline_handler);
}

/* GObject methods.  */

static void
impl_dispose (GObject *object)
{
	CalendarOfflineHandler *offline_handler;
	CalendarOfflineHandlerPrivate *priv;

	offline_handler = CALENDAR_OFFLINE_HANDLER (object);
	priv = offline_handler->priv;

	if (priv->client) {
		g_object_unref (priv->client);
		priv->client = NULL;
	}
	
	if (priv->listener_interface != CORBA_OBJECT_NIL) {
		CORBA_Environment ev;

		CORBA_exception_init (&ev);
		CORBA_Object_release (priv->listener_interface, &ev);
		CORBA_exception_free (&ev);

		priv->listener_interface = CORBA_OBJECT_NIL;
	}

	if (G_OBJECT_CLASS (parent_class)->dispose)
		(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
impl_finalize (GObject *object)
{
	CalendarOfflineHandler *offline_handler;
	CalendarOfflineHandlerPrivate *priv;

	offline_handler = CALENDAR_OFFLINE_HANDLER (object);
	priv = offline_handler->priv;

	g_free (priv);

	if (G_OBJECT_CLASS (parent_class)->finalize)
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

/* GTK+ type initialization.  */

static void
calendar_offline_handler_class_init (CalendarOfflineHandlerClass *klass)
{
	GObjectClass *object_class;
	POA_GNOME_Evolution_Offline__epv *epv;

	object_class = G_OBJECT_CLASS (klass);
	object_class->dispose = impl_dispose;
	object_class->finalize = impl_finalize;

	epv = & klass->epv;
	epv->_get_isOffline    = impl__get_isOffline;
	epv->prepareForOffline = impl_prepareForOffline;
	epv->goOffline         = impl_goOffline;
	epv->goOnline          = impl_goOnline;

	parent_class = gtk_type_class (PARENT_TYPE);
}

static void
calendar_offline_handler_init (CalendarOfflineHandler *offline_handler)
{
	CalendarOfflineHandlerPrivate *priv;

	priv = g_new (CalendarOfflineHandlerPrivate, 1);
	offline_handler->priv = priv;

	/* FIXME This should not use LAST */
	/* FIXME: what URI to use? */
	priv->client = auth_new_cal_from_uri ("", E_CAL_SOURCE_TYPE_LAST);
	priv->listener_interface = CORBA_OBJECT_NIL;
	priv->is_offline = FALSE;
}

CalendarOfflineHandler *
calendar_offline_handler_new (void)
{
	CalendarOfflineHandler *new;

	new = g_object_new (calendar_offline_handler_get_type (), NULL);
	
	return new;
}

BONOBO_TYPE_FUNC_FULL (CalendarOfflineHandler, GNOME_Evolution_Offline, PARENT_TYPE, calendar_offline_handler);
