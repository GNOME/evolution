/* Evolution calendar - Backend cache for calendar queries.
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Author: Rodrigo Moya <rodrigo@ximian.com>
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
#include <libgnome/gnome-i18n.h>
#include <cal-util/cal-component.h>
#include "query.h"
#include "query-backend.h"

static void query_backend_class_init (QueryBackendClass *klass);
static void query_backend_init       (QueryBackend *qb, QueryBackendClass *klass);
static void query_backend_finalize   (GObject *object);

typedef struct {
	CalComponent *comp;
} QueryBackendComponent;

/* Private part of the QueryBackend structure */
struct _QueryBackendPrivate {
	char *uri;
	CalBackend *backend;
	GHashTable *components;
	GList *queries;
};

static GHashTable *loaded_backends = NULL;
static GObjectClass *parent_class = NULL;

/* Class initialization function for the backend cache */
static void
query_backend_class_init (QueryBackendClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = query_backend_finalize;
}

/* Object initialization function for the backend cache */
static void
query_backend_init (QueryBackend *qb, QueryBackendClass *klass)
{
	QueryBackendPrivate *priv;

	priv = g_new0 (QueryBackendPrivate, 1);
	qb->priv = priv;

	priv->uri = NULL;
	priv->backend = NULL;
	priv->components = g_hash_table_new (g_str_hash, g_str_equal);
	priv->queries = NULL;
}

static void
free_hash_comp_cb (gpointer key, gpointer value, gpointer user_data)
{
	g_free (key);
	g_object_unref (value);
}

/* Finalize handler for the backend cache */
static void
query_backend_finalize (GObject *object)
{
	QueryBackend *qb = (QueryBackend *) object;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_QUERY_BACKEND (qb));

	/* remove the QueryBackend from the internal hash table */
	g_hash_table_remove (loaded_backends, qb->priv->uri);
	if (g_hash_table_size (loaded_backends) == 0) {
		g_hash_table_destroy (loaded_backends);
		loaded_backends = NULL;
	}

	/* free memory */
	qb->priv->backend = NULL;

	g_free (qb->priv->uri);
	qb->priv->uri = NULL;

	g_hash_table_foreach (qb->priv->components, (GHFunc) free_hash_comp_cb, NULL);
	g_hash_table_destroy (qb->priv->components);
	qb->priv->components = NULL;

	g_list_free (qb->priv->queries);
	qb->priv->queries = NULL;

	g_free (qb->priv);
	qb->priv = NULL;

	if (G_OBJECT_CLASS (parent_class)->finalize)
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

/**
 * query_backend_get_type:
 * @void:
 *
 * Registers the #QueryBackend class if necessary, and returns the type ID
 * associated to it.
 *
 * Return value: The type ID of the #QueryBackend class.
 **/
GType
query_backend_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static GTypeInfo info = {
                        sizeof (QueryBackendClass),
                        (GBaseInitFunc) NULL,
                        (GBaseFinalizeFunc) NULL,
                        (GClassInitFunc) query_backend_class_init,
                        NULL, NULL,
                        sizeof (QueryBackend),
                        0,
                        (GInstanceInitFunc) query_backend_init
                };
		type = g_type_register_static (G_TYPE_OBJECT, "QueryBackend", &info, 0);
	}

	return type;
}

static void
backend_destroyed_cb (GObject *object, gpointer user_data)
{
	CalBackend *backend = (CalBackend *) object;
	QueryBackend *qb = (QueryBackend *) user_data;

	g_return_if_fail (IS_CAL_BACKEND (backend));
	g_return_if_fail (IS_QUERY_BACKEND (qb));

	g_object_unref (qb);
}

static void
object_updated_cb (CalBackend *backend, const char *uid, gpointer user_data)
{
	gpointer orig_key, orig_value;
	const char *tmp_uid;
	CalComponent *comp;
	icalcomponent *icalcomp;
	char *comp_str;
	QueryBackend *qb = (QueryBackend *) user_data;

	g_return_if_fail (IS_QUERY_BACKEND (qb));

	if (g_hash_table_lookup_extended (qb->priv->components, uid, &orig_key, &orig_value)) {
		g_hash_table_remove (qb->priv->components, uid);
		g_free (orig_key);
		g_object_unref (orig_value);
	}

	comp_str = cal_backend_get_object (qb->priv->backend, uid);
	if (!comp_str)
		return;

	icalcomp = icalparser_parse_string (comp_str);
	g_free (comp_str);
	if (icalcomp) {
		comp = cal_component_new ();
		if (!cal_component_set_icalcomponent (comp, icalcomp)) {
			icalcomponent_free (icalcomp);
			g_object_unref (comp);
			return;
		}

		cal_component_get_uid (comp, &tmp_uid);
		if (!uid || !*uid) {
			g_object_unref (comp);
		} else
			g_hash_table_insert (qb->priv->components, g_strdup (tmp_uid), comp);
	}
}

static void
object_removed_cb (CalBackend *backend, const char *uid, gpointer user_data)
{
	gpointer orig_key, orig_value;
	QueryBackend *qb = (QueryBackend *) user_data;

	g_return_if_fail (IS_QUERY_BACKEND (qb));

	if (g_hash_table_lookup_extended (qb->priv->components, uid, &orig_key, &orig_value)) {
		g_hash_table_remove (qb->priv->components, uid);
		g_free (orig_key);
		g_object_unref (orig_value);
	}
}

