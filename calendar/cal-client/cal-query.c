/* Evolution calendar - Live query client object
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
#include <config.h>
#endif

#include <gtk/gtksignal.h>
#include "cal-query.h"
#include "query-listener.h"



/* Private part of the CalQuery structure */
struct _CalQueryPrivate {
	/* Our query listener implementation */
	QueryListener *ql;

	/* Handle to the query in the server */
	GNOME_Evolution_Calendar_Query corba_query;
};



static void cal_query_class_init (CalQueryClass *class);
static void cal_query_init (CalQuery *query);
static void cal_query_destroy (GtkObject *object);

/* Signal IDs */
enum {
	OBJ_UPDATED,
	OBJ_REMOVED,
	QUERY_DONE,
	EVAL_ERROR,
	LAST_SIGNAL
};

static void marshal_obj_updated (GtkObject *object,
				 GtkSignalFunc func, gpointer func_data,
				 GtkArg *args);
static void marshal_query_done (GtkObject *object,
				GtkSignalFunc func, gpointer func_data,
				GtkArg *args);

static guint query_signals[LAST_SIGNAL];

static GtkObjectClass *parent_class;



/**
 * cal_query_get_type:
 * 
 * Registers the #CalQuery class if necessary, and returns the type ID assigned
 * to it.
 * 
 * Return value: The type ID of the #CalQuery class.
 **/
GtkType
cal_query_get_type (void)
{
	static GtkType cal_query_type = 0;

	if (!cal_query_type) {
		static const GtkTypeInfo cal_query_info = {
			"CalQuery",
			sizeof (CalQuery),
			sizeof (CalQueryClass),
			(GtkClassInitFunc) cal_query_class_init,
			(GtkObjectInitFunc) cal_query_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		cal_query_type = gtk_type_unique (GTK_TYPE_OBJECT, &cal_query_info);
	}

	return cal_query_type;
}

/* Class initialization function for the calendar query */
static void
cal_query_class_init (CalQueryClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;

	parent_class = gtk_type_class (GTK_TYPE_OBJECT);

	query_signals[OBJ_UPDATED] =
		gtk_signal_new ("obj_updated",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (CalQueryClass, obj_updated),
				marshal_obj_updated,
				GTK_TYPE_NONE, 4,
				GTK_TYPE_STRING,
				GTK_TYPE_BOOL,
				GTK_TYPE_INT,
				GTK_TYPE_INT);
	query_signals[OBJ_REMOVED] =
		gtk_signal_new ("obj_removed",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (CalQueryClass, obj_removed),
				gtk_marshal_NONE__STRING,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_STRING);
	query_signals[QUERY_DONE] =
		gtk_signal_new ("query_done",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (CalQueryClass, query_done),
				marshal_query_done,
				GTK_TYPE_NONE, 2,
				GTK_TYPE_ENUM,
				GTK_TYPE_STRING);
	query_signals[EVAL_ERROR] =
		gtk_signal_new ("eval_error",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (CalQueryClass, eval_error),
				gtk_marshal_NONE__STRING,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_STRING);

	gtk_object_class_add_signals (object_class, query_signals, LAST_SIGNAL);

	class->obj_updated = NULL;
	class->obj_removed = NULL;
	class->query_done = NULL;
	class->eval_error = NULL;

	object_class->destroy = cal_query_destroy;
}

/* Object initialization function for the calendar query */
static void
cal_query_init (CalQuery *query)
{
	CalQueryPrivate *priv;

	priv = g_new0 (CalQueryPrivate, 1);
	query->priv = priv;

	priv->ql = NULL;
	priv->corba_query = CORBA_OBJECT_NIL;
}

/* Destroy handler for the calendar query */
static void
cal_query_destroy (GtkObject *object)
{
	CalQuery *query;
	CalQueryPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_CAL_QUERY (object));

	query = CAL_QUERY (object);
	priv = query->priv;

	/* The server unrefs the query listener, so we just NULL it out here */
	priv->ql = NULL;

	if (priv->corba_query != CORBA_OBJECT_NIL) {
		CORBA_Environment ev;

		CORBA_exception_init (&ev);
		bonobo_object_release_unref (priv->corba_query, &ev);

		if (ev._major != CORBA_NO_EXCEPTION)
			g_message ("cal_query_destroy(): Could not release/unref the query");

		CORBA_exception_free (&ev);
		priv->corba_query = CORBA_OBJECT_NIL;
	}

	g_free (priv);
	query->priv = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}



