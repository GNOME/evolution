/* Evolution calendar - Live query client object
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Author: Federico Mena-Quintero <federico@ximian.com>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <bonobo/bonobo-exception.h>
#include "cal-marshal.h"
#include "cal-client.h"
#include "cal-query.h"
#include "query-listener.h"



/* Private part of the CalQuery structure */
struct _CalQueryPrivate {
	/* Handle to the query in the server */
	GNOME_Evolution_Calendar_Query query;

	/* Our query listener implementation */
	QueryListener *listener;

	/* The CalClient associated with this query */
	CalClient *client;
};

/* Property IDs */
enum props {
	PROP_0,
	PROP_QUERY,
	PROP_LISTENER,
	PROP_CLIENT
};

/* Signal IDs */
enum {
	OBJECTS_ADDED,
	OBJECTS_MODIFIED,
	OBJECTS_REMOVED,
	QUERY_PROGRESS,
	QUERY_DONE,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static GObjectClass *parent_class;



static void
objects_added_cb (QueryListener *listener, GList *objects, gpointer data)
{
	CalQuery *query;

	query = CAL_QUERY (data);

	g_signal_emit (G_OBJECT (query), signals[OBJECTS_ADDED], 0, objects);
}

static void
objects_modified_cb (QueryListener *listener, GList *objects, gpointer data)
{
	CalQuery *query;

	query = CAL_QUERY (data);

	g_signal_emit (G_OBJECT (query), signals[OBJECTS_MODIFIED], 0, objects);
}

static void
objects_removed_cb (QueryListener *listener, GList *uids, gpointer data)
{
	CalQuery *query;

	query = CAL_QUERY (data);

	g_signal_emit (G_OBJECT (query), signals[OBJECTS_REMOVED], 0, uids);
}

static void
query_progress_cb (QueryListener *listener, const char *message, int percent, gpointer data)
{
	CalQuery *query;

	query = CAL_QUERY (data);

	g_signal_emit (G_OBJECT (query), signals[QUERY_PROGRESS], 0, message, percent);
}

static void
query_done_cb (QueryListener *listener, ECalendarStatus status, gpointer data)
{
	CalQuery *query;

	query = CAL_QUERY (data);

	g_signal_emit (G_OBJECT (query), signals[QUERY_DONE], 0, status);
}

/* Object initialization function for the calendar query */
static void
cal_query_init (CalQuery *query, CalQueryClass *klass)
{
	CalQueryPrivate *priv;

	priv = g_new0 (CalQueryPrivate, 1);
	query->priv = priv;

	priv->listener = NULL;
	priv->query = CORBA_OBJECT_NIL;
}

static void
cal_query_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	CalQuery *query;
	CalQueryPrivate *priv;
	
	query = CAL_QUERY (object);
	priv = query->priv;
	
