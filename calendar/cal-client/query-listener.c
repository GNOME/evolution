/* Evolution calendar - Live search query listener convenience object
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

#include "query-listener.h"



/* Private part of the QueryListener structure */

struct _QueryListenerPrivate {
	/* Callbacks for notification and their closure data */
	QueryListenerObjUpdatedFn obj_updated_fn;
	QueryListenerObjRemovedFn obj_removed_fn;
	QueryListenerQueryDoneFn query_done_fn;
	QueryListenerEvalErrorFn eval_error_fn;
	gpointer fn_data;
};



static void query_listener_class_init (QueryListenerClass *class);
static void query_listener_init (QueryListener *ql);
static void query_listener_destroy (GtkObject *object);

static void impl_notifyObjUpdated (PortableServer_Servant servant,
				   GNOME_Evolution_Calendar_CalObjUID uid,
				   CORBA_boolean query_in_progress,
				   CORBA_long n_scanned,
				   CORBA_long total,
				   CORBA_Environment *ev);

static void impl_notifyObjRemoved (PortableServer_Servant servant,
				   GNOME_Evolution_Calendar_CalObjUID uid,
				   CORBA_Environment *ev);

static void impl_notifyQueryDone (PortableServer_Servant servant,
				  GNOME_Evolution_Calendar_QueryListener_QueryDoneStatus corba_status,
				  const CORBA_char *error_str,
				  CORBA_Environment *ev);

static void impl_notifyEvalError (PortableServer_Servant servant,
				  const CORBA_char *error_str,
				  CORBA_Environment *ev);

static BonoboXObjectClass *parent_class;



BONOBO_X_TYPE_FUNC_FULL (QueryListener,
			 GNOME_Evolution_Calendar_QueryListener,
			 BONOBO_X_OBJECT_TYPE,
			 query_listener);

/* Class initialization function for the live search query listener */
static void
query_listener_class_init (QueryListenerClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;

	parent_class = gtk_type_class (BONOBO_X_OBJECT_TYPE);

	object_class->destroy = query_listener_destroy;

	class->epv.notifyObjUpdated = impl_notifyObjUpdated;
	class->epv.notifyObjRemoved = impl_notifyObjRemoved;
	class->epv.notifyQueryDone = impl_notifyQueryDone;
	class->epv.notifyEvalError = impl_notifyEvalError;
}

/* Object initialization function for the live search query listener */
static void
query_listener_init (QueryListener *ql)
{
	QueryListenerPrivate *priv;

	priv = g_new0 (QueryListenerPrivate, 1);
	ql->priv = priv;

	priv->obj_updated_fn = NULL;
	priv->obj_removed_fn = NULL;
	priv->query_done_fn = NULL;
	priv->eval_error_fn = NULL;
	priv->fn_data = NULL;
}

/* Destroy handler for the live search query listener */
static void
query_listener_destroy (GtkObject *object)
{
	QueryListener *ql;
	QueryListenerPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_QUERY_LISTENER (object));

	ql = QUERY_LISTENER (object);
	priv = ql->priv;

	priv->obj_updated_fn = NULL;
	priv->obj_removed_fn = NULL;
	priv->query_done_fn = NULL;
	priv->eval_error_fn = NULL;
	priv->fn_data = NULL;

	g_free (priv);
	ql->priv = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}



/* CORBA method implementations */

/* ::notifyObjUpdated() method */
static void
impl_notifyObjUpdated (PortableServer_Servant servant,
		       GNOME_Evolution_Calendar_CalObjUID uid,
		       CORBA_boolean query_in_progress,
		       CORBA_long n_scanned,
		       CORBA_long total,
		       CORBA_Environment *ev)
{
	QueryListener *ql;
	QueryListenerPrivate *priv;

	ql = QUERY_LISTENER (bonobo_object_from_servant (servant));
	priv = ql->priv;

	g_assert (priv->obj_updated_fn != NULL);
	(* priv->obj_updated_fn) (ql, uid, query_in_progress, n_scanned, total, priv->fn_data);
}

