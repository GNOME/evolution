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
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <gtk/gtksignal.h>
#include <cal-util/cal-component.h>
#include "query.h"
#include "query-backend.h"

static void query_backend_class_init (QueryBackendClass *klass);
static void query_backend_init       (QueryBackend *qb);
static void query_backend_destroy    (GtkObject *object);

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
static GtkObjectClass *parent_class = NULL;

/* Class initialization function for the backend cache */
static void
query_backend_class_init (QueryBackendClass *klass)
{
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);

	parent_class = gtk_type_class (GTK_TYPE_OBJECT);

	object_class->destroy = query_backend_destroy;
}

/* Object initialization function for the backend cache */
static void
query_backend_init (QueryBackend *qb)
{
	QueryBackendPrivate *priv;

	priv = g_new0 (QueryBackendPrivate, 1);
	qb->priv = priv;

	priv->uri = NULL;
	priv->backend = NULL;
	priv->components = g_hash_table_new (g_str_hash, g_str_equal);
	priv->queries = NULL;
}

/* Destroy handler for the backend cache */
static void
query_backend_destroy (GtkObject *object)
{
	QueryBackend *qb = (QueryBackend *) object;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_QUERY_BACKEND (qb));

	/* remove the QueryBackend from the internal hash table */
	g_hash_table_remove (loaded_backends, qb->priv->uri);

	/* free memory */
	qb->priv->backend = NULL;

	g_free (qb->priv->uri);
	qb->priv->uri = NULL;

	g_hash_table_destroy (qb->priv->components);
	qb->priv->components = NULL;

	g_list_free (qb->priv->queries);
	qb->priv->queries = NULL;

	g_free (qb->priv);
	qb->priv = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
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
GtkType
query_backend_get_type (void)
{
	static GtkType type = 0;

	if (!type) {
		static const GtkTypeInfo info = {
			"QueryBackend",
			sizeof (QueryBackend),
			sizeof (QueryBackendClass),
			(GtkClassInitFunc) query_backend_class_init,
			(GtkObjectInitFunc) query_backend_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (GTK_TYPE_OBJECT, &info);
	}

	return type;
}

static void
backend_destroyed_cb (GtkObject *object, gpointer user_data)
{
	CalBackend *backend = (CalBackend *) object;
	QueryBackend *qb = (QueryBackend *) user_data;

	g_return_if_fail (IS_CAL_BACKEND (backend));
	g_return_if_fail (IS_QUERY_BACKEND (qb));

	gtk_object_unref (GTK_OBJECT (qb));
}

static void
object_updated_cb (CalBackend *backend, const char *uid, gpointer user_data)
{
	gpointer orig_key, orig_value;
	char *tmp_uid;
	CalComponent *comp;
	QueryBackend *qb = (QueryBackend *) user_data;

	g_return_if_fail (IS_QUERY_BACKEND (qb));

	if (g_hash_table_lookup_extended (qb->priv->components, uid, &orig_key, &orig_value)) {
		g_hash_table_remove (qb->priv->components, uid);
	}

	comp = cal_backend_get_object_component (qb->priv->backend, uid);
	if (IS_CAL_COMPONENT (comp)) {
		cal_component_get_uid (comp, &tmp_uid);
		g_hash_table_insert (qb->priv->components, tmp_uid, comp);
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
	}
}

static void
query_destroyed_cb (GtkObject *object, gpointer user_data)
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
	CalComponent *comp;
	char *uid;
	QueryBackend *qb = (QueryBackend *) user_data;

	g_return_if_fail (data != NULL);
	g_return_if_fail (IS_QUERY_BACKEND (qb));

	comp = cal_backend_get_object_component (qb->priv->backend, (const char *) data);
	if (IS_CAL_COMPONENT (comp)) {
		cal_component_get_uid (comp, &uid);
		g_hash_table_insert (qb->priv->components, uid, comp);
	}
	else
		g_warning (_("Could not get component with UID = %s"), (const char *) data);
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

	/* see if we already have the backend loaded */
	qb = g_hash_table_lookup (loaded_backends,
				  cal_backend_get_uri (backend));
	if (!qb) {
		GList *uidlist;

		qb = gtk_type_new (QUERY_BACKEND_TYPE);

		qb->priv->uri = g_strdup (cal_backend_get_uri (backend));
		qb->priv->backend = backend;

		/* load all UIDs */
		uidlist = cal_backend_get_uids (backend, CALOBJ_TYPE_ANY);
		g_list_foreach (uidlist, foreach_uid_cb, qb);
		cal_obj_uid_list_free (uidlist);

		gtk_signal_connect (GTK_OBJECT (backend), "destroy",
				    GTK_SIGNAL_FUNC (backend_destroyed_cb), qb);
		gtk_signal_connect (GTK_OBJECT (backend), "obj_updated",
				    GTK_SIGNAL_FUNC (object_updated_cb), qb);
		gtk_signal_connect (GTK_OBJECT (backend), "obj_removed",
				    GTK_SIGNAL_FUNC (object_removed_cb), qb);

		g_hash_table_insert (loaded_backends, qb->priv->uri, qb);
	}

	qb->priv->queries = g_list_append (qb->priv->queries, query);
	gtk_signal_connect (GTK_OBJECT (query), "destroy",
			    GTK_SIGNAL_FUNC (query_destroyed_cb), qb);

	return qb;
}
