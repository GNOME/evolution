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

#include <string.h>
#include <glib.h>
#include <gtk/gtkmain.h>
#include <libgnome/gnome-i18n.h>
#include <bonobo/bonobo-exception.h>
#include <gal/widgets/e-unicode.h>
#include <e-util/e-component-listener.h>
#include <e-util/e-sexp.h>
#include <cal-util/cal-recur.h>
#include <cal-util/timeutil.h>
#include "cal-backend.h"
#include "query.h"
#include "query-backend.h"



typedef struct {
	Query *query;
	GNOME_Evolution_Calendar_QueryListener ql;
	guint tid;
} StartCachedQueryInfo;

/* States of a query */
typedef enum {
	QUERY_WAIT_FOR_BACKEND, /* the query is not populated and the backend is not loaded */
	QUERY_START_PENDING,	/* the query is not populated yet, but the backend is loaded */
	QUERY_IN_PROGRESS,	/* the query is populated; components are still being processed */
	QUERY_DONE,		/* the query is done, but still accepts object changes */
	QUERY_PARSE_ERROR	/* a parse error occurred when initially creating the ESexp */
} QueryState;

/* Private part of the Query structure */
struct _QueryPrivate {
	/* The backend we are monitoring */
	CalBackend *backend;

	/* The cache backend */
	QueryBackend *qb;

	/* The default timezone for the calendar. */
	icaltimezone *default_zone;

	/* Listeners to which we report changes in the live query */
	GList *listeners;
	GList *component_listeners;

	/* Sexp that defines the query */
	char *sexp;
	ESExp *esexp;

	/* Timeout handler ID for asynchronous queries and current state of the query */
	guint timeout_id;
	QueryState state;

