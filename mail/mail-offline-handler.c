/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* mail-offline-handler.c
 *
 * Copyright (C) 2001  Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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

#include "mail-offline-handler.h"
#include "mail.h"
#include "mail-ops.h"

#include <gtk/gtkmain.h>

#include <gal/util/e-util.h>

#define PARENT_TYPE bonobo_x_object_get_type ()
static BonoboXObjectClass *parent_class = NULL;

struct _MailOfflineHandlerPrivate {
	GNOME_Evolution_OfflineProgressListener listener_interface;
};

static void
add_connection (gpointer key, gpointer data, gpointer user_data)
{
	CamelService *service = key;
	GNOME_Evolution_ConnectionList *list = user_data;

	if (!(service->provider->flags & CAMEL_PROVIDER_IS_REMOTE) ||
	    !service->connected)
		return;

	if (CAMEL_IS_DISCO_STORE (service) &&
	    camel_disco_store_status (CAMEL_DISCO_STORE (service)) == CAMEL_DISCO_STORE_OFFLINE)
		return;

	list->_buffer[list->_length].hostName = CORBA_string_dup (service->url->host);
	list->_buffer[list->_length].type     = CORBA_string_dup (service->provider->name);
	list->_length++;
}

static GNOME_Evolution_ConnectionList *
create_connection_list (void)
{
	GNOME_Evolution_ConnectionList *list;

	list = GNOME_Evolution_ConnectionList__alloc ();
	list->_length = 0;
	list->_maximum = mail_storages_count ();
	list->_buffer = CORBA_sequence_GNOME_Evolution_Connection_allocbuf (list->_maximum);

	mail_storages_foreach (add_connection, list);

	return list;
}

/* GNOME::Evolution::Offline methods.  */

static CORBA_boolean
impl__get_isOffline (PortableServer_Servant servant,
		     CORBA_Environment *ev)
{
	return !camel_session_is_online (session);
}

static void
impl_prepareForOffline (PortableServer_Servant servant,
			GNOME_Evolution_ConnectionList **active_connection_list,
			CORBA_Environment *ev)
{
	MailOfflineHandler *offline_handler;
	MailOfflineHandlerPrivate *priv;

	offline_handler = MAIL_OFFLINE_HANDLER (bonobo_object_from_servant (servant));
	priv = offline_handler->priv;

	*active_connection_list = create_connection_list ();
}

static void
went_offline (CamelStore *store, void *data)
{
	MailOfflineHandler *offline_handler = data;
	MailOfflineHandlerPrivate *priv;
	CORBA_Environment ev;
	GNOME_Evolution_ConnectionList *connection_list;

	priv = offline_handler->priv;

	connection_list = create_connection_list ();

	CORBA_exception_init (&ev);

	GNOME_Evolution_OfflineProgressListener_updateProgress (priv->listener_interface, connection_list, &ev);
	if (ev._major != CORBA_NO_EXCEPTION)
		g_warning ("Error updating offline progress");

	CORBA_exception_free (&ev);

	/* CORBA_free (connection_list); */
}

static void
storage_go_offline (gpointer key, gpointer value, gpointer data)
{
	CamelStore *store = key;
	MailOfflineHandler *offline_handler = data;

	mail_store_set_offline (store, TRUE, went_offline, offline_handler);
}

static void
impl_goOffline (PortableServer_Servant servant,
		const GNOME_Evolution_OfflineProgressListener progress_listener,
		CORBA_Environment *ev)
{
	MailOfflineHandler *offline_handler;
	MailOfflineHandlerPrivate *priv;

	offline_handler = MAIL_OFFLINE_HANDLER (bonobo_object_from_servant (servant));
	priv = offline_handler->priv;

	priv->listener_interface = CORBA_Object_duplicate (progress_listener, ev);

	/* This will disable further auto-mail-check action. */
	camel_session_set_online (session, FALSE);

	/* FIXME: If send/receive active, wait for it to finish */

	mail_storages_foreach (storage_go_offline, offline_handler);
}

static void
storage_go_online (gpointer key, gpointer value, gpointer data)
{
	CamelStore *store = key;

	mail_store_set_offline (store, FALSE, NULL, NULL);
}

static void
impl_goOnline (PortableServer_Servant servant,
	       CORBA_Environment *ev)
{
	MailOfflineHandler *offline_handler;
	MailOfflineHandlerPrivate *priv;

	offline_handler = MAIL_OFFLINE_HANDLER (bonobo_object_from_servant (servant));
	priv = offline_handler->priv;

	mail_storages_foreach (storage_go_online, NULL);
}

/* GtkObject methods.  */

static void
impl_destroy (GtkObject *object)
{
	MailOfflineHandler *offline_handler;
	MailOfflineHandlerPrivate *priv;

	offline_handler = MAIL_OFFLINE_HANDLER (object);
	priv = offline_handler->priv;

	if (priv->listener_interface != CORBA_OBJECT_NIL) {
		CORBA_Environment ev;

		CORBA_exception_init (&ev);
		CORBA_Object_release (priv->listener_interface, &ev);
		CORBA_exception_free (&ev);
	}

	g_free (priv);

	if (GTK_OBJECT_CLASS (parent_class)->destroy != NULL)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

/* GTK+ type initialization.  */

static void
mail_offline_handler_class_init (MailOfflineHandlerClass *klass)
{
	GtkObjectClass *object_class;
	POA_GNOME_Evolution_Offline__epv *epv;

	object_class = GTK_OBJECT_CLASS (klass);
	object_class->destroy = impl_destroy;

	epv = & klass->epv;
	epv->_get_isOffline    = impl__get_isOffline;
	epv->prepareForOffline = impl_prepareForOffline;
	epv->goOffline         = impl_goOffline;
	epv->goOnline          = impl_goOnline;

	parent_class = gtk_type_class (PARENT_TYPE);
}

static void
mail_offline_handler_init (MailOfflineHandler *offline_handler)
{
	MailOfflineHandlerPrivate *priv;

	priv = g_new (MailOfflineHandlerPrivate, 1);
	priv->listener_interface = CORBA_OBJECT_NIL;

	offline_handler->priv = priv;
}

MailOfflineHandler *
mail_offline_handler_new (void)
{
	MailOfflineHandler *new;

	new = gtk_type_new (mail_offline_handler_get_type ());

	return new;
}

BONOBO_X_TYPE_FUNC_FULL (MailOfflineHandler, GNOME_Evolution_Offline, PARENT_TYPE, mail_offline_handler);
