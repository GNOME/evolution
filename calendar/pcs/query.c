/* Evolution calendar - Live search query implementation
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

#include <string.h>
#include <glib.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <gtk/gtksignal.h>
#include <e-util/e-sexp.h>
#include <cal-util/cal-recur.h>
#include <cal-util/timeutil.h>
#include "query.h"



/* Private part of the Query structure */
struct _QueryPrivate {
	/* The backend we are monitoring */
	CalBackend *backend;

	/* Listener to which we report changes in the live query */
	GNOME_Evolution_Calendar_QueryListener ql;

	/* Sexp that defines the query */
	char *sexp;
	ESExp *esexp;

	/* Idle handler ID for asynchronous queries */
	guint idle_id;

	/* List of UIDs that we still have to process */
	GList *pending_uids;
	int n_pending;
	int pending_total;

	/* Table of the UIDs we know do match the query */
	GHashTable *uids;

	/* The next component that will be handled in e_sexp_eval(); put here
	 * just because the query object itself is the esexp context.
	 */
	CalComponent *next_comp;
};



static void query_class_init (QueryClass *class);
static void query_init (Query *query);
static void query_destroy (GtkObject *object);

static BonoboXObjectClass *parent_class;



BONOBO_X_TYPE_FUNC_FULL (Query,
			 GNOME_Evolution_Calendar_Query,
			 BONOBO_X_OBJECT_TYPE,
			 query);

/* Class initialization function for the live search query */
static void
query_class_init (QueryClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;

	parent_class = gtk_type_class (BONOBO_X_OBJECT_TYPE);

	object_class->destroy = query_destroy;

	/* The Query interface (ha ha! query interface!) has no methods, so we
	 * don't need to fiddle with the epv.
	 */
}

/* Object initialization function for the live search query */
static void
query_init (Query *query)
{
	QueryPrivate *priv;

	priv = g_new0 (QueryPrivate, 1);
	query->priv = priv;

	priv->backend = NULL;
	priv->ql = CORBA_OBJECT_NIL;
	priv->sexp = NULL;

	priv->pending_uids = NULL;
	priv->uids = g_hash_table_new (g_str_hash, g_str_equal);

	priv->next_comp = NULL;
}

/* Used from g_hash_table_foreach(); frees a UID */
static void
free_uid_cb (gpointer key, gpointer value, gpointer data)
{
	char *uid;

	uid = key;
	g_free (uid);
}

/* Destroy handler for the live search query */
static void
query_destroy (GtkObject *object)
{
	Query *query;
	QueryPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_QUERY (object));

	query = QUERY (object);
	priv = query->priv;

	if (priv->backend) {
		gtk_signal_disconnect_by_data (GTK_OBJECT (priv->backend), query);
		gtk_object_unref (GTK_OBJECT (priv->backend));
		priv->backend = NULL;
	}

	if (priv->ql != CORBA_OBJECT_NIL) {
		CORBA_Environment ev;

		CORBA_exception_init (&ev);
		bonobo_object_release_unref (priv->ql, &ev);

		if (ev._major != CORBA_NO_EXCEPTION)
			g_message ("query_destroy(): Could not unref the listener\n");

		CORBA_exception_free (&ev);

		priv->ql = CORBA_OBJECT_NIL;
	}

	if (priv->sexp) {
		g_free (priv->sexp);
		priv->sexp = NULL;
	}

	if (priv->esexp) {
		e_sexp_unref (priv->esexp);
		priv->esexp = NULL;
	}

	if (priv->idle_id) {
		g_source_remove (priv->idle_id);
		priv->idle_id = 0;
	}

	if (priv->pending_uids) {
		GList *l;

		for (l = priv->pending_uids; l; l = l->next) {
			char *uid;

			uid = l->data;
			g_assert (uid != NULL);
			g_free (uid);
		}

		g_list_free (priv->pending_uids);
		priv->pending_uids = NULL;
		priv->n_pending = 0;
	}

	g_hash_table_foreach (priv->uids, free_uid_cb, NULL);
	g_hash_table_destroy (priv->uids);
	priv->uids = NULL;

	g_free (priv);
	query->priv = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}