/* ::notifyObjRemoved() method */
static void
impl_notifyObjRemoved (PortableServer_Servant servant,
		       GNOME_Evolution_Calendar_CalObjUID uid,
		       CORBA_Environment *ev)
{
	QueryListener *ql;
	QueryListenerPrivate *priv;

	ql = QUERY_LISTENER (bonobo_object_from_servant (servant));
	priv = ql->priv;

	g_assert (priv->obj_removed_fn != NULL);
	(* priv->obj_removed_fn) (ql, uid, priv->fn_data);
}

/* ::notifyQueryDone() method */
static void
impl_notifyQueryDone (PortableServer_Servant servant,
		      GNOME_Evolution_Calendar_QueryListener_QueryDoneStatus corba_status,
		      const CORBA_char *error_str,
		      CORBA_Environment *ev)
{
	QueryListener *ql;
	QueryListenerPrivate *priv;

	ql = QUERY_LISTENER (bonobo_object_from_servant (servant));
	priv = ql->priv;

	g_assert (priv->query_done_fn != NULL);
	(* priv->query_done_fn) (ql, corba_status, error_str, priv->fn_data);
}

/* ::notifyEvalError() method */
static void
impl_notifyEvalError (PortableServer_Servant servant,
		      const CORBA_char *error_str,
		      CORBA_Environment *ev)
{
	QueryListener *ql;
	QueryListenerPrivate *priv;

	ql = QUERY_LISTENER (bonobo_object_from_servant (servant));
	priv = ql->priv;

	g_assert (priv->eval_error_fn != NULL);
	(* priv->eval_error_fn) (ql, error_str, priv->fn_data);
}



/**
 * query_listener_construct:
 * @ql: A query listener.
 * @obj_updated_fn: Callback to use when a component is updated in the query.
 * @obj_removed_fn: Callback to use when a component is removed from the query.
 * @query_done_fn: Callback to use when a query is done.
 * @eval_error_fn: Callback to use when an evaluation error happens during a query.
 * @fn_data: Closure data to pass to the callbacks.
 * 
 * Constructs a query listener by setting the callbacks it will use for
 * notification from the calendar server.
 * 
 * Return value: The same value as @ql.
 **/
QueryListener *
query_listener_construct (QueryListener *ql,
			  QueryListenerObjUpdatedFn obj_updated_fn,
			  QueryListenerObjRemovedFn obj_removed_fn,
			  QueryListenerQueryDoneFn query_done_fn,
			  QueryListenerEvalErrorFn eval_error_fn,
			  gpointer fn_data)
{
	QueryListenerPrivate *priv;

	g_return_val_if_fail (ql != NULL, NULL);
	g_return_val_if_fail (IS_QUERY_LISTENER (ql), NULL);
	g_return_val_if_fail (obj_updated_fn != NULL, NULL);
	g_return_val_if_fail (obj_removed_fn != NULL, NULL);
	g_return_val_if_fail (query_done_fn != NULL, NULL);
	g_return_val_if_fail (eval_error_fn != NULL, NULL);

	priv = ql->priv;

	priv->obj_updated_fn = obj_updated_fn;
	priv->obj_removed_fn = obj_removed_fn;
	priv->query_done_fn = query_done_fn;
	priv->eval_error_fn = eval_error_fn;
	priv->fn_data = fn_data;

	return ql;
}

/**
 * query_listener_new:
 * @obj_updated_fn: Callback to use when a component is updated in the query.
 * @obj_removed_fn: Callback to use when a component is removed from the query.
 * @query_done_fn: Callback to use when a query is done.
 * @eval_error_fn: Callback to use when an evaluation error happens during a query.
 * @fn_data: Closure data to pass to the callbacks.
 * 
 * Creates a new query listener object.
 * 
 * Return value: A newly-created query listener object.
 **/
QueryListener *
query_listener_new (QueryListenerObjUpdatedFn obj_updated_fn,
		    QueryListenerObjRemovedFn obj_removed_fn,
		    QueryListenerQueryDoneFn query_done_fn,
		    QueryListenerEvalErrorFn eval_error_fn,
		    gpointer fn_data)
{
	QueryListener *ql;

	ql = gtk_type_new (QUERY_LISTENER_TYPE);

	return query_listener_construct (ql,
					 obj_updated_fn,
					 obj_removed_fn,
					 query_done_fn,
					 eval_error_fn,
					 fn_data);
}
