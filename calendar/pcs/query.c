/* Evolution calendar - Live search query implementation
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

#include <glib.h>
#include <bonobo/bonobo-exception.h>
#include <e-util/e-component-listener.h>
#include <pcs/cal-backend-object-sexp.h>
#include "query.h"



/* Private part of the Query structure */
struct _QueryPrivate {
	/* The backend we are monitoring */
	CalBackend *backend;

	/* The listener we report to */
	GNOME_Evolution_Calendar_QueryListener listener;
	EComponentListener *component_listener;

	/* Sexp that defines the query */
	CalBackendObjectSExp *sexp;
};




static void query_class_init (QueryClass *class);
static void query_init (Query *query, QueryClass *class);
static void query_finalize (GObject *object);

static BonoboObjectClass *parent_class;



BONOBO_TYPE_FUNC_FULL (Query,
		       GNOME_Evolution_Calendar_Query,
		       BONOBO_TYPE_OBJECT,
		       query);

/* Property IDs */
enum props {
	PROP_0,
	PROP_BACKEND,
	PROP_LISTENER,
	PROP_SEXP
};


static void
listener_died_cb (EComponentListener *cl, gpointer data)
{
	Query *query = QUERY (data);
	QueryPrivate *priv;

	priv = query->priv;

	g_object_unref (priv->component_listener);
	priv->component_listener = NULL;
	
	bonobo_object_release_unref (priv->listener, NULL);
	priv->listener = NULL;
}

static void
impl_Query_start (PortableServer_Servant servant, CORBA_Environment *ev)
{
	Query *query;
	QueryPrivate *priv;

	query = QUERY (bonobo_object_from_servant (servant));
	priv = query->priv;

	cal_backend_start_query (priv->backend, query);
}