	GList *cached_timeouts;

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
static void query_init (Query *query, QueryClass *class);
static void query_finalize (GObject *object);

static BonoboObjectClass *parent_class;
static GList *cached_queries = NULL;



BONOBO_TYPE_FUNC_FULL (Query,
		       GNOME_Evolution_Calendar_Query,
		       BONOBO_TYPE_OBJECT,
		       query);

/* Class initialization function for the live search query */
static void
query_class_init (QueryClass *class)
{
	GObjectClass *object_class;

	object_class = (GObjectClass *) class;

	parent_class = g_type_class_peek_parent (class);

	object_class->finalize = query_finalize;

	/* The Query interface (ha ha! query interface!) has no methods, so we
	 * don't need to fiddle with the epv.
	 */
}

/* Object initialization function for the live search query */
static void
query_init (Query *query, QueryClass *class)
{
	QueryPrivate *priv;

	priv = g_new0 (QueryPrivate, 1);
	query->priv = priv;

	priv->backend = NULL;
	priv->qb = NULL;
	priv->default_zone = NULL;
	priv->listeners = NULL;
	priv->component_listeners = NULL;
	priv->sexp = NULL;

	priv->timeout_id = 0;
	priv->state = QUERY_WAIT_FOR_BACKEND;

	priv->cached_timeouts = NULL;

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

	if (priv->backend) {
		/* If we are waiting for the backend to be opened, we'll be
		 * connected to its "opened" signal.  If we are in the middle of
		 * a query or if we are just waiting for object update
		 * notifications, we'll have the "obj_removed" and "obj_updated"
		 * connections.  Otherwise, we are either in a parse error state
		 * or waiting for the query to be populated, and in both cases
		 * we have no signal connections.
		 */
		if (priv->state == QUERY_WAIT_FOR_BACKEND
		    || priv->state == QUERY_IN_PROGRESS || priv->state == QUERY_DONE)
			g_signal_handlers_disconnect_matched (G_OBJECT (priv->backend),
							      G_SIGNAL_MATCH_DATA,
							      0, 0, NULL, NULL, query);

		g_object_unref (priv->backend);
		priv->backend = NULL;
	}

	priv->qb = NULL;

	if (priv->listeners != NULL) {
		CORBA_Environment ev;
		GList *l;

		CORBA_exception_init (&ev);
		for (l = priv->listeners; l != NULL; l = l->next) {
			bonobo_object_release_unref (l->data, &ev);

			if (BONOBO_EX (&ev))
				g_message ("query_destroy(): Could not unref the listener\n");
		}

		CORBA_exception_free (&ev);

		g_list_free (priv->listeners);
		priv->listeners = NULL;
	}

	if (priv->component_listeners != NULL) {
		g_list_foreach (priv->component_listeners, (GFunc) g_object_unref, NULL);
		g_list_free (priv->component_listeners);
		priv->component_listeners = NULL;
	}

	if (priv->sexp) {
		g_free (priv->sexp);
		priv->sexp = NULL;
	}

	if (priv->esexp) {
		e_sexp_unref (priv->esexp);
		priv->esexp = NULL;
	}

	if (priv->timeout_id) {
		g_source_remove (priv->timeout_id);
		priv->timeout_id = 0;
	}

	if (priv->cached_timeouts) {
		GList *l;

		for (l = priv->cached_timeouts; l != NULL; l = l->next)
			g_source_remove (GPOINTER_TO_INT (l->data));

		g_list_free (priv->cached_timeouts);
		priv->cached_timeouts = NULL;
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

	if (G_OBJECT_CLASS (parent_class)->finalize)
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
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
 *
 * FIXME: TIMEZONES - need to use a timezone or daylight saving changes will
 * make the result incorrect.
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
 *
 * FIXME: TIMEZONES - this uses the current Unix timezone.
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
 *
 * FIXME: TIMEZONES - this uses the current Unix timezone.
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

/* Call the backend function to get a timezone from a TZID. */
static icaltimezone*
resolve_tzid (const char *tzid, gpointer data)
{
	Query *query = data;

	if (!tzid || !tzid[0])
		return NULL;
	else
		return cal_backend_get_timezone (query->priv->backend, tzid);
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

	cal_recur_generate_instances (comp, start, end,
				      instance_occur_cb, &occurs,
				      resolve_tzid, query, priv->default_zone);

	result = e_sexp_result_new (esexp, ESEXP_RES_BOOL);
	result->value.bool = occurs;

	return result;
}

/* Returns whether a list of CalComponentText items matches the specified string */
static gboolean
matches_text_list (GSList *text_list, const char *str)
{
	GSList *l;
	gboolean matches;

	matches = FALSE;

	for (l = text_list; l; l = l->next) {
		CalComponentText *text;

		text = l->data;
		g_assert (text->value != NULL);

		if (e_utf8_strstrcasedecomp (text->value, str) != NULL) {
			matches = TRUE;
			break;
		}
	}

	return matches;
}

/* Returns whether the comments in a component matches the specified string */
static gboolean
matches_comment (CalComponent *comp, const char *str)
{
	GSList *list;
	gboolean matches;

	cal_component_get_comment_list (comp, &list);
	matches = matches_text_list (list, str);
	cal_component_free_text_list (list);

	return matches;
}

/* Returns whether the description in a component matches the specified string */
static gboolean
matches_description (CalComponent *comp, const char *str)
{
	GSList *list;
	gboolean matches;

	cal_component_get_description_list (comp, &list);
	matches = matches_text_list (list, str);
	cal_component_free_text_list (list);

	return matches;
}

/* Returns whether the summary in a component matches the specified string */
static gboolean
matches_summary (CalComponent *comp, const char *str)
{
	CalComponentText text;

	cal_component_get_summary (comp, &text);

	if (!text.value)
		return FALSE;

	return e_utf8_strstrcasedecomp (text.value, str) != NULL;
}

/* Returns whether any text field in a component matches the specified string */
static gboolean
matches_any (CalComponent *comp, const char *str)
{
	/* As an optimization, and to make life easier for the individual
	 * predicate functions, see if we are looking for the empty string right
	 * away.
	 */
	if (strlen (str) == 0)
		return TRUE;

	return (matches_comment (comp, str)
		|| matches_description (comp, str)
		|| matches_summary (comp, str));
}

/* (contains? FIELD STR)
 *
 * FIELD - string, name of field to match (any, comment, description, summary)
 * STR - string, match string
 *
 * Returns a boolean indicating whether the specified field contains the
 * specified string.
 */
static ESExpResult *
func_contains (ESExp *esexp, int argc, ESExpResult **argv, void *data)
{
	Query *query;
	QueryPrivate *priv;
	CalComponent *comp;
	const char *field;
	const char *str;
	gboolean matches;
	ESExpResult *result;

	query = QUERY (data);
	priv = query->priv;

	g_assert (priv->next_comp != NULL);
	comp = priv->next_comp;

	/* Check argument types */

	if (argc != 2) {
		e_sexp_fatal_error (esexp, _("contains? expects 2 arguments"));
		return NULL;
	}

	if (argv[0]->type != ESEXP_RES_STRING) {
		e_sexp_fatal_error (esexp, _("contains? expects argument 1 "
					     "to be a string"));
		return NULL;
	}
	field = argv[0]->value.string;

	if (argv[1]->type != ESEXP_RES_STRING) {
		e_sexp_fatal_error (esexp, _("contains? expects argument 2 "
					     "to be a string"));
		return NULL;
	}
	str = argv[1]->value.string;

	/* See if it matches */

	if (strcmp (field, "any") == 0)
		matches = matches_any (comp, str);
	else if (strcmp (field, "comment") == 0)
		matches = matches_comment (comp, str);
	else if (strcmp (field, "description") == 0)
		matches = matches_description (comp, str);
	else if (strcmp (field, "summary") == 0)
		matches = matches_summary (comp, str);
	else {
		e_sexp_fatal_error (esexp, _("contains? expects argument 1 to "
					     "be one of \"any\", \"summary\", \"description\""));
		return NULL;
	}

	result = e_sexp_result_new (esexp, ESEXP_RES_BOOL);
	result->value.bool = matches;

	return result;
}

/* (has-categories? STR+)
 * (has-categories? #f)
 *
 * STR - At least one string specifying a category
 * Or you can specify a single #f (boolean false) value for components
 * that have no categories assigned to them ("unfiled").
 *
 * Returns a boolean indicating whether the component has all the specified
 * categories.
 */
static ESExpResult *
func_has_categories (ESExp *esexp, int argc, ESExpResult **argv, void *data)
{
	Query *query;
	QueryPrivate *priv;
	CalComponent *comp;
	gboolean unfiled;
	int i;
	GSList *categories;
	gboolean matches;
	ESExpResult *result;

	query = QUERY (data);
	priv = query->priv;

	g_assert (priv->next_comp != NULL);
	comp = priv->next_comp;

	/* Check argument types */

	if (argc < 1) {
		e_sexp_fatal_error (esexp, _("has-categories? expects at least 1 argument"));
		return NULL;
	}

	if (argc == 1 && argv[0]->type == ESEXP_RES_BOOL)
		unfiled = TRUE;
	else
		unfiled = FALSE;

	if (!unfiled)
		for (i = 0; i < argc; i++)
			if (argv[i]->type != ESEXP_RES_STRING) {
				e_sexp_fatal_error (esexp, _("has-categories? expects all arguments "
							     "to be strings or one and only one "
							     "argument to be a boolean false (#f)"));
				return NULL;
			}

	/* Search categories.  First, if there are no categories we return
	 * whether unfiled components are supposed to match.
	 */

	cal_component_get_categories_list (comp, &categories);
	if (!categories) {
		result = e_sexp_result_new (esexp, ESEXP_RES_BOOL);
		result->value.bool = unfiled;

		return result;
	}

	/* Otherwise, we *do* have categories but unfiled components were
	 * requested, so this component does not match.
	 */
	if (unfiled) {
		result = e_sexp_result_new (esexp, ESEXP_RES_BOOL);
		result->value.bool = FALSE;

		return result;
	}

	matches = TRUE;

	for (i = 0; i < argc; i++) {
		const char *sought;
		GSList *l;
		gboolean has_category;

		sought = argv[i]->value.string;

		has_category = FALSE;

		for (l = categories; l; l = l->next) {
			const char *category;

			category = l->data;

			if (strcmp (category, sought) == 0) {
				has_category = TRUE;
				break;
			}
		}

		if (!has_category) {
			matches = FALSE;
			break;
		}
	}

	cal_component_free_categories_list (categories);

	result = e_sexp_result_new (esexp, ESEXP_RES_BOOL);
	result->value.bool = matches;

	return result;
}

/* (is-completed?)
 *
 * Returns a boolean indicating whether the component is completed (i.e. has
 * a COMPLETED property. This is really only useful for TODO components.
 */
static ESExpResult *
func_is_completed (ESExp *esexp, int argc, ESExpResult **argv, void *data)
{
	Query *query;
	QueryPrivate *priv;
	CalComponent *comp;
	ESExpResult *result;
	struct icaltimetype *t;
	gboolean complete = FALSE;

	query = QUERY (data);
	priv = query->priv;

	g_assert (priv->next_comp != NULL);
	comp = priv->next_comp;

	/* Check argument types */

	if (argc != 0) {
		e_sexp_fatal_error (esexp, _("is-completed? expects 0 arguments"));
		return NULL;
	}

	cal_component_get_completed (comp, &t);
	if (t) {
		complete = TRUE;
		cal_component_free_icaltimetype (t);
	}

	result = e_sexp_result_new (esexp, ESEXP_RES_BOOL);
	result->value.bool = complete;

	return result;
}

/* (completed-before? TIME)
 *
 * TIME - time_t
 *
 * Returns a boolean indicating whether the component was completed on or
 * before the given time (i.e. it checks the COMPLETED property).
 * This is really only useful for TODO components.
 */
static ESExpResult *
func_completed_before (ESExp *esexp, int argc, ESExpResult **argv, void *data)
{
	Query *query;
	QueryPrivate *priv;
	CalComponent *comp;
	ESExpResult *result;
	struct icaltimetype *tt;
	icaltimezone *zone;
	gboolean retval = FALSE;
	time_t before_time, completed_time;

	query = QUERY (data);
	priv = query->priv;

	g_assert (priv->next_comp != NULL);
	comp = priv->next_comp;

	/* Check argument types */

	if (argc != 1) {
		e_sexp_fatal_error (esexp, _("completed-before? expects 1 argument"));
		return NULL;
	}

	if (argv[0]->type != ESEXP_RES_TIME) {
		e_sexp_fatal_error (esexp, _("completed-before? expects argument 1 "
					     "to be a time_t"));
		return NULL;
	}
	before_time = argv[0]->value.time;

	cal_component_get_completed (comp, &tt);
	if (tt) {
		/* COMPLETED must be in UTC. */
		zone = icaltimezone_get_utc_timezone ();
		completed_time = icaltime_as_timet_with_zone (*tt, zone);

#if 0
		g_print ("Query Time    : %s", ctime (&before_time));
		g_print ("Completed Time: %s", ctime (&completed_time));
#endif

		/* We want to return TRUE if before_time is after
		   completed_time. */
		if (difftime (before_time, completed_time) > 0) {
#if 0
			g_print ("  Returning TRUE\n");
#endif
			retval = TRUE;
		}

		cal_component_free_icaltimetype (tt);
	}

	result = e_sexp_result_new (esexp, ESEXP_RES_BOOL);
	result->value.bool = retval;

	return result;
}



/* Adds a component to our the UIDs hash table and notifies the client */
static void
add_component (Query *query, const char *uid, gboolean query_in_progress, int n_scanned, int total)
{
	QueryPrivate *priv;
	char *old_uid;
	CORBA_Environment ev;
	GList *l;

	if (query_in_progress)
		g_assert (n_scanned > 0 || n_scanned <= total);

	priv = query->priv;

	if (g_hash_table_lookup_extended (priv->uids, uid, (gpointer *) &old_uid, NULL)) {
		g_hash_table_remove (priv->uids, old_uid);
		g_free (old_uid);
	}

	g_hash_table_insert (priv->uids, g_strdup (uid), NULL);

	CORBA_exception_init (&ev);
	for (l = priv->listeners; l != NULL; l = l->next) {
		GNOME_Evolution_Calendar_CalObjUIDSeq *corba_uids;

		corba_uids = GNOME_Evolution_Calendar_CalObjUIDSeq__alloc ();
		CORBA_sequence_set_release (corba_uids, TRUE);
		corba_uids->_buffer = CORBA_sequence_GNOME_Evolution_Calendar_CalObjUID_allocbuf (1);
		corba_uids->_length = 1;
		corba_uids->_buffer[0] = CORBA_string_dup (uid);

		GNOME_Evolution_Calendar_QueryListener_notifyObjUpdated (
			l->data,
			corba_uids,
			query_in_progress,
			n_scanned,
			total,
			&ev);

		if (BONOBO_EX (&ev))
			g_message ("add_component(): Could not notify the listener of an "
				   "updated component");

		CORBA_free (corba_uids);
	}

	CORBA_exception_free (&ev);
}

/* Removes a component from our the UIDs hash table and notifies the client */
static void
remove_component (Query *query, const char *uid)
{
	QueryPrivate *priv;
	char *old_uid;
	CORBA_Environment ev;
	GList *l;

	priv = query->priv;

	if (!g_hash_table_lookup_extended (priv->uids, uid, (gpointer *) &old_uid, NULL))
		return;

	/* The component did match the query before but it no longer does, so we
	 * have to notify the client.
	 */

	g_hash_table_remove (priv->uids, old_uid);
	g_free (old_uid);

	CORBA_exception_init (&ev);
	for (l = priv->listeners; l != NULL; l = l->next) {
		GNOME_Evolution_Calendar_QueryListener_notifyObjRemoved (
			l->data,
			(char *) uid,
			&ev);

		if (BONOBO_EX (&ev))
			g_message ("remove_component(): Could not notify the listener of a "
				   "removed component");
	}

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
	{ "occur-in-time-range?", func_occur_in_time_range },
	{ "contains?", func_contains },
	{ "has-categories?", func_has_categories },
	{ "is-completed?", func_is_completed },
	{ "completed-before?", func_completed_before }
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

/* Creates the ESexp and parses the esexp.  If a parse error occurs, it sets the
 * query state to QUERY_PARSE_ERROR and returns FALSE.
 */
static gboolean
parse_sexp (Query *query)
{
	QueryPrivate *priv;

	priv = query->priv;

	/* Compile the query string */

	priv->esexp = create_sexp (query);

	g_assert (priv->sexp != NULL);
	e_sexp_input_text (priv->esexp, priv->sexp, strlen (priv->sexp));

	if (e_sexp_parse (priv->esexp) == -1) {
		const char *error_str;
		CORBA_Environment ev;
		GList *l;

		priv->state = QUERY_PARSE_ERROR;

		/* Report the error to the listeners */

		error_str = e_sexp_error (priv->esexp);
		g_assert (error_str != NULL);

		CORBA_exception_init (&ev);
		for (l = priv->listeners; l != NULL; l = l->next) {
			GNOME_Evolution_Calendar_QueryListener_notifyQueryDone (
				l->data,
				GNOME_Evolution_Calendar_QueryListener_PARSE_ERROR,
				error_str,
				&ev);

			if (BONOBO_EX (&ev))
				g_message ("parse_sexp(): Could not notify the listener of "
					   "a parse error");
		}

		CORBA_exception_free (&ev);

		e_sexp_unref (priv->esexp);
		priv->esexp = NULL;

		/* remove the query from the list of cached queries */
		cached_queries = g_list_remove (cached_queries, query);
		bonobo_object_unref (BONOBO_OBJECT (query));

		return FALSE;
	}

	return TRUE;
}

/* Evaluates the query sexp on the specified component and notifies the listener
 * as appropriate.
 */
static void
match_component (Query *query, const char *uid,
		 gboolean query_in_progress, int n_scanned, int total)
{
	QueryPrivate *priv;
	CalComponent *comp;
	ESExpResult *result;

	priv = query->priv;

	g_assert (priv->state == QUERY_IN_PROGRESS || priv->state == QUERY_DONE);
	g_assert (priv->esexp != NULL);

	comp = query_backend_get_object_component (priv->qb, uid);
	if (!comp)
		return;

	/* Eval the sexp */

	g_assert (priv->next_comp == NULL);

	priv->next_comp = comp;
	result = e_sexp_eval (priv->esexp);
	priv->next_comp = NULL;

	if (!result) {
		const char *error_str;
		CORBA_Environment ev;
		GList *l;

		error_str = e_sexp_error (priv->esexp);
		g_assert (error_str != NULL);

		CORBA_exception_init (&ev);
		for (l = priv->listeners; l != NULL; l = l->next) {
			GNOME_Evolution_Calendar_QueryListener_notifyEvalError (
				l->data,
				error_str,
				&ev);

			if (BONOBO_EX (&ev))
				g_message ("match_component(): Could not notify the listener of "
					   "an evaluation error");
		}

		CORBA_exception_free (&ev);
		return;
	} else if (result->type != ESEXP_RES_BOOL) {
		CORBA_Environment ev;
		GList *l;

		CORBA_exception_init (&ev);
		for (l = priv->listeners; l != NULL; l = l->next) {
			GNOME_Evolution_Calendar_QueryListener_notifyEvalError (
				l->data,
				_("Evaluation of the search expression did not yield a boolean value"),
				&ev);

			if (BONOBO_EX (&ev))
				g_message ("match_component(): Could not notify the listener of "
					   "an unexpected result value type when evaluating the "
					   "search expression");
		}

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

/* Processes all components that are queued in the list */
static gboolean
process_components_cb (gpointer data)
{
	Query *query;
	QueryPrivate *priv;
	char *uid;
	GList *l;
	CORBA_Environment ev;

	query = QUERY (data);
	priv = query->priv;

	g_source_remove (priv->timeout_id);
	priv->timeout_id = 0;

	bonobo_object_ref (BONOBO_OBJECT (query));

	while (priv->pending_uids) {
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

		/* run the main loop, for not blocking */
		if (gtk_events_pending ())
			gtk_main_iteration ();
	}

	bonobo_object_unref (BONOBO_OBJECT (query));
	if (!priv || !priv->listeners)
		return FALSE;

	/* notify listeners that the query ended */
	priv->state = QUERY_DONE;

	CORBA_exception_init (&ev);
	for (l = priv->listeners; l != NULL; l = l->next) {
		GNOME_Evolution_Calendar_QueryListener_notifyQueryDone (
			l->data,
			GNOME_Evolution_Calendar_QueryListener_SUCCESS,
			"",
			&ev);

		if (BONOBO_EX (&ev))
			g_message ("start_query(): Could not notify the listener of "
				   "a finished query");
	}

	CORBA_exception_free (&ev);

	return FALSE;
}

/* Callback used when a component changes in the backend */
static void
backend_obj_updated_cb (CalBackend *backend, const char *uid, gpointer data)
{
	Query *query;
	QueryPrivate *priv;

	query = QUERY (data);
	priv = query->priv;

	g_assert (priv->state == QUERY_IN_PROGRESS || priv->state == QUERY_DONE);

	bonobo_object_ref (BONOBO_OBJECT (query));

	match_component (query, uid, FALSE, 0, 0);
	remove_from_pending (query, uid);

	bonobo_object_unref (BONOBO_OBJECT (query));
}

/* Callback used when a component is removed from the backend */
static void
backend_obj_removed_cb (CalBackend *backend, const char *uid, gpointer data)
{
	Query *query;
	QueryPrivate *priv;

	query = QUERY (data);
	priv = query->priv;

	g_assert (priv->state == QUERY_IN_PROGRESS || priv->state == QUERY_DONE);

	bonobo_object_ref (BONOBO_OBJECT (query));

	remove_component (query, uid);
	remove_from_pending (query, uid);

	bonobo_object_unref (BONOBO_OBJECT (query));
}

/* Actually starts the query */
static void
start_query (Query *query)
{
	QueryPrivate *priv;

	priv = query->priv;

	if (!parse_sexp (query))
		return;

	/* Populate the query with UIDs so that we can process them asynchronously */

	priv->state = QUERY_IN_PROGRESS;
	priv->pending_uids = query_backend_get_uids (priv->qb, CALOBJ_TYPE_ANY);
	priv->pending_total = g_list_length (priv->pending_uids);
	priv->n_pending = priv->pending_total;

	g_signal_connect (G_OBJECT (priv->backend), "obj_updated",
			  G_CALLBACK (backend_obj_updated_cb),
			  query);
	g_signal_connect (G_OBJECT (priv->backend), "obj_removed",
			  G_CALLBACK (backend_obj_removed_cb),
			  query);

	priv->timeout_id = g_timeout_add (100, (GSourceFunc) process_components_cb, query);
}

/* Idle handler for starting a query */
static gboolean
start_query_cb (gpointer data)
{
	Query *query;
	QueryPrivate *priv;

	query = QUERY (data);
	priv = query->priv;

	g_source_remove (priv->timeout_id);
	priv->timeout_id = 0;

	if (priv->state == QUERY_START_PENDING) {
		priv->state = QUERY_IN_PROGRESS;
		start_query (query);
	}

	return FALSE;
}

static void
listener_died_cb (EComponentListener *cl, gpointer data)
{
	QueryPrivate *priv;
	Query *query = QUERY (data);
	GNOME_Evolution_Calendar_QueryListener ql;
	CORBA_Environment ev;

	priv = query->priv;

	ql = e_component_listener_get_component (cl);
	priv->listeners = g_list_remove (priv->listeners, ql);

	priv->component_listeners = g_list_remove (priv->component_listeners, cl);
	g_object_unref (cl);

	CORBA_exception_init (&ev);
	bonobo_object_release_unref (ql, &ev);

	if (BONOBO_EX (&ev))
		g_message ("query_destroy(): Could not unref the listener\n");

	CORBA_exception_free (&ev);
}

static void
add_uid_cb (gpointer key, gpointer value, gpointer data)
{
	char *uid = (char *) key;
	GList **uidlist = (GList **) data;

	*uidlist = g_list_append (*uidlist, uid);
}

/* Idle handler for starting a cached query */
static gboolean
start_cached_query_cb (gpointer data)
{
	CORBA_Environment ev;
	QueryPrivate *priv;
	EComponentListener *cl;
	StartCachedQueryInfo *info = (StartCachedQueryInfo *) data;

	priv = info->query->priv;

	g_source_remove (info->tid);
	priv->cached_timeouts = g_list_remove (priv->cached_timeouts,
					       GINT_TO_POINTER (info->tid));

	/* if the query hasn't started yet, we add the listener */
	if (priv->state == QUERY_START_PENDING ||
	    priv->state == QUERY_WAIT_FOR_BACKEND) {
		priv->listeners = g_list_append (priv->listeners, info->ql);

		cl = e_component_listener_new (info->ql);
		priv->component_listeners = g_list_append (priv->component_listeners, cl);
		g_signal_connect (G_OBJECT (cl), "component_died",
				  G_CALLBACK (listener_died_cb), info->query);
	} else if (priv->state == QUERY_IN_PROGRESS) {
		/* if it's in progress, we re-add the timeout */
		info->tid = g_timeout_add (100, (GSourceFunc) start_cached_query_cb, info);
		priv->cached_timeouts = g_list_append (priv->cached_timeouts,
						       GINT_TO_POINTER (info->tid));

		return FALSE;
	} else if (priv->state == QUERY_PARSE_ERROR) {
		/* notify listener of error */
		CORBA_exception_init (&ev);
		GNOME_Evolution_Calendar_QueryListener_notifyQueryDone (
			info->ql,
			GNOME_Evolution_Calendar_QueryListener_PARSE_ERROR,
			_("Parse error"),
			&ev);

		if (BONOBO_EX (&ev))
			g_message ("start_cached_query_cb(): Could not notify the listener of "
				   "a parse error");

		CORBA_exception_free (&ev);

		/* remove all traces of this query */
		cached_queries = g_list_remove (cached_queries, info->query);
		bonobo_object_unref (BONOBO_OBJECT (info->query));
	} else if (priv->state == QUERY_DONE) {
		int len;
		GList *uid_list = NULL, *l;

		CORBA_exception_init (&ev);

		/* if the query is done, then we just notify the listener of all the
		 * UIDS we've got so far, all at once */
		g_hash_table_foreach (priv->uids, (GHFunc) add_uid_cb, &uid_list);

		len = g_list_length (uid_list);
		if (len > 0) {
			int n;
			GNOME_Evolution_Calendar_CalObjUIDSeq *corba_uids;

			corba_uids = GNOME_Evolution_Calendar_CalObjUIDSeq__alloc ();
			corba_uids->_length = len;
			corba_uids->_maximum = len;
			corba_uids->_buffer = CORBA_sequence_GNOME_Evolution_Calendar_CalObjUID_allocbuf (len);
			CORBA_sequence_set_release (corba_uids, TRUE);

			for (l = uid_list, n = 0; l != NULL; l = l->next, n++)
				corba_uids->_buffer[n] = CORBA_string_dup ((CORBA_char *) l->data);

			GNOME_Evolution_Calendar_QueryListener_notifyObjUpdated (
				info->ql,
				corba_uids,
				TRUE,
				len,
				len, &ev);

			if (BONOBO_EX (&ev))
				g_message ("start_cached_query_cb(): Could not notify the listener of all "
					   "cached components");

			CORBA_free (corba_uids);
			g_list_free (uid_list);
		}

		/* setup private data and notify listener that the query ended */
		priv->listeners = g_list_append (priv->listeners, info->ql);

		cl = e_component_listener_new (info->ql);
		priv->component_listeners = g_list_append (priv->component_listeners, cl);
		g_signal_connect (G_OBJECT (cl), "component_died",
				  G_CALLBACK (listener_died_cb), info->query);

		GNOME_Evolution_Calendar_QueryListener_notifyQueryDone (
			info->ql,
			GNOME_Evolution_Calendar_QueryListener_SUCCESS,
			"",
			&ev);
		if (BONOBO_EX (&ev))
			g_message ("start_cached_query_cb(): Could not notify the listener of "
				   "a finished query");

		CORBA_exception_free (&ev);
	}
	
	g_free (info);

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

	g_assert (priv->state == QUERY_WAIT_FOR_BACKEND);

	g_signal_handlers_disconnect_matched (G_OBJECT (priv->backend),
					      G_SIGNAL_MATCH_DATA,
					      0, 0, NULL, NULL, query);
	priv->state = QUERY_START_PENDING;

	if (status == CAL_BACKEND_OPEN_SUCCESS) {
		g_assert (cal_backend_is_loaded (backend));

		priv->timeout_id = g_timeout_add (100, (GSourceFunc) start_query_cb, query);
	}
}

/* Callback used when the backend for a cached query is destroyed */
static void
backend_destroyed_cb (gpointer data, GObject *where_backend_was)
{
	Query *query;

	query = QUERY (data);

	cached_queries = g_list_remove (cached_queries, query);
	bonobo_object_unref (BONOBO_OBJECT (query));
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
	EComponentListener *cl;

	g_return_val_if_fail (query != NULL, NULL);
	g_return_val_if_fail (IS_QUERY (query), NULL);
	g_return_val_if_fail (backend != NULL, NULL);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), NULL);
	g_return_val_if_fail (ql != CORBA_OBJECT_NIL, NULL);
	g_return_val_if_fail (sexp != NULL, NULL);

	priv = query->priv;

	CORBA_exception_init (&ev);
	priv->listeners = g_list_append (NULL, CORBA_Object_duplicate (ql, &ev));
	if (BONOBO_EX (&ev)) {
		g_message ("query_construct(): Could not duplicate the listener");
		priv->listeners = NULL;
		CORBA_exception_free (&ev);
		return NULL;
	}
	CORBA_exception_free (&ev);

	cl = e_component_listener_new (ql);
	priv->component_listeners = g_list_append (priv->component_listeners, cl);
	g_signal_connect (G_OBJECT (cl), "component_died",
			  G_CALLBACK (listener_died_cb), query);

	priv->backend = backend;
	g_object_ref (priv->backend);

	priv->qb = query_backend_new (query, backend);
	priv->default_zone = cal_backend_get_default_timezone (backend);

	priv->sexp = g_strdup (sexp);

	/* Queue the query to be started asynchronously */

	if (cal_backend_is_loaded (priv->backend)) {
		priv->state = QUERY_START_PENDING;

		priv->timeout_id = g_timeout_add (100, (GSourceFunc) start_query_cb, query);
	} else
		g_signal_connect (G_OBJECT (priv->backend), "opened",
				  G_CALLBACK (backend_opened_cb),
				  query);

	return query;
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
	   const char *sexp)
{
	Query *query;
	GList *l;

	/* first, see if we've got this query in our cache */
	for (l = cached_queries; l != NULL; l = l->next) {
		query = QUERY (l->data);

		g_assert (query != NULL);

		if (query->priv->backend == backend &&
		    !strcmp (query->priv->sexp, sexp)) {
			StartCachedQueryInfo *info;
			CORBA_Environment ev;

			info = g_new0 (StartCachedQueryInfo, 1);
			info->query = query;

			CORBA_exception_init (&ev);
			info->ql = CORBA_Object_duplicate (ql, &ev);
			if (BONOBO_EX (&ev)) {
				g_message ("query_new(): Could not duplicate listener object");
				g_free (info);

				return NULL;
			}
			CORBA_exception_free (&ev);

			info->tid = g_timeout_add (100, (GSourceFunc) start_cached_query_cb, info);
			query->priv->cached_timeouts = g_list_append (query->priv->cached_timeouts,
								      GINT_TO_POINTER (info->tid));

			bonobo_object_ref (BONOBO_OBJECT (query));
			return query;
		}
	}

	/* not found, so create a new one */
	query = QUERY (g_object_new (QUERY_TYPE, NULL));
	if (!query_construct (query, backend, ql, sexp)) {
		bonobo_object_unref (BONOBO_OBJECT (query));
		return NULL;
	}

	/* add the new query to our cache */
	g_object_weak_ref (G_OBJECT (query->priv->backend),
			   backend_destroyed_cb, query);

	bonobo_object_ref (BONOBO_OBJECT (query));
	cached_queries = g_list_append (cached_queries, query);

	return query;
}