	switch (property_id) {
	case PROP_QUERY:
		priv->query = bonobo_object_dup_ref (g_value_get_pointer (value), NULL);
		break;
	case PROP_LISTENER:
		priv->listener = bonobo_object_ref (g_value_get_pointer (value));

		g_signal_connect (G_OBJECT (priv->listener), "objects_added", 
				  G_CALLBACK (objects_added_cb), query);
		g_signal_connect (G_OBJECT (priv->listener), "objects_modified", 
				  G_CALLBACK (objects_modified_cb), query);
		g_signal_connect (G_OBJECT (priv->listener), "objects_removed", 
				  G_CALLBACK (objects_removed_cb), query);
		g_signal_connect (G_OBJECT (priv->listener), "query_progress", 
				  G_CALLBACK (query_progress_cb), query);
		g_signal_connect (G_OBJECT (priv->listener), "query_done", 
				  G_CALLBACK (query_done_cb), query);
		break;		
	case PROP_CLIENT:
		priv->client = CAL_CLIENT (g_value_dup_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
cal_query_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	CalQuery *query;
	CalQueryPrivate *priv;
	
	query = CAL_QUERY (object);
	priv = query->priv;

	switch (property_id) {
	case PROP_QUERY:
		g_value_set_pointer (value, priv->query);
		break;
	case PROP_LISTENER:
		g_value_set_pointer (value, priv->listener);
		break;
	case PROP_CLIENT:
		g_value_set_object (value, priv->client);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}


/* Finalize handler for the calendar query */
static void
cal_query_finalize (GObject *object)
{
	CalQuery *query;
	CalQueryPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_CAL_QUERY (object));

	query = CAL_QUERY (object);
	priv = query->priv;

	/* The server keeps a copy of the query listener, so we must unref it */
	g_signal_handlers_disconnect_matched (priv->listener, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, query);
	bonobo_object_unref (BONOBO_OBJECT (priv->listener));

	if (priv->query != CORBA_OBJECT_NIL)
		bonobo_object_release_unref (priv->query, NULL);

	g_free (priv);

	if (G_OBJECT_CLASS (parent_class)->finalize)
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

/* Class initialization function for the calendar query */
static void
cal_query_class_init (CalQueryClass *klass)
{
	GObjectClass *object_class;
	GParamSpec *param;
	
	object_class = (GObjectClass *) klass;

	parent_class = g_type_class_peek_parent (klass);

	object_class->set_property = cal_query_set_property;
	object_class->get_property = cal_query_get_property;
	object_class->finalize = cal_query_finalize;

	param =  g_param_spec_pointer ("query", NULL, NULL,
				      G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);
	g_object_class_install_property (object_class, PROP_QUERY, param);
	param =  g_param_spec_pointer ("listener", NULL, NULL,
				      G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);
	g_object_class_install_property (object_class, PROP_LISTENER, param);
	param =  g_param_spec_object ("client", NULL, NULL, CAL_CLIENT_TYPE,
				      G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);
	g_object_class_install_property (object_class, PROP_CLIENT, param);
	
	signals[OBJECTS_ADDED] =
		g_signal_new ("objects_added",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (CalQueryClass, objects_added),
			      NULL, NULL,
			      cal_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);
	signals[OBJECTS_MODIFIED] =
		g_signal_new ("objects_modified",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (CalQueryClass, objects_modified),
			      NULL, NULL,
			      cal_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);
	signals[OBJECTS_REMOVED] =
		g_signal_new ("objects_removed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (CalQueryClass, objects_removed),
			      NULL, NULL,
			      cal_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);
	signals[QUERY_PROGRESS] =
		g_signal_new ("query_progress",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (CalQueryClass, query_progress),
			      NULL, NULL,
			      cal_marshal_VOID__POINTER,
			      G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_INT);
	signals[QUERY_DONE] =
		g_signal_new ("query_done",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (CalQueryClass, query_done),
			      NULL, NULL,
			      cal_marshal_VOID__INT,
			      G_TYPE_NONE, 1, G_TYPE_INT);
}

/**
 * cal_query_get_type:
 * 
 * Registers the #CalQuery class if necessary, and returns the type ID assigned
 * to it.
 * 
 * Return value: The type ID of the #CalQuery class.
 **/
GType
cal_query_get_type (void)
{
	static GType cal_query_type = 0;

	if (!cal_query_type) {
		static GTypeInfo info = {
                        sizeof (CalQueryClass),
                        (GBaseInitFunc) NULL,
                        (GBaseFinalizeFunc) NULL,
                        (GClassInitFunc) cal_query_class_init,
                        NULL, NULL,
                        sizeof (CalQuery),
                        0,
                        (GInstanceInitFunc) cal_query_init
                };
		cal_query_type = g_type_register_static (G_TYPE_OBJECT, "CalQuery", &info, 0);
	}

	return cal_query_type;
}

/**
 * cal_query_new:
 * @client: Client from which the query is being created.
 * @cal: Handle to an open calendar.
 * @sexp: S-expression that defines the query.
 * 
 * Creates a new query object by issuing the query creation request to the
 * calendar server.
 * 
 * Return value: A newly-created query object, or NULL if the request failed.
 **/
CalQuery *
cal_query_new (GNOME_Evolution_Calendar_Query corba_query, QueryListener *listener, CalClient *client)
{
	CalQuery *query;

	query = g_object_new (CAL_QUERY_TYPE, "query", corba_query, "listener", 
			      listener, "client", client, NULL);

	return query;
}

/**
 * cal_query_get_client
 * @query: A #CalQuery object.
 *
 * Get the #CalClient associated with this query.
 *
 * Returns: the associated client.
 */
CalClient *
cal_query_get_client (CalQuery *query)
{
	g_return_val_if_fail (IS_CAL_QUERY (query), NULL);

	return query->priv->client;
}

void
cal_query_start (CalQuery *query)
{
	CalQueryPrivate *priv;
	CORBA_Environment ev;

	g_return_if_fail (query != NULL);
	g_return_if_fail (IS_CAL_QUERY (query));
	
	priv = query->priv;
	
	CORBA_exception_init (&ev);

	GNOME_Evolution_Calendar_Query_start (priv->query, &ev);
	if (BONOBO_EX (&ev)) 
		g_warning (G_STRLOC ": Unable to start query");

	CORBA_exception_free (&ev);
}