static void
query_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	Query *query;
	QueryPrivate *priv;
	CORBA_Environment ev;

	query = QUERY (object);
	priv = query->priv;
	
	switch (property_id) {
	case PROP_BACKEND:
		priv->backend = CAL_BACKEND (g_value_dup_object (value));
		break;
	case PROP_LISTENER:
		CORBA_exception_init (&ev);
		priv->listener = CORBA_Object_duplicate (g_value_get_pointer (value), &ev);
		CORBA_exception_free (&ev);

		priv->component_listener = e_component_listener_new (priv->listener);
		g_signal_connect (G_OBJECT (priv->component_listener), "component_died",
				  G_CALLBACK (listener_died_cb), query);
		break;
	case PROP_SEXP:
		priv->sexp = CAL_BACKEND_OBJECT_SEXP (g_value_dup_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
query_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	Query *query;
	QueryPrivate *priv;
	
	query = QUERY (object);
	priv = query->priv;

	switch (property_id) {
	case PROP_BACKEND:
		g_value_set_object (value, priv->backend);
	case PROP_LISTENER:
		g_value_set_pointer (value, priv->listener);
		break;
	case PROP_SEXP:
		g_value_set_object (value, priv->sexp);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

/* Class initialization function for the live search query */
static void
query_class_init (QueryClass *klass)
{
	GObjectClass *object_class;
	POA_GNOME_Evolution_Calendar_Query__epv *epv = &klass->epv;
	GParamSpec *param;
	
	object_class = (GObjectClass *) klass;

	parent_class = g_type_class_peek_parent (klass);

	object_class->set_property = query_set_property;
	object_class->get_property = query_get_property;
	object_class->finalize = query_finalize;

	epv->start = impl_Query_start;

	param =  g_param_spec_object ("backend", NULL, NULL, CAL_BACKEND_TYPE,
				      G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);
	g_object_class_install_property (object_class, PROP_BACKEND, param);
	param =  g_param_spec_pointer ("listener", NULL, NULL,
				      G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);
	g_object_class_install_property (object_class, PROP_LISTENER, param);
	param =  g_param_spec_object ("sexp", NULL, NULL, CAL_TYPE_BACKEND_OBJECT_SEXP,
				      G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);
	g_object_class_install_property (object_class, PROP_SEXP, param);
}

/* Object initialization function for the live search query */
static void
query_init (Query *query, QueryClass *class)
{
	QueryPrivate *priv;

	priv = g_new0 (QueryPrivate, 1);
	query->priv = priv;

	priv->backend = NULL;
	priv->listener = NULL;
	priv->component_listener = NULL;
	priv->sexp = NULL;
}

/* Finalize handler for the live search query */
static void
query_finalize (GObject *object)
{
	Query *query;
	QueryPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_QUERY (object));

	query = QUERY (object);
	priv = query->priv;

	if (priv->backend)
		g_object_unref (priv->backend);

	if (priv->listener != NULL)
		bonobo_object_release_unref (priv->listener, NULL);

	if (priv->component_listener != NULL)
		g_object_unref (priv->component_listener);

	if (priv->sexp)
		g_object_unref (priv->sexp);

	g_free (priv);

	if (G_OBJECT_CLASS (parent_class)->finalize)
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

/**
 * query_new:
 * @backend: Calendar backend that the query object will monitor.
 * @ql: Listener for query results.
 * @sexp: Sexp that defines the query.
 * 
 * Creates a new query engine object that monitors a calendar backend.
 * 
 * Return value: A newly-created query object, or NULL on failure.
 **/
Query *
query_new (CalBackend *backend,
	   GNOME_Evolution_Calendar_QueryListener ql,
	   CalBackendObjectSExp *sexp)
{
	Query *query;

	query = g_object_new (QUERY_TYPE, "backend", backend, "listener", ql,
			      "sexp", sexp, NULL);

	return query;
}

/**
 * query_get_sexp
 * @query: A #Query object.
 *
 * Get the expression used for the given query.
 *
 * Returns: the query expression used to search.
 */
const char *
query_get_text (Query *query)
{
	g_return_val_if_fail (IS_QUERY (query), NULL);

	return cal_backend_object_sexp_text (query->priv->sexp);
}

CalBackendObjectSExp *
query_get_object_sexp (Query *query)
{
	g_return_val_if_fail (IS_QUERY (query), NULL);

	return query->priv->sexp;
}

gboolean
query_object_matches (Query *query, const char *object)
{
	QueryPrivate *priv;
	
	g_return_val_if_fail (query != NULL, FALSE);
	g_return_val_if_fail (IS_QUERY (query), FALSE);
	g_return_val_if_fail (object != NULL, FALSE);

	priv = query->priv;
	
	return cal_backend_object_sexp_match_object (priv->sexp, object, priv->backend);
}

void
query_notify_objects_added (Query *query, const GList *objects)
{
	QueryPrivate *priv;
	GNOME_Evolution_Calendar_stringlist obj_list;
	CORBA_Environment ev;
	const GList *l;
	int num_objs, i;
	
	g_return_if_fail (query != NULL);
	g_return_if_fail (IS_QUERY (query));

	priv = query->priv;
	g_return_if_fail (priv->listener != CORBA_OBJECT_NIL);
	
	CORBA_exception_init (&ev);

	num_objs = g_list_length ((GList*)objects);
	obj_list._buffer = GNOME_Evolution_Calendar_stringlist_allocbuf (num_objs);
	obj_list._maximum = num_objs;
	obj_list._length = num_objs;

	for (l = objects, i = 0; l; l = l->next, i++)
		obj_list._buffer[i] = CORBA_string_dup (l->data);

	GNOME_Evolution_Calendar_QueryListener_notifyObjectsAdded (priv->listener, &obj_list, &ev);

	CORBA_free (obj_list._buffer);

	if (BONOBO_EX (&ev))
		g_warning (G_STRLOC ": could not notify the listener of object addition");

	CORBA_exception_free (&ev);
}

void
query_notify_objects_added_1 (Query *query, const char *object)
{
	QueryPrivate *priv;
	GList objects;
	
	g_return_if_fail (query != NULL);
	g_return_if_fail (IS_QUERY (query));

	priv = query->priv;
	g_return_if_fail (priv->listener != CORBA_OBJECT_NIL);

	objects.next = objects.prev = NULL;
	objects.data = (gpointer)object;

	query_notify_objects_added (query, &objects);
}

void
query_notify_objects_modified (Query *query, const GList *objects)
{
	QueryPrivate *priv;
	GNOME_Evolution_Calendar_CalObjUIDSeq obj_list;
	CORBA_Environment ev;
	const GList *l;
	int num_objs, i;
	
	g_return_if_fail (query != NULL);
	g_return_if_fail (IS_QUERY (query));

	priv = query->priv;
	g_return_if_fail (priv->listener != CORBA_OBJECT_NIL);
	
	CORBA_exception_init (&ev);

	num_objs = g_list_length ((GList*)objects);
	obj_list._buffer = GNOME_Evolution_Calendar_stringlist_allocbuf (num_objs);
	obj_list._maximum = num_objs;
	obj_list._length = num_objs;

	for (l = objects, i = 0; l; l = l->next, i++)
		obj_list._buffer[i] = CORBA_string_dup (l->data);

	GNOME_Evolution_Calendar_QueryListener_notifyObjectsModified (priv->listener, &obj_list, &ev);

	CORBA_free (obj_list._buffer);

	if (BONOBO_EX (&ev))
		g_warning (G_STRLOC ": could not notify the listener of object modification");

	CORBA_exception_free (&ev);
}

void
query_notify_objects_modified_1 (Query *query, const char *object)
{
	QueryPrivate *priv;
	GList objects;
	
	g_return_if_fail (query != NULL);
	g_return_if_fail (IS_QUERY (query));

	priv = query->priv;
	g_return_if_fail (priv->listener != CORBA_OBJECT_NIL);

	objects.next = objects.prev = NULL;
	objects.data = (gpointer)object;
	
	query_notify_objects_modified (query, &objects);
}

void
query_notify_objects_removed (Query *query, const GList *uids)
{
	QueryPrivate *priv;
	GNOME_Evolution_Calendar_CalObjUIDSeq uid_list;
	CORBA_Environment ev;
	const GList *l;
	int num_uids, i;
	
	g_return_if_fail (query != NULL);
	g_return_if_fail (IS_QUERY (query));

	priv = query->priv;
	g_return_if_fail (priv->listener != CORBA_OBJECT_NIL);
	
	CORBA_exception_init (&ev);

	num_uids = g_list_length ((GList*)uids);
	uid_list._buffer = GNOME_Evolution_Calendar_CalObjUIDSeq_allocbuf (num_uids);
	uid_list._maximum = num_uids;
	uid_list._length = num_uids;

	for (l = uids, i = 0; l; l = l->next, i ++)
		uid_list._buffer[i] = CORBA_string_dup (l->data);

	GNOME_Evolution_Calendar_QueryListener_notifyObjectsRemoved (priv->listener, &uid_list, &ev);

	CORBA_free (uid_list._buffer);

	if (BONOBO_EX (&ev))
		g_warning (G_STRLOC ": could not notify the listener of object removal");


	CORBA_exception_free (&ev);
}

void
query_notify_objects_removed_1 (Query *query, const char *uid)
{
	QueryPrivate *priv;
	GList uids;
	
	g_return_if_fail (query != NULL);
	g_return_if_fail (IS_QUERY (query));

	priv = query->priv;
	g_return_if_fail (priv->listener != CORBA_OBJECT_NIL);

	uids.next = uids.prev = NULL;
	uids.data = (gpointer)uid;
	
	query_notify_objects_removed (query, &uids);
}

void
query_notify_query_progress (Query *query, const char *message, int percent)
{
	QueryPrivate *priv;	
	CORBA_Environment ev;

	g_return_if_fail (query != NULL);
	g_return_if_fail (IS_QUERY (query));

	priv = query->priv;
	g_return_if_fail (priv->listener != CORBA_OBJECT_NIL);
	
	CORBA_exception_init (&ev);

	GNOME_Evolution_Calendar_QueryListener_notifyQueryProgress (priv->listener, message, percent, &ev);

	if (BONOBO_EX (&ev))
		g_warning (G_STRLOC ": could not notify the listener of query progress");

	CORBA_exception_free (&ev);
}

void
query_notify_query_done (Query *query, GNOME_Evolution_Calendar_CallStatus status)
{
	QueryPrivate *priv;	
	CORBA_Environment ev;

	g_return_if_fail (query != NULL);
	g_return_if_fail (IS_QUERY (query));

	priv = query->priv;
	g_return_if_fail (priv->listener != CORBA_OBJECT_NIL);
	
	CORBA_exception_init (&ev);

	GNOME_Evolution_Calendar_QueryListener_notifyQueryDone (priv->listener, status, &ev);

	if (BONOBO_EX (&ev))
		g_warning (G_STRLOC ": could not notify the listener of query completion");

	CORBA_exception_free (&ev);
}