/* E-Sexp functions */

/* (time-now)
 *
 * Returns a time_t of time (NULL).
 */
static ESExpResult *
func_time_now (ESExp *esexp, int argc, ESExpResult **argv, void *data)
{
	ESExpResult *result;

	if (argc != 0) {
		e_sexp_fatal_error (esexp, _("time-now expects 0 arguments"));
		return NULL;
	}

	result = e_sexp_result_new (esexp, ESEXP_RES_TIME);
	result->value.time = time (NULL);

	return result;
}

/* (make-time ISODATE)
 *
 * ISODATE - string, ISO 8601 date/time representation
 *
 * Constructs a time_t value for the specified date.
 */
static ESExpResult *
func_make_time (ESExp *esexp, int argc, ESExpResult **argv, void *data)
{
	const char *str;
	time_t t;
	ESExpResult *result;

	if (argc != 1) {
		e_sexp_fatal_error (esexp, _("make-time expects 1 argument"));
		return NULL;
	}

	if (argv[0]->type != ESEXP_RES_STRING) {
		e_sexp_fatal_error (esexp, _("make-time expects argument 1 "
					     "to be a string"));
		return NULL;
	}
	str = argv[0]->value.string;

	t = time_from_isodate (str);
	if (t == -1) {
		e_sexp_fatal_error (esexp, _("make-time argument 1 must be an "
					     "ISO 8601 date/time string"));
		return NULL;
	}

	result = e_sexp_result_new (esexp, ESEXP_RES_TIME);
	result->value.time = t;

	return result;
}

/* (time-add-day TIME N)
 *
 * TIME - time_t, base time
 * N - int, number of days to add
 *
 * Adds the specified number of days to a time value.
 */
static ESExpResult *
func_time_add_day (ESExp *esexp, int argc, ESExpResult **argv, void *data)
{
	ESExpResult *result;
	time_t t;
	int n;

	if (argc != 2) {
		e_sexp_fatal_error (esexp, _("time-add-day expects 2 arguments"));
		return NULL;
	}

	if (argv[0]->type != ESEXP_RES_TIME) {
		e_sexp_fatal_error (esexp, _("time-add-day expects argument 1 "
					     "to be a time_t"));
		return NULL;
	}
	t = argv[0]->value.time;

	if (argv[1]->type != ESEXP_RES_INT) {
		e_sexp_fatal_error (esexp, _("time-add-day expects argument 2 "
					     "to be an integer"));
		return NULL;
	}
	n = argv[1]->value.number;
	
	result = e_sexp_result_new (esexp, ESEXP_RES_TIME);
	result->value.time = time_add_day (t, n);

	return result;
}

/* (time-day-begin TIME)
 *
 * TIME - time_t, base time
 *
 * Returns the start of the day, according to the local time.
 */
static ESExpResult *
func_time_day_begin (ESExp *esexp, int argc, ESExpResult **argv, void *data)
{
	time_t t;
	ESExpResult *result;

	if (argc != 1) {
		e_sexp_fatal_error (esexp, _("time-day-begin expects 1 argument"));
		return NULL;
	}

	if (argv[0]->type != ESEXP_RES_TIME) {
		e_sexp_fatal_error (esexp, _("time-day-begin expects argument 1 "
					     "to be a time_t"));
		return NULL;
	}
	t = argv[0]->value.time;

	result = e_sexp_result_new (esexp, ESEXP_RES_TIME);
	result->value.time = time_day_begin (t);

	return result;
}

/* (time-day-end TIME)
 *
 * TIME - time_t, base time
 *
 * Returns the end of the day, according to the local time.
 */
static ESExpResult *
func_time_day_end (ESExp *esexp, int argc, ESExpResult **argv, void *data)
{
	time_t t;
	ESExpResult *result;

	if (argc != 1) {
		e_sexp_fatal_error (esexp, _("time-day-end expects 1 argument"));
		return NULL;
	}

	if (argv[0]->type != ESEXP_RES_TIME) {
		e_sexp_fatal_error (esexp, _("time-day-end expects argument 1 "
					     "to be a time_t"));
		return NULL;
	}
	t = argv[0]->value.time;

	result = e_sexp_result_new (esexp, ESEXP_RES_TIME);
	result->value.time = time_day_end (t);

	return result;
}

