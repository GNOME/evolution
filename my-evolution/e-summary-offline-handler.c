/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-summary-offline-handler.c
 *
 * Copyright (C) 2001 Ximian, Inc.
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
 * Ettore Perazzoli <ettore@ximian.com>
 * Dan Winship <danw@ximian.com>
 * Iain Holmes  <iain@ximian.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-summary-offline-handler.h"
#include "e-summary.h"

#include <bonobo/bonobo-exception.h>
#include <gtk/gtkmain.h>
#include <gal/util/e-util.h>

#define PARENT_TYPE bonobo_x_object_get_type ()
static BonoboXObjectClass *parent_class = NULL;

struct _ESummaryOfflineHandlerPriv {
	GSList *summaries;
	GNOME_Evolution_OfflineProgressListener listener_interface;
};

/* ::destroy handler for the bookkeeping of the list of summary views.  */

static void
summary_destroy_callback (GtkObject *object,
			  void *data)
{
	ESummaryOfflineHandler *offline_handler;
	ESummaryOfflineHandlerPriv *priv;
	ESummary *summary;

	summary = E_SUMMARY (object);
	offline_handler = E_SUMMARY_OFFLINE_HANDLER (data);
	priv = offline_handler->priv;

	priv->summaries = g_slist_remove (priv->summaries, summary);
}

static GNOME_Evolution_ConnectionList *
create_connection_list (GSList  *summaries)
{
	GNOME_Evolution_ConnectionList *list;
	GList *connections, *p;
	GSList *sp;

	list = GNOME_Evolution_ConnectionList__alloc ();
	list->_length = 0;
	list->_maximum = 0;

	/* Count the total number of connections first to make CORBA happy.  */
	for (sp = summaries; sp != NULL; sp = sp->next)
		list->_maximum += e_summary_count_connections (E_SUMMARY (sp->data));

	list->_buffer = CORBA_sequence_GNOME_Evolution_Connection_allocbuf (list->_maximum);

	for (sp = summaries; sp != NULL; sp = sp->next) {
		ESummary *summary;

		summary = E_SUMMARY (sp->data);
		connections = e_summary_add_connections (summary);
		for (p = connections; p != NULL; p = p->next) {
			ESummaryConnectionData *data;

			data = p->data;
			list->_buffer[list->_length].hostName = CORBA_string_dup (data->hostname);
			list->_buffer[list->_length].type = CORBA_string_dup (data->type);
			list->_length++;

			g_free (data->hostname);
			g_free (data->type);
			g_free (data);
		}
		g_list_free (connections);
	}
	
	return list;
}

/* GNOME::Evolution::Offline methods. */
static CORBA_boolean
impl__get_isOffline (PortableServer_Servant servant,
		     CORBA_Environment *ev)
{
	ESummaryOfflineHandler *offline_handler;
	GSList *sp;

	offline_handler = E_SUMMARY_OFFLINE_HANDLER (bonobo_object_from_servant (servant));

	/* We are offline if all of the views are offline.  */
	for (sp = offline_handler->priv->summaries; sp != NULL; sp = sp->next) {
		ESummary *summary;

		summary = E_SUMMARY (sp->data);
		if (summary->online)
			return FALSE;
	}

	return TRUE;
}

static void
impl_prepareForOffline (PortableServer_Servant servant,
			GNOME_Evolution_ConnectionList **active_connection_list,
			CORBA_Environment *ev)
{
	ESummaryOfflineHandler *offline_handler;
	ESummaryOfflineHandlerPriv *priv;

	offline_handler = E_SUMMARY_OFFLINE_HANDLER (bonobo_object_from_servant (servant));
	priv = offline_handler->priv;

	*active_connection_list = create_connection_list (priv->summaries);
}

static void
went_offline (ESummary *summary,
	      void *data)
{
	ESummaryOfflineHandler *offline_handler = data;
	ESummaryOfflineHandlerPriv *priv;
	CORBA_Environment ev;
	GNOME_Evolution_ConnectionList *connection_list;

	g_return_if_fail (summary != NULL);
	g_return_if_fail (IS_E_SUMMARY (summary));
	g_return_if_fail (offline_handler != NULL);

	priv = offline_handler->priv;
	connection_list = create_connection_list (priv->summaries);

	CORBA_exception_init (&ev);

	g_warning ("Went offline");
  	GNOME_Evolution_OfflineProgressListener_updateProgress (priv->listener_interface, connection_list, &ev); 
  	if (BONOBO_EX (&ev)) { 
  		g_warning ("Error updating offline progress: %s", 
  			   CORBA_exception_id (&ev)); 
  	} 

  	CORBA_exception_free (&ev);
}

