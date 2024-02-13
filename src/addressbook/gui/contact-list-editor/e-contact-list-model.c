/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <string.h>

#include "e-contact-list-model.h"
#include "shell/e-shell.h"

struct _EContactListModelPrivate {
	GHashTable *uids_table;
	GHashTable *emails_table;
};

G_DEFINE_TYPE_WITH_PRIVATE (EContactListModel, e_contact_list_model, GTK_TYPE_TREE_STORE);

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
	object = G_OBJECT_CLASS (e_contact_list_model_parent_class)->
		constructor (type, n_construct_properties, construct_properties);

	gtk_tree_store_set_column_types (
		GTK_TREE_STORE (object), G_N_ELEMENTS (types), types);

	return object;
}

static void
contact_list_model_dispose (GObject *object)
{
	EContactListModelPrivate *priv = E_CONTACT_LIST_MODEL (object)->priv;

	g_clear_pointer (&priv->uids_table, g_hash_table_destroy);
	g_clear_pointer (&priv->emails_table, g_hash_table_destroy);

	G_OBJECT_CLASS (e_contact_list_model_parent_class)->dispose (object);
}

static void
e_contact_list_model_class_init (EContactListModelClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructor = contact_list_model_constructor;
	object_class->dispose = contact_list_model_dispose;
}

static void
e_contact_list_model_init (EContactListModel *model)
{
	model->priv = e_contact_list_model_get_instance_private (model);

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
                              const gchar *uid)
{
	return (g_hash_table_lookup (model->priv->uids_table, uid) != NULL);
}

GtkTreePath *
e_contact_list_model_add_destination (EContactListModel *model,
                                      EDestination *destination,
                                      GtkTreeIter *parent,
                                      gboolean ignore_conflicts)
{
	GtkTreeIter iter;
	GtkTreePath *path = NULL;

	g_return_val_if_fail (E_IS_CONTACT_LIST_MODEL (model), NULL);
	g_return_val_if_fail (E_IS_DESTINATION (destination), NULL);

	if (e_destination_is_evolution_list (destination)) {
		const GList *dest, *dests = e_destination_list_get_root_dests (destination);
		/* Get number of instances of this list in the model */
		gint list_refs = GPOINTER_TO_INT (
			g_hash_table_lookup (model->priv->uids_table,
			e_destination_get_contact_uid (destination)));

		gtk_tree_store_append (GTK_TREE_STORE (model), &iter, parent);
		gtk_tree_store_set (GTK_TREE_STORE (model), &iter, 0, destination, -1);

		for (dest = dests; dest; dest = dest->next) {
			path = e_contact_list_model_add_destination (model, dest->data, &iter, ignore_conflicts);
			if (dest->next && path) {
				gtk_tree_path_free (path);
				path = NULL;
			}
		}

		/* When the list has no children the remove it. We don't want empty sublists displayed. */
		if (!gtk_tree_model_iter_has_child (GTK_TREE_MODEL (model), &iter)) {
			gtk_tree_store_remove (GTK_TREE_STORE (model), &iter);
		} else {
			g_hash_table_insert (
				model->priv->uids_table,
				g_strdup (e_destination_get_contact_uid (destination)),
				GINT_TO_POINTER (list_refs + 1));
		}
	} else {
		gint dest_refs;

		if (e_contact_list_model_has_email (model, e_destination_get_email (destination)) &&
		    ignore_conflicts == FALSE) {
			return NULL;
		}

		dest_refs = GPOINTER_TO_INT (
			g_hash_table_lookup (model->priv->emails_table,
			e_destination_get_email (destination)));

		g_hash_table_insert (
			model->priv->emails_table,
			g_strdup (e_destination_get_email (destination)),
			GINT_TO_POINTER (dest_refs + 1));

		gtk_tree_store_append (GTK_TREE_STORE (model), &iter, parent);
		gtk_tree_store_set (GTK_TREE_STORE (model), &iter, 0, destination, -1);

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
	e_contact_list_model_add_destination (model, destination, NULL, TRUE);
}

static void
contact_list_model_unref_row_dest (EContactListModel *model,
                                   GtkTreeIter *iter)
{
	EDestination *dest;
	GtkTreeModel *tree_model;

	tree_model = GTK_TREE_MODEL (model);
	gtk_tree_model_get (tree_model, iter, 0, &dest, -1);

	if (gtk_tree_model_iter_has_child (tree_model, iter)) {
		GtkTreeIter child_iter;
		gint list_refs = GPOINTER_TO_INT (
			g_hash_table_lookup (model->priv->uids_table,
			e_destination_get_contact_uid (dest)));

		/* If the list is only once in the model, then remove it from the hash table,
		 * otherwise decrease the counter by one */
		if (list_refs <= 1) {
			g_hash_table_remove (
				model->priv->uids_table,
				e_destination_get_contact_uid (dest));
		} else {
			g_hash_table_insert (
				model->priv->uids_table,
				g_strdup (e_destination_get_contact_uid (dest)),
				GINT_TO_POINTER (list_refs - 1));
		}

		if (gtk_tree_model_iter_children (tree_model, &child_iter, iter)) {
			do {
				contact_list_model_unref_row_dest (model, &child_iter);
			} while (gtk_tree_model_iter_next (tree_model, &child_iter));
		}

	} else {
		gint dest_refs = GPOINTER_TO_INT (
			g_hash_table_lookup (
				model->priv->emails_table,
				e_destination_get_email (dest)));

		if (dest_refs <= 1) {
			g_hash_table_remove (
				model->priv->emails_table,
				e_destination_get_email (dest));
		} else {
			g_hash_table_insert (
				model->priv->emails_table,
				g_strdup (e_destination_get_email (dest)),
				GINT_TO_POINTER (dest_refs - 1));
		}
	}

	g_object_unref (dest);
}

void
e_contact_list_model_remove_row (EContactListModel *model,
                                 GtkTreeIter *iter)
{
	GtkTreeIter parent_iter;

	g_return_if_fail (E_IS_CONTACT_LIST_MODEL (model));
	g_return_if_fail (iter);

	/* Use helper function to update our reference counters in
	 * hash tables but don't remove any row. */
	contact_list_model_unref_row_dest (model, iter);

	/* Get iter of parent of the row to be removed. After the row is removed, check if there are
	 * any more children left for the parent_iter, an eventually remove the parent_iter as well */
	if (gtk_tree_model_iter_parent (GTK_TREE_MODEL (model), &parent_iter, iter)) {
		gtk_tree_store_remove (GTK_TREE_STORE (model), iter);
		if (!gtk_tree_model_iter_has_child (GTK_TREE_MODEL (model), &parent_iter)) {
			contact_list_model_unref_row_dest (model, &parent_iter);
			gtk_tree_store_remove (GTK_TREE_STORE (model), &parent_iter);
		}
	} else {
		gtk_tree_store_remove (GTK_TREE_STORE (model), iter);
	}
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