/* (get-vtype)
 *
 * Returns a string indicating the type of component (VEVENT, VTODO, VJOURNAL,
 * VFREEBUSY, VTIMEZONE, UNKNOWN).
 */
static ESExpResult *
func_get_vtype (ESExp *esexp, int argc, ESExpResult **argv, void *data)
{
	Query *query;
	QueryPrivate *priv;
	CalComponent *comp;
	CalComponentVType vtype;
	char *str;
	ESExpResult *result;

	query = QUERY (data);
	priv = query->priv;

	g_assert (priv->next_comp != NULL);
	comp = priv->next_comp;

	/* Check argument types */

	if (argc != 0) {
		e_sexp_fatal_error (esexp, _("get-vtype expects 0 arguments"));
		return NULL;
	}

	/* Get the type */

	vtype = cal_component_get_vtype (comp);

	switch (vtype) {
	case CAL_COMPONENT_EVENT:
		str = g_strdup ("VEVENT");
		break;

	case CAL_COMPONENT_TODO:
		str = g_strdup ("VTODO");
		break;

	case CAL_COMPONENT_JOURNAL:
		str = g_strdup ("VJOURNAL");
		break;

	case CAL_COMPONENT_FREEBUSY:
		str = g_strdup ("VFREEBUSY");
		break;

	case CAL_COMPONENT_TIMEZONE:
		str = g_strdup ("VTIMEZONE");
		break;

	default:
		str = g_strdup ("UNKNOWN");
		break;
	}

	result = e_sexp_result_new (esexp, ESEXP_RES_STRING);
	result->value.string = str;

	return result;
}

/* Sets a boolean value in the data to TRUE; called from
 * cal_recur_generate_instances() to indicate that at least one instance occurs
 * in the sought time range.  We always return FALSE because we want the
 * recurrence engine to finish as soon as possible.
 */
static gboolean
instance_occur_cb (CalComponent *comp, time_t start, time_t end, gpointer data)
{
	gboolean *occurs;

	occurs = data;
	*occurs = TRUE;

	return FALSE;
}

/* (occur-in-time-range? START END)
 *
 * START - time_t, start of the time range
 * END - time_t, end of the time range
 *
 * Returns a boolean indicating whether the component has any occurrences in the
 * specified time range.
 */
static ESExpResult *
func_occur_in_time_range (ESExp *esexp, int argc, ESExpResult **argv, void *data)
{
	Query *query;
	QueryPrivate *priv;
	CalComponent *comp;
	time_t start, end;
	gboolean occurs;
	ESExpResult *result;

	query = QUERY (data);
	priv = query->priv;

	g_assert (priv->next_comp != NULL);
	comp = priv->next_comp;

	/* Check argument types */

	if (argc != 2) {
		e_sexp_fatal_error (esexp, _("occur-in-time-range? expects 2 arguments"));
		return NULL;
	}

	if (argv[0]->type != ESEXP_RES_TIME) {
		e_sexp_fatal_error (esexp, _("occur-in-time-range? expects argument 1 "
					     "to be a time_t"));
		return NULL;
	}
	start = argv[0]->value.time;

	if (argv[1]->type != ESEXP_RES_TIME) {
		e_sexp_fatal_error (esexp, _("occur-in-time-range? expects argument 2 "
					     "to be a time_t"));
		return NULL;
	}
	end = argv[1]->value.time;

	/* See if there is at least one instance in that range */

	occurs = FALSE;
	cal_recur_generate_instances (comp, start, end, instance_occur_cb, &occurs);

	result = e_sexp_result_new (esexp, ESEXP_RES_BOOL);
	result->value.bool = occurs;

	return result;
}



