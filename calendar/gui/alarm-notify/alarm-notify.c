/* Evolution calendar - Alarm notification service object
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Author: Federico Mena-Quintero <federico@ximian.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <libgnomevfs/gnome-vfs.h>
#include <cal-client/cal-client.h>
#include "alarm-notify.h"
#include "alarm-queue.h"



/* A loaded client */
typedef struct {
	/* The actual client */
	CalClient *client;

	/* The URI of the client in gnome-vfs's format.  This *is* the key that
	 * is stored in the uri_client_hash hash table below.
	 */
	GnomeVFSURI *uri;

	/* Number of times clients have requested this URI to be added to the
	 * alarm notification system.
	 */
	int refcount;
} LoadedClient;

/* Private part of the AlarmNotify structure */
struct _AlarmNotifyPrivate {
	/* Mapping from GnomeVFSURIs to LoadedClient structures */
	GHashTable *uri_client_hash;
};



static void alarm_notify_class_init (AlarmNotifyClass *class);
static void alarm_notify_init (AlarmNotify *an);
static void alarm_notify_destroy (GtkObject *object);

static void AlarmNotify_addCalendar (PortableServer_Servant servant,
				     const CORBA_char *str_uri,
				     CORBA_Environment *ev);
static void AlarmNotify_removeCalendar (PortableServer_Servant servant,
					const CORBA_char *str_uri,
					CORBA_Environment *ev);
static void AlarmNotify_die (PortableServer_Servant servant,
			     CORBA_Environment *ev);


static BonoboXObjectClass *parent_class;



BONOBO_X_TYPE_FUNC_FULL (AlarmNotify,
			 GNOME_Evolution_Calendar_AlarmNotify,
			 BONOBO_X_OBJECT_TYPE,
			 alarm_notify);

/* Class initialization function for the alarm notify service */
static void
alarm_notify_class_init (AlarmNotifyClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;

	parent_class = gtk_type_class (BONOBO_X_OBJECT_TYPE);

	class->epv.addCalendar = AlarmNotify_addCalendar;
	class->epv.removeCalendar = AlarmNotify_removeCalendar;
	class->epv.die = AlarmNotify_die;

	object_class->destroy = alarm_notify_destroy;
}

/* Object initialization function for the alarm notify system */
static void
alarm_notify_init (AlarmNotify *an)
{
	AlarmNotifyPrivate *priv;

	priv = g_new0 (AlarmNotifyPrivate, 1);
	an->priv = priv;

	priv->uri_client_hash = g_hash_table_new (gnome_vfs_uri_hash, gnome_vfs_uri_hequal);
}

/* Callback used from g_hash-table_forach(), used to destroy a loade client */
static void
destroy_loaded_client_cb (gpointer key, gpointer value, gpointer data)
{
	LoadedClient *lc;

	lc = value;

	gtk_object_unref (GTK_OBJECT (lc->client));
	gnome_vfs_uri_unref (lc->uri);
	g_free (lc);
}

/* Destroy handler for the alarm notify system */
static void
alarm_notify_destroy (GtkObject *object)
{
	AlarmNotify *an;
	AlarmNotifyPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_ALARM_NOTIFY (object));

	an = ALARM_NOTIFY (object);
	priv = an->priv;

	g_hash_table_foreach (priv->uri_client_hash, destroy_loaded_client_cb, NULL);

	g_hash_table_destroy (priv->uri_client_hash);
	priv->uri_client_hash = NULL;

	g_free (priv);
	an->priv = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}



/* CORBA servant implementation */

/* AlarmNotify::addCalendar method */
static void
AlarmNotify_addCalendar (PortableServer_Servant servant,
			 const CORBA_char *str_uri,
			 CORBA_Environment *ev)
{
	AlarmNotify *an;
	AlarmNotifyPrivate *priv;
	GnomeVFSURI *uri;
	CalClient *client;
	LoadedClient *lc;

	an = ALARM_NOTIFY (bonobo_object_from_servant (servant));
	priv = an->priv;

	uri = gnome_vfs_uri_new (str_uri);
	if (!uri) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_GNOME_Evolution_Calendar_AlarmNotify_InvalidURI,
				     NULL);
		return;
	}

	lc = g_hash_table_lookup (priv->uri_client_hash, uri);

	if (lc) {
		gnome_vfs_uri_unref (uri);
		g_assert (lc->refcount > 0);
		lc->refcount++;
		return;
	}

	client = cal_client_new ();

	if (client) {
		if (cal_client_open_calendar (client, str_uri, FALSE)) {
			lc = g_new (LoadedClient, 1);
			lc->client = client;
			lc->uri = uri;
			lc->refcount = 1;
			g_hash_table_insert (priv->uri_client_hash, uri, lc);

			alarm_queue_add_client (client);
		} else {
			gtk_object_unref (GTK_OBJECT (client));
			client = NULL;
		}
	}

	if (!client) {
		gnome_vfs_uri_unref (uri);

		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_GNOME_Evolution_Calendar_AlarmNotify_BackendContactError,
				     NULL);
		return;
	}
}

/* AlarmNotify::removeCalendar method */
static void
AlarmNotify_removeCalendar (PortableServer_Servant servant,
			    const CORBA_char *str_uri,
			    CORBA_Environment *ev)
{
	AlarmNotify *an;
	AlarmNotifyPrivate *priv;
	LoadedClient *lc;
	GnomeVFSURI *uri;

	an = ALARM_NOTIFY (bonobo_object_from_servant (servant));
	priv = an->priv;

	uri = gnome_vfs_uri_new (str_uri);
	if (!uri) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_GNOME_Evolution_Calendar_AlarmNotify_InvalidURI,
				     NULL);
		return;
	}

	lc = g_hash_table_lookup (priv->uri_client_hash, uri);
	gnome_vfs_uri_unref (uri);

	if (!lc) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_GNOME_Evolution_Calendar_AlarmNotify_NotFound,
				     NULL);
		return;
	}

	g_assert (lc->refcount > 0);

	lc->refcount--;
	if (lc->refcount > 0)
		return;

	g_hash_table_remove (priv->uri_client_hash, lc->uri);

	gtk_object_unref (GTK_OBJECT (lc->client));
	gnome_vfs_uri_unref (lc->uri);
	g_free (lc);
}

static void
AlarmNotify_die (PortableServer_Servant servant,
		 CORBA_Environment *ev)
{
	AlarmNotify *an;
	AlarmNotifyPrivate *priv;

	an = ALARM_NOTIFY (bonobo_object_from_servant (servant));
	priv = an->priv;

	/* FIXME */
}



/**
 * alarm_notify_new:
 * 
 * Creates a new #AlarmNotify object.
 * 
 * Return value: A newly-created #AlarmNotify, or NULL if its corresponding
 * CORBA object could not be created.
 **/
AlarmNotify *
alarm_notify_new (void)
{
	AlarmNotify *an;

	an = gtk_type_new (TYPE_ALARM_NOTIFY);
	return an;
}
