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
#include "cal-util/cal-util-marshal.h"
#include "cal-query.h"
#include "query-listener.h"



/* Private part of the CalQuery structure */
struct _CalQueryPrivate {
	/* Our query listener implementation */
	QueryListener *ql;

	/* Handle to the query in the server */
	GNOME_Evolution_Calendar_Query corba_query;

	/* The CalClient associated with this query */
	CalClient *client;
};



static void cal_query_class_init (CalQueryClass *klass);
static void cal_query_init (CalQuery *query, CalQueryClass *klass);
static void cal_query_finalize (GObject *object);

/* Signal IDs */
enum {
	OBJ_UPDATED,
	OBJ_REMOVED,
	QUERY_DONE,
	EVAL_ERROR,
	LAST_SIGNAL
};

static guint query_signals[LAST_SIGNAL];

static GObjectClass *parent_class;



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

GType
cal_query_done_status_enum_get_type (void)
{
	static GType cal_query_done_status_enum_type = 0;

	if (!cal_query_done_status_enum_type) {
		static GEnumValue values [] = {
		  { CAL_QUERY_DONE_SUCCESS,     "CalQueryDoneSuccess",    "success"     },
		  { CAL_QUERY_DONE_PARSE_ERROR, "CalQueryDoneParseError", "parse-error" },
		  { -1,                         NULL,                     NULL          }
		};

		cal_query_done_status_enum_type =
		  g_enum_register_static ("CalQueryDoneStatusEnum", values);
	}

	return cal_query_done_status_enum_type;
}

/* Class initialization function for the calendar query */
static void
cal_query_class_init (CalQueryClass *klass)
{
	GObjectClass *object_class;

	object_class = (GObjectClass *) klass;

	parent_class = g_type_class_peek_parent (klass);

	query_signals[OBJ_UPDATED] =
		g_signal_new ("obj_updated",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (CalQueryClass, obj_updated),
			      NULL, NULL,
			      cal_util_marshal_VOID__STRING_BOOLEAN_INT_INT,
			      G_TYPE_NONE, 4,
			      G_TYPE_STRING,
			      G_TYPE_BOOLEAN,
			      G_TYPE_INT,
			      G_TYPE_INT);
	query_signals[OBJ_REMOVED] =
		g_signal_new ("obj_removed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (CalQueryClass, obj_removed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1,
			      G_TYPE_STRING);
	query_signals[QUERY_DONE] =
		g_signal_new ("query_done",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (CalQueryClass, query_done),
			      NULL, NULL,
			      cal_util_marshal_VOID__ENUM_STRING,
			      G_TYPE_NONE, 2,
			      CAL_QUERY_DONE_STATUS_ENUM_TYPE,
			      G_TYPE_STRING);
	query_signals[EVAL_ERROR] =
		g_signal_new ("eval_error",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (CalQueryClass, eval_error),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1,
			      G_TYPE_STRING);

	klass->obj_updated = NULL;
	klass->obj_removed = NULL;
	klass->query_done = NULL;
	klass->eval_error = NULL;

	object_class->finalize = cal_query_finalize;
}

/* Object initialization function for the calendar query */
static void
cal_query_init (CalQuery *query, CalQueryClass *klass)
{
	CalQueryPrivate *priv;

	priv = g_new0 (CalQueryPrivate, 1);
	query->priv = priv;

	priv->ql = NULL;
	priv->corba_query = CORBA_OBJECT_NIL;
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
	query_listener_stop_notification (priv->ql);
	bonobo_object_unref (BONOBO_OBJECT (priv->ql));
	priv->ql = NULL;

	if (priv->corba_query != CORBA_OBJECT_NIL) {
		CORBA_Environment ev;

		CORBA_exception_init (&ev);
		bonobo_object_release_unref (priv->corba_query, &ev);

		if (BONOBO_EX (&ev))
			g_message ("cal_query_destroy(): Could not release/unref the query");

		CORBA_exception_free (&ev);
		priv->corba_query = CORBA_OBJECT_NIL;
	}

	g_free (priv);
	query->priv = NULL;

	if (G_OBJECT_CLASS (parent_class)->finalize)
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}