static void
impl_goOffline (PortableServer_Servant servant,
		const GNOME_Evolution_OfflineProgressListener progress_listener,
		CORBA_Environment *ev)
{
	ESummaryOfflineHandler *offline_handler;
	ESummaryOfflineHandlerPriv *priv;
	GSList *sp;

	offline_handler = E_SUMMARY_OFFLINE_HANDLER (bonobo_object_from_servant (servant));
	priv = offline_handler->priv;

	/* FIXME: If we have a progress already, then something is wrong and we
	   should raise an exception.  */
	priv->listener_interface = CORBA_Object_duplicate (progress_listener, ev);

	for (sp = priv->summaries; sp != NULL; sp = sp->next) {
		ESummary *summary;

		summary = E_SUMMARY (sp->data);
		e_summary_set_online (summary, CORBA_OBJECT_NIL, FALSE, went_offline, offline_handler);
	}
}

static void
impl_goOnline (PortableServer_Servant servant,
	       CORBA_Environment *ev)
{
	ESummaryOfflineHandler *offline_handler;
	GSList *sp;

	offline_handler = E_SUMMARY_OFFLINE_HANDLER (bonobo_object_from_servant (servant));

	for (sp = offline_handler->priv->summaries; sp != NULL; sp = sp->next) {
		ESummary *summary;

		summary = E_SUMMARY (sp->data);
		e_summary_set_online (summary, CORBA_OBJECT_NIL, TRUE, NULL, NULL);
	}
}

/* GtkObject methods */
static void
impl_destroy (GtkObject *object)
{
	ESummaryOfflineHandler *offline_handler;
	ESummaryOfflineHandlerPriv *priv;

	offline_handler = E_SUMMARY_OFFLINE_HANDLER (object);
	priv = offline_handler->priv;

	if (priv == NULL) {
		return;
	}

	if (priv->listener_interface != CORBA_OBJECT_NIL) {
		CORBA_Environment ev;

		CORBA_exception_init (&ev);
		CORBA_Object_release (priv->listener_interface, &ev);
		CORBA_exception_free (&ev);
	}

	g_slist_free (priv->summaries);

	offline_handler->priv = NULL;
	g_free (priv);

	if (GTK_OBJECT_CLASS (parent_class)->destroy != NULL) {
		GTK_OBJECT_CLASS (parent_class)->destroy (object);
	}
}

static void
e_summary_offline_handler_class_init (ESummaryOfflineHandlerClass *klass)
{
	GtkObjectClass *object_class;
	POA_GNOME_Evolution_Offline__epv *epv;

	object_class = GTK_OBJECT_CLASS (klass);
	object_class->destroy = impl_destroy;

	epv = &klass->epv;
	epv->_get_isOffline = impl__get_isOffline;
	epv->prepareForOffline = impl_prepareForOffline;
	epv->goOffline = impl_goOffline;
	epv->goOnline = impl_goOnline;

	parent_class = gtk_type_class (PARENT_TYPE);
}

static void
e_summary_offline_handler_init (ESummaryOfflineHandler *offline_handler)
{
	ESummaryOfflineHandlerPriv *priv;

	priv = g_new0 (ESummaryOfflineHandlerPriv, 1);

	offline_handler->priv = priv;
}

ESummaryOfflineHandler *
e_summary_offline_handler_new (void)
{
	ESummaryOfflineHandler *new;

	new = gtk_type_new (e_summary_offline_handler_get_type ());

	return new;
}

void
e_summary_offline_handler_add_summary (ESummaryOfflineHandler *handler,
				       ESummary *summary)
{
	g_return_if_fail (handler != NULL);
	g_return_if_fail (summary != NULL);
	g_return_if_fail (IS_E_SUMMARY (summary));

	handler->priv->summaries = g_slist_prepend (handler->priv->summaries,
						    summary);

	gtk_signal_connect (GTK_OBJECT (summary), "destroy",
			    GTK_SIGNAL_FUNC (summary_destroy_callback), handler);
}

BONOBO_X_TYPE_FUNC_FULL (ESummaryOfflineHandler, GNOME_Evolution_Offline, PARENT_TYPE, e_summary_offline_handler);