static void
query_destroyed_cb (GObject *object, gpointer user_data)
{
	Query *query = (Query *) object;
	QueryBackend *qb = (QueryBackend *) user_data;

	g_return_if_fail (IS_QUERY (query));
	g_return_if_fail (IS_QUERY_BACKEND (qb));

	qb->priv->queries = g_list_remove (qb->priv->queries, query);
}

static void
foreach_uid_cb (gpointer data, gpointer user_data)
{
	QueryBackend *qb = (QueryBackend *) user_data;

	g_return_if_fail (data != NULL);
	g_return_if_fail (IS_QUERY_BACKEND (qb));

	object_updated_cb (qb->priv->backend, (const char *) data, qb);
}

/**
 * query_backend_new
 * @query: The #Query object that issues the query.
 * @backend: A #CalBackend object.
 *
 * Create a new #QueryBackend instance, which is a class to
 * have a cache of objects for the calendar queries, so that
 * we don't have to ask the calendar backend to get the objects
 * everytime.
 *
 * Returns: the newly-created object.
 */
QueryBackend *
query_backend_new (Query *query, CalBackend *backend)
{
	QueryBackend *qb = NULL;

	g_return_val_if_fail (IS_QUERY (query), NULL);
	g_return_val_if_fail (IS_CAL_BACKEND (backend), NULL);

	if (!loaded_backends)
		loaded_backends = g_hash_table_new (g_str_hash, g_str_equal);

	/* see if we already have the backend loaded */
	qb = g_hash_table_lookup (loaded_backends,
				  cal_backend_get_uri (backend));
	if (!qb) {
		GList *uidlist;

		qb = g_object_new (QUERY_BACKEND_TYPE, NULL);

		qb->priv->uri = g_strdup (cal_backend_get_uri (backend));
		qb->priv->backend = backend;

		/* load all UIDs */
		uidlist = cal_backend_get_uids (backend, CALOBJ_TYPE_ANY);
		g_list_foreach (uidlist, foreach_uid_cb, qb);
		cal_obj_uid_list_free (uidlist);

		g_signal_connect (G_OBJECT (backend), "destroy",
				  G_CALLBACK (backend_destroyed_cb), qb);
		g_signal_connect (G_OBJECT (backend), "obj_updated",
				  G_CALLBACK (object_updated_cb), qb);
		g_signal_connect (G_OBJECT (backend), "obj_removed",
				  G_CALLBACK (object_removed_cb), qb);

		g_hash_table_insert (loaded_backends, qb->priv->uri, qb);
	}

	qb->priv->queries = g_list_append (qb->priv->queries, query);
	g_signal_connect (G_OBJECT (query), "destroy",
			  G_CALLBACK (query_destroyed_cb), qb);

	return qb;
}

typedef struct {
	GList *uidlist;
	CalObjType type;
} GetUidsData;

static void
uid_hash_cb (gpointer key, gpointer value, gpointer user_data)
{
	CalComponentVType vtype;
	char *uid = (char *) key;
	CalComponent *comp = (CalComponent *) value;
	GetUidsData *uids_data = (GetUidsData *) user_data;

	g_return_if_fail (uid != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));
	g_return_if_fail (uids_data != NULL);

	vtype = cal_component_get_vtype (comp);
	if (vtype == CAL_COMPONENT_EVENT && uids_data->type == CALOBJ_TYPE_EVENT)
		uids_data->uidlist = g_list_append (uids_data->uidlist, g_strdup (uid));
	else if (vtype == CAL_COMPONENT_TODO && uids_data->type == CALOBJ_TYPE_TODO)
		uids_data->uidlist = g_list_append (uids_data->uidlist, g_strdup (uid));
	else if (vtype == CAL_COMPONENT_JOURNAL && uids_data->type == CALOBJ_TYPE_JOURNAL)
		uids_data->uidlist = g_list_append (uids_data->uidlist, g_strdup (uid));
	else if (uids_data->type == CALOBJ_TYPE_ANY)
		uids_data->uidlist = g_list_append (uids_data->uidlist, g_strdup (uid));
}

/**
 * query_backend_get_uids
 * @qb: A #QueryBackend type.
 * @type: Type of objects to get the UIDs for.
 *
 * Get a list of all UIDs for objects of the given type out from
 * the specified #QueryBackend object.
 *
 * Returns: a GList of UIDs, which should be freed, when no longer needed,
 * via a call to cal_obj_uid_list_free.
 */
GList *
query_backend_get_uids (QueryBackend *qb, CalObjType type)
{
	GetUidsData uids_data;

	g_return_val_if_fail (IS_QUERY_BACKEND (qb), NULL);

	uids_data.uidlist = NULL;
	uids_data.type = type;
	g_hash_table_foreach (qb->priv->components, (GHFunc) uid_hash_cb, &uids_data);

	return uids_data.uidlist;
}

/**
 * query_backend_get_object_component
 * @qb: A #QueryBackend object.
 * @uid: UID of the object to retrieve.
 *
 * Get a #CalComponent from the given #QueryBackend.
 *
 * Returns: the component if found, NULL otherwise.
 */
CalComponent *
query_backend_get_object_component (QueryBackend *qb, const char *uid)
{
	g_return_val_if_fail (IS_QUERY_BACKEND (qb), NULL);
	g_return_val_if_fail (uid != NULL, NULL);

	return g_hash_table_lookup (qb->priv->components, uid);
}
