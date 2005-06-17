/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* mail-offline-handler.c
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

#include "mail-offline-handler.h"
#include "mail-component.h"
#include "mail-ops.h"
#include "mail-folder-cache.h"
#include "em-folder-tree.h"

#include <camel/camel-disco-store.h>
#include <camel/camel-offline-store.h>
#include "mail-session.h"

#include <gtk/gtkmain.h>

#include <e-util/e-util.h>


#define PARENT_TYPE bonobo_object_get_type ()
static BonoboObjectClass *parent_class = NULL;

struct _MailOfflineHandlerPrivate {
	GHashTable *sync_table;
};

static gboolean
service_is_relevant (CamelService *service, gboolean going_offline)
{
	if (!(service->provider->flags & CAMEL_PROVIDER_IS_REMOTE))
		return FALSE;

	if (CAMEL_IS_DISCO_STORE (service) &&
	    camel_disco_store_status (CAMEL_DISCO_STORE (service)) == CAMEL_DISCO_STORE_OFFLINE)
			return !going_offline;
	else if ( CAMEL_IS_OFFLINE_STORE (service) && 
		  CAMEL_OFFLINE_STORE ( service )->state == CAMEL_OFFLINE_STORE_NETWORK_UNAVAIL )
				return !going_offline;
	return service->status != CAMEL_SERVICE_DISCONNECTED;
}

static void
add_connection (gpointer key, gpointer value, gpointer user_data)
{
	CamelService *service = key;
	GNOME_Evolution_ConnectionList *list = user_data;

	if (!service_is_relevant (service, TRUE))
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
	list->_maximum = mail_component_get_store_count (mail_component_peek ());
	list->_buffer = CORBA_sequence_GNOME_Evolution_Connection_allocbuf (list->_maximum);
	
	mail_component_stores_foreach (mail_component_peek (), add_connection, list);
	
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

/* keep track of each sync in progress */
struct _sync_info {
	char *uri;		/* uri of folder being synced */
	CamelOperation *cancel;	/* progress report/cancellation object */
	int pc;			/* percent complete (0-100) */
	int lastpc;		/* last percent reported, so we dont overreport */
	int id;			/* timeout id */
	GHashTable *table;      /* the hashtable that we're registered in */
};

static void
went_offline (CamelStore *store, void *data)
{
	CORBA_Environment ev;
	GNOME_Evolution_ConnectionList *connection_list;
	GNOME_Evolution_OfflineProgressListener listener = data;

	connection_list = create_connection_list ();

	CORBA_exception_init (&ev);
	GNOME_Evolution_OfflineProgressListener_updateProgress(listener, connection_list, &ev);
	if (ev._major != CORBA_NO_EXCEPTION)
		g_warning ("Error updating offline progress");
	CORBA_Object_release(listener, &ev);

	CORBA_exception_free (&ev);
	CORBA_free (connection_list);
}

static void
store_go_offline (gpointer key, gpointer value, gpointer data)
{
	CamelStore *store = key;
	GNOME_Evolution_OfflineProgressListener listener = data;
	CORBA_Environment ev;
	
	CORBA_exception_init(&ev);
	if (service_is_relevant (CAMEL_SERVICE (store), TRUE)) {
		mail_store_set_offline (store, TRUE, went_offline, CORBA_Object_duplicate(listener, &ev));
	}
	CORBA_exception_free(&ev);
}

static void
impl_goOffline (PortableServer_Servant servant,
		const GNOME_Evolution_OfflineProgressListener progress_listener,
		CORBA_Environment *ev)
{
	MailOfflineHandler *offline_handler;

	offline_handler = MAIL_OFFLINE_HANDLER (bonobo_object_from_servant (servant));

	/* This will disable further auto-mail-check action. */
	camel_session_set_online (session, FALSE);

	/* FIXME: If send/receive active, wait for it to finish */

	mail_component_stores_foreach (mail_component_peek (), store_go_offline, progress_listener);
}

static void
store_went_online(CamelStore *store, void *data)
{
	char *name = data;

	em_folder_tree_model_add_store(mail_component_peek_tree_model(mail_component_peek()), store, name);
	mail_note_store (store, NULL, NULL, NULL);
	g_free(name);
}

static void
store_go_online (gpointer key, gpointer value, gpointer data)
{
	CamelStore *store = key;
	char *name = value;

	if (service_is_relevant (CAMEL_SERVICE (store), FALSE)) {
		mail_store_set_offline (store, FALSE, store_went_online, g_strdup(name));
	}
}

static void
impl_goOnline (PortableServer_Servant servant,
	       CORBA_Environment *ev)
{
	MailOfflineHandler *offline_handler;
	MailOfflineHandlerPrivate *priv;
	
	offline_handler = MAIL_OFFLINE_HANDLER (bonobo_object_from_servant (servant));
	priv = offline_handler->priv;
	
	/* Enable auto-mail-checking */
	camel_session_set_online (session, TRUE);
	
	mail_component_stores_foreach (mail_component_peek (), store_go_online, NULL);
}

/* GObject methods.  */

static void
impl_finalise (GObject *object)
{
	MailOfflineHandler *offline_handler;
	MailOfflineHandlerPrivate *priv;

	offline_handler = MAIL_OFFLINE_HANDLER (object);
	priv = offline_handler->priv;
	g_hash_table_destroy(priv->sync_table);
	g_free (priv);

	if (G_OBJECT_CLASS (parent_class)->finalize != NULL)
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

/* GTK+ type initialization.  */

static void
mail_offline_handler_class_init (MailOfflineHandlerClass *klass)
{
	GObjectClass *object_class;
	POA_GNOME_Evolution_Offline__epv *epv;

	object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = impl_finalise;

	epv = & klass->epv;
	epv->_get_isOffline    = impl__get_isOffline;
	epv->prepareForOffline = impl_prepareForOffline;
	epv->goOffline         = impl_goOffline;
	epv->goOnline          = impl_goOnline;

	parent_class = g_type_class_ref(PARENT_TYPE);
}

static void
mail_offline_handler_init (MailOfflineHandler *offline_handler)
{
	MailOfflineHandlerPrivate *priv;

	priv = g_new (MailOfflineHandlerPrivate, 1);
	priv->sync_table = g_hash_table_new(g_str_hash, g_str_equal);

	offline_handler->priv = priv;
}

MailOfflineHandler *
mail_offline_handler_new (void)
{
	MailOfflineHandler *new;

	new = g_object_new(mail_offline_handler_get_type (), NULL);

	return new;
}

BONOBO_TYPE_FUNC_FULL (MailOfflineHandler, GNOME_Evolution_Offline, PARENT_TYPE, mail_offline_handler);