/* Marshalers */

typedef void (* ObjUpdatedFunc) (QueryListener *ql, const char *uid,
				 gboolean query_in_progress, int n_scanned, int total,
				 gpointer data);

static void
marshal_obj_updated (GtkObject *object, GtkSignalFunc func, gpointer func_data, GtkArg *args)
{
	ObjUpdatedFunc f;

	f = (ObjUpdatedFunc) func;

	(* f) (QUERY_LISTENER (object), GTK_VALUE_STRING (args[0]),
	       GTK_VALUE_BOOL (args[1]), GTK_VALUE_INT (args[2]), GTK_VALUE_INT (args[3]),
	       func_data);
}

typedef void (* QueryDoneFunc) (QueryListener *ql, CalQueryDoneStatus status, const char *error_str,
				gpointer data);

static void
marshal_query_done (GtkObject *object, GtkSignalFunc func, gpointer func_data, GtkArg *args)
{
	QueryDoneFunc f;

	f = (QueryDoneFunc) func;

	(* f) (QUERY_LISTENER (object), GTK_VALUE_ENUM (args[0]), GTK_VALUE_STRING (args[1]),
	       func_data);
}



/* Callback used when an object is updated in the query */
static void
obj_updated_cb (QueryListener *ql,
		const GNOME_Evolution_Calendar_CalObjUID uid,
		CORBA_boolean query_in_progress,
		CORBA_long n_scanned,
		CORBA_long total,
		gpointer data)
{
	CalQuery *query;

	query = CAL_QUERY (data);

	gtk_signal_emit (GTK_OBJECT (query), query_signals[OBJ_UPDATED],
			 uid, query_in_progress, (int) n_scanned, (int) total);
}

/* Callback used when an object is removed from the query */
static void
obj_removed_cb (QueryListener *ql,
		const GNOME_Evolution_Calendar_CalObjUID uid,
		gpointer data)
{
	CalQuery *query;

	query = CAL_QUERY (data);

	gtk_signal_emit (GTK_OBJECT (query), query_signals[OBJ_REMOVED],
			 uid);
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

	gtk_signal_emit (GTK_OBJECT (query), query_signals[QUERY_DONE],
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

	gtk_signal_emit (GTK_OBJECT (query), query_signals[EVAL_ERROR],
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

	if (ev._major == CORBA_USER_EXCEPTION
	    && strcmp (CORBA_exception_id (&ev),
		       ex_GNOME_Evolution_Calendar_Cal_CouldNotCreate) == 0) {
		g_message ("cal_query_construct(): The server could not create the query");
		goto error;
	} else if (ev._major != CORBA_NO_EXCEPTION) {
		g_message ("cal_query_construct(): Could not issue the getQuery() request");
		goto error;
	}

	CORBA_exception_free (&ev);

	return query;

 error:

	CORBA_exception_free (&ev);

	bonobo_object_unref (BONOBO_OBJECT (priv->ql));
	priv->ql = NULL;
	return NULL;
}

/**
 * cal_query_new:
 * @cal: Handle to an open calendar.
 * @sexp: S-expression that defines the query.
 * 
 * Creates a new query object by issuing the query creation request to the
 * calendar server.
 * 
 * Return value: A newly-created query object, or NULL if the request failed.
 **/
CalQuery *
cal_query_new (GNOME_Evolution_Calendar_Cal cal,
	       const char *sexp)
{
	CalQuery *query;

	query = gtk_type_new (CAL_QUERY_TYPE);

	if (!cal_query_construct (query, cal, sexp)) {
		gtk_object_unref (GTK_OBJECT (query));
		return NULL;
	}

	return query;
}