/* Adds a component to our the UIDs hash table and notifies the client */
static void
add_component (Query *query, const char *uid, gboolean query_in_progress, int n_scanned, int total)
{
	QueryPrivate *priv;
	char *old_uid;
	CORBA_Environment ev;

	if (query_in_progress)
		g_assert (n_scanned > 0 || n_scanned <= total);

	priv = query->priv;

	if (g_hash_table_lookup_extended (priv->uids, uid, (gpointer *) &old_uid, NULL)) {
		g_hash_table_remove (priv->uids, old_uid);
		g_free (old_uid);
	}

	g_hash_table_insert (priv->uids, g_strdup (uid), NULL);

	CORBA_exception_init (&ev);
	GNOME_Evolution_Calendar_QueryListener_notifyObjUpdated (
		priv->ql,
		(char *) uid,
		query_in_progress,
		n_scanned,
		total,
		&ev);

	if (ev._major != CORBA_NO_EXCEPTION)
		g_message ("add_component(): Could not notify the listener of an "
			   "updated component");

	CORBA_exception_free (&ev);
}

/* Removes a component from our the UIDs hash table and notifies the client */
static void
remove_component (Query *query, const char *uid)
{
	QueryPrivate *priv;
	char *old_uid;
	CORBA_Environment ev;

	priv = query->priv;

	if (!g_hash_table_lookup_extended (priv->uids, uid, (gpointer *) &old_uid, NULL))
		return;

	/* The component did match the query before but it no longer does, so we
	 * have to notify the client.
	 */

	g_hash_table_remove (priv->uids, old_uid);
	g_free (old_uid);

	CORBA_exception_init (&ev);
	GNOME_Evolution_Calendar_QueryListener_notifyObjRemoved (
		priv->ql,
		(char *) uid,
		&ev);

	if (ev._major != CORBA_NO_EXCEPTION)
		g_message ("remove_component(): Could not notify the listener of a "
			   "removed component");

	CORBA_exception_free (&ev);
}

/* Removes a component from the list of pending UIDs */
static void
remove_from_pending (Query *query, const char *remove_uid)
{
	QueryPrivate *priv;
	GList *l;

	priv = query->priv;

	for (l = priv->pending_uids; l; l = l->next) {
		char *uid;

		g_assert (priv->n_pending > 0);

		uid = l->data;
		if (strcmp (remove_uid, uid))
			continue;

		g_free (uid);

		priv->pending_uids = g_list_remove_link (priv->pending_uids, l);
		g_list_free_1 (l);
		priv->n_pending--;

		g_assert ((priv->pending_uids && priv->n_pending != 0)
			  || (!priv->pending_uids && priv->n_pending == 0));

		break;
	}
}

static struct {
	char *name;
	ESExpFunc *func;
} functions[] = {
	/* Time-related functions */
	{ "time-now", func_time_now },
	{ "make-time", func_make_time },
	{ "time-add-day", func_time_add_day },
	{ "time-day-begin", func_time_day_begin },
	{ "time-day-end", func_time_day_end },

	/* Component-related functions */
	{ "get-vtype", func_get_vtype },
	{ "occur-in-time-range?", func_occur_in_time_range }
};

/* Initializes a sexp by interning our own symbols */
static ESExp *
create_sexp (Query *query)
{
	ESExp *esexp;
	int i;

	esexp = e_sexp_new ();

	for (i = 0; i < (sizeof (functions) / sizeof (functions[0])); i++)
		e_sexp_add_function (esexp, 0, functions[i].name, functions[i].func, query);

	return esexp;
}

/* Evaluates the query sexp on the specified component and notifies the listener
 * as appropriate.
 */
