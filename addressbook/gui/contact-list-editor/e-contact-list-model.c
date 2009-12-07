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

#include <config.h>
#include <string.h>

#include "e-contact-list-model.h"
#include "e-util/e-alert-dialog.h"
#include "shell/e-shell.h"

static gpointer parent_class;

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

	gtk_list_store_set_column_types (
		GTK_LIST_STORE (object), G_N_ELEMENTS (types), types);

	return object;
}

static void
contact_list_model_class_init (EContactListModelClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);

	object_class = G_OBJECT_CLASS (class);
	object_class->constructor = contact_list_model_constructor;
}

GType
e_contact_list_model_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
		type = g_type_register_static_simple (
			GTK_TYPE_LIST_STORE,
			"EContactListModel",
			sizeof (EContactListModelClass),
			(GClassInitFunc) contact_list_model_class_init,
			sizeof (EContactListModel),
			(GInstanceInitFunc) NULL, 0);

	return type;
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
	GtkTreeIter iter;
	gboolean iter_valid;
	gboolean has_email = FALSE;

	g_return_val_if_fail (E_IS_CONTACT_LIST_MODEL (model), FALSE);
	g_return_val_if_fail (email != NULL, FALSE);

	iter_valid = gtk_tree_model_get_iter_first (
		GTK_TREE_MODEL (model), &iter);

	while (!has_email && iter_valid) {
		EDestination *destination;
		const gchar *textrep;

		gtk_tree_model_get (
			GTK_TREE_MODEL (model), &iter, 0, &destination, -1);
		textrep = e_destination_get_textrep (destination, TRUE);
		has_email = (strcmp (email, textrep) == 0);
		g_object_unref (destination);

		iter_valid = gtk_tree_model_iter_next (
			GTK_TREE_MODEL (model), &iter);
	}

	return has_email;
}

void
e_contact_list_model_add_destination (EContactListModel *model,
                                      EDestination *destination)
{
	GtkTreeIter iter;

	g_return_if_fail (E_IS_CONTACT_LIST_MODEL (model));
	g_return_if_fail (E_IS_DESTINATION (destination));

	gtk_list_store_append (GTK_LIST_STORE (model), &iter);
	gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, destination, -1);
}

void
e_contact_list_model_add_email (EContactListModel *model,
                                const gchar *email)
{
	const gchar *tag = "addressbook:ask-list-add-exists";
	EDestination *destination;

	g_return_if_fail (E_IS_CONTACT_LIST_MODEL (model));
	g_return_if_fail (email != NULL);

	if (e_contact_list_model_has_email (model, email))
		if (e_alert_run_dialog_for_args (e_shell_get_active_window
						 (NULL), tag, email, NULL) != GTK_RESPONSE_YES)
			return;

	destination = e_destination_new ();
	e_destination_set_email (destination, email);
	e_contact_list_model_add_destination (model, destination);
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
	e_contact_list_model_add_destination (model, destination);
}

void
e_contact_list_model_remove_row (EContactListModel *model,
                                 gint row)
{
	GtkTreeIter iter;
	gboolean iter_valid;

	g_return_if_fail (E_IS_CONTACT_LIST_MODEL (model));

	iter_valid = contact_list_get_iter (model, &iter, row);
	g_return_if_fail (iter_valid);

	gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
}

void
e_contact_list_model_remove_all (EContactListModel *model)
{
	g_return_if_fail (E_IS_CONTACT_LIST_MODEL (model));

	gtk_list_store_clear (GTK_LIST_STORE (model));
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
