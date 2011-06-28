/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "e-contact-list-model.h"
#include "e-util/e-alert-dialog.h"
#include "shell/e-shell.h"

static gpointer parent_class;

G_DEFINE_TYPE (EContactListModel, e_contact_list_model, GTK_TYPE_TREE_STORE);

struct _EContactListModelPrivate {

	GHashTable *uids_table;
	GHashTable *emails_table;

};

static gboolean
contact_list_get_iter (EContactListModel *model,
                       GtkTreeIter *iter,
                       gint row)
{
	GtkTreePath *path;
	gboolean iter_valid;

	path = gtk_tree_path_new_from_indices (row, -1);
	iter_valid = gtk_tree_model_get_iter (
		GTK_TREE_MODEL (model), iter, path);
	gtk_tree_path_free (path);

	return iter_valid;
}

static GObject *
contact_list_model_constructor (GType type,
                                guint n_construct_properties,
                                GObjectConstructParam *construct_properties)
{
	GObject *object;
	GType types[1];

	types[0] = E_TYPE_DESTINATION;

	/* Chain up to parent's constructor() method. */
	object = G_OBJECT_CLASS (parent_class)->constructor (
		type, n_construct_properties, construct_properties);

	gtk_tree_store_set_column_types (
		GTK_TREE_STORE (object), G_N_ELEMENTS (types), types);

	return object;
}

static void
contact_list_model_dispose (GObject *object)
{
	EContactListModelPrivate *priv = E_CONTACT_LIST_MODEL (object)->priv;

	if (priv->uids_table) {
		g_hash_table_unref (priv->uids_table);
		priv->uids_table = NULL;
	}

	if (priv->emails_table) {
		g_hash_table_unref (priv->emails_table);
		priv->emails_table = NULL;
	}

	G_OBJECT_CLASS (e_contact_list_model_parent_class)->dispose (object);
}

static void
e_contact_list_model_class_init (EContactListModelClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EContactListModelPrivate));

	parent_class = g_type_class_peek_parent (class);

	object_class = G_OBJECT_CLASS (class);
	object_class->constructor = contact_list_model_constructor;
	object_class->dispose = contact_list_model_dispose;
}

static void
e_contact_list_model_init (EContactListModel *model)
{
	model->priv = G_TYPE_INSTANCE_GET_PRIVATE (model, E_TYPE_CONTACT_LIST_MODEL, EContactListModelPrivate);

	model->priv->uids_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	model->priv->emails_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
}

GtkTreeModel *
e_contact_list_model_new (void)
{
	return g_object_new (E_TYPE_CONTACT_LIST_MODEL, NULL);
}

gboolean
e_contact_list_model_has_email (EContactListModel *model,
                                const gchar *email)
{
	return (g_hash_table_lookup (model->priv->emails_table, email) != NULL);
}

gboolean
e_contact_list_model_has_uid (EContactListModel *model,
			      const gchar* uid)
{
	return (g_hash_table_lookup (model->priv->uids_table, uid) != NULL);
}

GtkTreePath*
e_contact_list_model_add_destination (EContactListModel *model,
                                      EDestination *destination,
                                      GtkTreeIter *parent)
{
	GtkTreeIter iter;
	GtkTreePath *path;

	g_return_val_if_fail (E_IS_CONTACT_LIST_MODEL (model), NULL);
	g_return_val_if_fail (E_IS_DESTINATION (destination), NULL);

	gtk_tree_store_append (GTK_TREE_STORE (model), &iter, parent);
	gtk_tree_store_set (GTK_TREE_STORE (model), &iter, 0, destination, -1);

	if (e_destination_is_evolution_list (destination)) {
		const GList *dest, *dests = e_destination_list_get_root_dests (destination);

		g_hash_table_insert (model->priv->uids_table,
			g_strdup (e_destination_get_contact_uid (destination)),
			destination);

		for (dest = dests; dest; dest = dest->next) {
			path = e_contact_list_model_add_destination (model, dest->data, &iter);
			if (dest->next)
				gtk_tree_path_free (path);
		}
	} else {
		g_hash_table_insert (model->priv->emails_table,
			g_strdup (e_destination_get_email (destination)),
			destination);

		path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
	}

	return path;
}

void
e_contact_list_model_add_contact (EContactListModel *model,
                                  EContact *contact,
                                  gint email_num)
{
	EDestination *destination;

	g_return_if_fail (E_IS_CONTACT_LIST_MODEL (model));
	g_return_if_fail (E_IS_CONTACT (contact));

	destination = e_destination_new ();
	e_destination_set_contact (destination, contact, email_num);
	e_contact_list_model_add_destination (model, destination, NULL);
}

void
e_contact_list_model_remove_row (EContactListModel *model,
                                 GtkTreeIter *iter)
{
	EDestination *dest;

	g_return_if_fail (E_IS_CONTACT_LIST_MODEL (model));
	g_return_if_fail (iter);

	gtk_tree_model_get (GTK_TREE_MODEL (model), iter, 0, &dest, -1);

	if (e_destination_is_evolution_list (dest)) {
		const gchar *uid = e_destination_get_contact_uid (dest);
		g_hash_table_remove (model->priv->uids_table, uid);
	} else {
		const gchar *email = e_destination_get_email (dest);
		g_hash_table_remove (model->priv->emails_table, email);
	}

	g_object_unref (dest);
	gtk_tree_store_remove (GTK_TREE_STORE (model), iter);
}

void
e_contact_list_model_remove_all (EContactListModel *model)
{
	g_return_if_fail (E_IS_CONTACT_LIST_MODEL (model));

	g_hash_table_remove_all (model->priv->uids_table);
	g_hash_table_remove_all (model->priv->emails_table);

	gtk_tree_store_clear (GTK_TREE_STORE (model));
}

EDestination *
e_contact_list_model_get_destination (EContactListModel *model,
                                      gint row)
{
	EDestination *destination;
	GtkTreeIter iter;
	gboolean iter_valid;

	g_return_val_if_fail (E_IS_CONTACT_LIST_MODEL (model), NULL);

	iter_valid = contact_list_get_iter (model, &iter, row);
	g_return_val_if_fail (iter_valid, NULL);

	gtk_tree_model_get (
		GTK_TREE_MODEL (model), &iter, 0, &destination, -1);

	return destination;
}