static void
match_component (Query *query, const char *uid,
		 gboolean query_in_progress, int n_scanned, int total)
{
	QueryPrivate *priv;
	char *comp_str;
	CalComponent *comp;
	icalcomponent *icalcomp;
	gboolean set_succeeded;
	ESExpResult *result;

	priv = query->priv;

	comp_str = cal_backend_get_object (priv->backend, uid);
	g_assert (comp_str != NULL);

	icalcomp = icalparser_parse_string (comp_str);
	g_assert (icalcomp != NULL);

	g_free (comp_str);

	comp = cal_component_new ();
	set_succeeded = cal_component_set_icalcomponent (comp, icalcomp);
	g_assert (set_succeeded);

	/* Eval the sexp */

	g_assert (priv->next_comp == NULL);

	priv->next_comp = comp;
	result = e_sexp_eval (priv->esexp);
	gtk_object_unref (GTK_OBJECT (comp));
	priv->next_comp = NULL;

	if (!result) {
		const char *error_str;
		CORBA_Environment ev;

		error_str = e_sexp_error (priv->esexp);
		g_assert (error_str != NULL);

		CORBA_exception_init (&ev);
		GNOME_Evolution_Calendar_QueryListener_notifyEvalError (
			priv->ql,
			error_str,
			&ev);

		if (ev._major != CORBA_NO_EXCEPTION)
			g_message ("process_component_cb(): Could not notify the listener of "
				   "an evaluation error");

		CORBA_exception_free (&ev);
		return;
	} else if (result->type != ESEXP_RES_BOOL) {
		CORBA_Environment ev;

		CORBA_exception_init (&ev);
		GNOME_Evolution_Calendar_QueryListener_notifyEvalError (
			priv->ql,
			_("Evaluation of the search expression did not yield a boolean value"),
			&ev);

		if (ev._major != CORBA_NO_EXCEPTION)
			g_message ("process_component_cb(): Could not notify the listener of "
				   "an unexpected result value type when evaluating the "
				   "search expression");

		CORBA_exception_free (&ev);
	} else {
		/* Success; process the component accordingly */

		if (result->value.bool)
			add_component (query, uid, query_in_progress, n_scanned, total);
		else
			remove_component (query, uid);
	}

	e_sexp_result_free (priv->esexp, result);
}

/* Processes a single component that is queued in the list */
static gboolean
process_component_cb (gpointer data)
{
	Query *query;
	QueryPrivate *priv;
	char *uid;
	GList *l;

	query = QUERY (data);
	priv = query->priv;

	/* No more components? */

	if (!priv->pending_uids) {
		g_assert (priv->n_pending == 0);

		g_source_remove (priv->idle_id);
		priv->idle_id = 0;
		return FALSE;
	}

	g_assert (priv->n_pending > 0);

	/* Fetch the component */

	l = priv->pending_uids;
	priv->pending_uids = g_list_remove_link (priv->pending_uids, l);
	priv->n_pending--;

	g_assert ((priv->pending_uids && priv->n_pending != 0)
		  || (!priv->pending_uids && priv->n_pending == 0));

	uid = l->data;
	g_assert (uid != NULL);

	g_list_free_1 (l);

	match_component (query, uid,
			 TRUE,
			 priv->pending_total - priv->n_pending,
			 priv->pending_total);

	g_free (uid);

	return TRUE;
}

/* Populates the query with pending UIDs so that they can be processed
 * asynchronously.
 */
static void
populate_query (Query *query)
{
	QueryPrivate *priv;

	priv = query->priv;
	g_assert (priv->idle_id == 0);

	priv->pending_uids = cal_backend_get_uids (priv->backend, CALOBJ_TYPE_ANY);
	priv->pending_total = g_list_length (priv->pending_uids);
	priv->n_pending = priv->pending_total;

	priv->idle_id = g_idle_add (process_component_cb, query);
}

/* Idle handler for starting a query */
static gboolean
start_query_cb (gpointer data)
{
	Query *query;
	QueryPrivate *priv;
	CORBA_Environment ev;

	query = QUERY (data);
	priv = query->priv;

	priv->idle_id = 0;

	priv->esexp = create_sexp (query);

	/* Compile the query string */

	g_assert (priv->sexp != NULL);
	e_sexp_input_text (priv->esexp, priv->sexp, strlen (priv->sexp));

	if (e_sexp_parse (priv->esexp) == -1) {
		const char *error_str;

		error_str = e_sexp_error (priv->esexp);
		g_assert (error_str != NULL);

		CORBA_exception_init (&ev);
		GNOME_Evolution_Calendar_QueryListener_notifyQueryDone (
			priv->ql,
			GNOME_Evolution_Calendar_QueryListener_PARSE_ERROR,
			error_str,
			&ev);

		if (ev._major != CORBA_NO_EXCEPTION)
			g_message ("start_query_cb(): Could not notify the listener of "
				   "a parse error");

		CORBA_exception_free (&ev);
		return FALSE;
	}

	/* Populate the query with UIDs so that we can process them asynchronously */

	populate_query (query);

	return FALSE;
}