/* Callback used when an object is updated in the query */
static void
obj_updated_cb (QueryListener *ql,
		const GNOME_Evolution_Calendar_CalObjUIDSeq *uids,
		CORBA_boolean query_in_progress,
		CORBA_long n_scanned,
		CORBA_long total,
		gpointer data)
{
	CalQuery *query;
	int n;

	query = CAL_QUERY (data);

	for (n = 0; n < uids->_length; n++) {
		g_signal_emit (G_OBJECT (query), query_signals[OBJ_UPDATED], 0,
			       uids->_buffer[n], query_in_progress,
			       (int) n_scanned, (int) total);
	}
}

/* Callback used when an object is removed from the query */
static void
obj_removed_cb (QueryListener *ql,
		const CORBA_char *uid,
		gpointer data)
{
	CalQuery *query;

	query = CAL_QUERY (data);

	g_signal_emit (G_OBJECT (query), query_signals[OBJ_REMOVED],
		       0, uid);
}

/* Callback used when the query terminates */
static void
query_done_cb (QueryListener *ql,
	       GNOME_Evolution_Calendar_QueryListener_QueryDoneStatus corba_status,
	       const CORBA_char *error_str,
	       gpointer data)
{
	CalQuery *query;
	CalQueryDoneStatus status;

	query = CAL_QUERY (data);

	switch (corba_status) {
	case GNOME_Evolution_Calendar_QueryListener_SUCCESS:
		status = CAL_QUERY_DONE_SUCCESS;
		break;

	case GNOME_Evolution_Calendar_QueryListener_PARSE_ERROR:
		status = CAL_QUERY_DONE_PARSE_ERROR;
		break;

	default:
		g_assert_not_reached ();
		return;
	}

	g_signal_emit (G_OBJECT (query), query_signals[QUERY_DONE], 0,
		       status, error_str);
}

/* Callback used when an error occurs when evaluating the query */
static void
eval_error_cb (QueryListener *ql,
	       const CORBA_char *error_str,
	       gpointer data)
{
	CalQuery *query;

	query = CAL_QUERY (data);

	g_signal_emit (G_OBJECT (query), query_signals[EVAL_ERROR], 0,
		       error_str);
}

/**
 * cal_query_construct:
 * @query: A calendar query.
 * @cal: Handle to an open calendar.
 * @sexp: S-expression that defines the query.
 * 
 * Constructs a query object by issuing the query creation request to the
 * calendar server.
 * 
 * Return value: The same value as @query on success, or NULL if the request
 * failed.
 **/
CalQuery *
cal_query_construct (CalQuery *query,
		     GNOME_Evolution_Calendar_Cal cal,
		     const char *sexp)
{
	CalQueryPrivate *priv;
	GNOME_Evolution_Calendar_QueryListener corba_ql;
	CORBA_Environment ev;

	g_return_val_if_fail (query != NULL, NULL);
	g_return_val_if_fail (IS_CAL_QUERY (query), NULL);
	g_return_val_if_fail (sexp != NULL, NULL);

	priv = query->priv;

	priv->ql = query_listener_new (obj_updated_cb,
				       obj_removed_cb,
				       query_done_cb,
				       eval_error_cb,
				       query);
	if (!priv->ql) {
		g_message ("cal_query_construct(): Could not create the query listener");
		return NULL;
	}

	corba_ql = BONOBO_OBJREF (priv->ql);
				 
	CORBA_exception_init (&ev);
	priv->corba_query = GNOME_Evolution_Calendar_Cal_getQuery (cal, sexp, corba_ql, &ev);

	if (BONOBO_USER_EX (&ev, ex_GNOME_Evolution_Calendar_Cal_CouldNotCreate)) {		
		g_message ("cal_query_construct(): The server could not create the query");
		goto error;
	} else if (BONOBO_EX (&ev)) {
		g_message ("cal_query_construct(): Could not issue the getQuery() request");
		goto error;
	}

	CORBA_exception_free (&ev);

	return query;

 error:

	CORBA_exception_free (&ev);

	bonobo_object_unref (BONOBO_OBJECT (priv->ql));
	priv->ql = NULL;
	priv->corba_query = CORBA_OBJECT_NIL;
	return NULL;
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
cal_query_new (CalClient *client,
	       GNOME_Evolution_Calendar_Cal cal,
	       const char *sexp)
{
	CalQuery *query;

	query = g_object_new (CAL_QUERY_TYPE, NULL);

	if (!cal_query_construct (query, cal, sexp)) {
		g_object_unref (G_OBJECT (query));
		return NULL;
	}

	query->priv->client = client;

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