/* Callback used when the backend gets loaded; we just queue the query to be
 * started later.
 */
static void
backend_opened_cb (CalBackend *backend, CalBackendOpenStatus status, gpointer data)
{
	Query *query;
	QueryPrivate *priv;

	query = QUERY (data);
	priv = query->priv;

	if (status == CAL_BACKEND_OPEN_SUCCESS) {
		g_assert (cal_backend_is_loaded (backend));

		priv->idle_id = g_idle_add (start_query_cb, query);
	}
}

/* Callback used when a component changes in the backend */
static void
backend_obj_updated_cb (CalBackend *backend, const char *uid, gpointer data)
{
	Query *query;

	query = QUERY (data);

	match_component (query, uid, FALSE, 0, 0);
	remove_from_pending (query, uid);
}

/* Callback used when a component is removed from the backend */
static void
backend_obj_removed_cb (CalBackend *backend, const char *uid, gpointer data)
{
	Query *query;
	QueryPrivate *priv;

	query = QUERY (data);
	priv = query->priv;

	remove_component (query, uid);
	remove_from_pending (query, uid);
}

/**
 * query_construct:
 * @query: A live search query.
 * @backend: Calendar backend that the query object will monitor.
 * @ql: Listener for query results.
 * @sexp: Sexp that defines the query.
 * 
 * Constructs a #Query object by binding it to a calendar backend and a query
 * listener.  The @query object will start to populate itself asynchronously and
 * call the listener as appropriate.
 * 
 * Return value: The same value as @query, or NULL if the query could not
 * be constructed.
 **/
Query *
query_construct (Query *query,
		 CalBackend *backend,
		 GNOME_Evolution_Calendar_QueryListener ql,
		 const char *sexp)
{
	QueryPrivate *priv;
	CORBA_Environment ev;

	g_return_val_if_fail (query != NULL, NULL);
	g_return_val_if_fail (IS_QUERY (query), NULL);
	g_return_val_if_fail (backend != NULL, NULL);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), NULL);
	g_return_val_if_fail (ql != CORBA_OBJECT_NIL, NULL);
	g_return_val_if_fail (sexp != NULL, NULL);

	priv = query->priv;

	CORBA_exception_init (&ev);
	priv->ql = CORBA_Object_duplicate (ql, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_message ("query_construct(): Could not duplicate the listener");
		priv->ql = CORBA_OBJECT_NIL;
		CORBA_exception_free (&ev);
		return NULL;
	}
	CORBA_exception_free (&ev);

	priv->backend = backend;
	gtk_object_ref (GTK_OBJECT (priv->backend));

	gtk_signal_connect (GTK_OBJECT (priv->backend), "obj_updated",
			    GTK_SIGNAL_FUNC (backend_obj_updated_cb),
			    query);
	gtk_signal_connect (GTK_OBJECT (priv->backend), "obj_removed",
			    GTK_SIGNAL_FUNC (backend_obj_removed_cb),
			    query);

	priv->sexp = g_strdup (sexp);

	/* Queue the query to be started asynchronously */

	if (cal_backend_is_loaded (priv->backend))
		priv->idle_id = g_idle_add (start_query_cb, query);
	else
		gtk_signal_connect (GTK_OBJECT (priv->backend), "opened",
				    GTK_SIGNAL_FUNC (backend_opened_cb),
				    query);

	return query;
}

Query *
query_new (CalBackend *backend,
	   GNOME_Evolution_Calendar_QueryListener ql,
	   const char *sexp)
{
	Query *query;

	query = QUERY (gtk_type_new (QUERY_TYPE));
	return query_construct (query, backend, ql, sexp);
}
