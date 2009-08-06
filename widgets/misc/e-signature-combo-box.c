/*
 *
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-signature-combo-box.h"

#include <glib/gi18n-lib.h>

#define E_SIGNATURE_COMBO_BOX_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SIGNATURE_COMBO_BOX, ESignatureComboBoxPrivate))

enum {
	COLUMN_STRING,
	COLUMN_SIGNATURE
};

enum {
	REFRESHED,
	LAST_SIGNAL
};

struct _ESignatureComboBoxPrivate {
	ESignatureList *signature_list;
	GHashTable *index;
};

static gpointer parent_class;
static guint signal_ids[LAST_SIGNAL];

static void
signature_combo_box_refresh_cb (ESignatureList *signature_list,
                                ESignature *unused,
                                ESignatureComboBox *combo_box)
{
	GtkListStore *store;
	GtkTreeModel *model;
	GtkTreeIter tree_iter;
	EIterator *signature_iter;
	ESignature *signature;
	GHashTable *index;
	GList *list = NULL;
	GList *iter;

	store = gtk_list_store_new (2, G_TYPE_STRING, E_TYPE_SIGNATURE);
	model = GTK_TREE_MODEL (store);
	index = combo_box->priv->index;

	g_hash_table_remove_all (index);

	gtk_list_store_append (store, &tree_iter);
	gtk_list_store_set (
		store, &tree_iter,
		COLUMN_STRING, _("None"),
		COLUMN_SIGNATURE, NULL, -1);

	if (signature_list == NULL)
		goto skip;

	/* Build a list of ESignatures to display. */
	signature_iter = e_list_get_iterator (E_LIST (signature_list));
	while (e_iterator_is_valid (signature_iter)) {

		/* XXX EIterator misuses const. */
		signature = (ESignature *) e_iterator_get (signature_iter);
		list = g_list_prepend (list, signature);
		e_iterator_next (signature_iter);
	}
	g_object_unref (signature_iter);

	list = g_list_reverse (list);

	/* Populate the list store and index. */
	for (iter = list; iter != NULL; iter = iter->next) {
		GtkTreeRowReference *reference;
		GtkTreePath *path;
		const gchar *string;

		signature = iter->data;
		string = e_signature_get_name (signature);

		gtk_list_store_append (store, &tree_iter);
		gtk_list_store_set (
			store, &tree_iter,
			COLUMN_STRING, string,
			COLUMN_SIGNATURE, signature, -1);

		path = gtk_tree_model_get_path (model, &tree_iter);
		reference = gtk_tree_row_reference_new (model, path);
		g_hash_table_insert (index, signature, reference);
		gtk_tree_path_free (path);
	}

skip:
	/* Restore the previously selected signature. */
	signature = e_signature_combo_box_get_active (combo_box);
	if (signature != NULL)
		g_object_ref (signature);
	gtk_combo_box_set_model (GTK_COMBO_BOX (combo_box), model);
	e_signature_combo_box_set_active (combo_box, signature);
	if (signature != NULL)
		g_object_unref (signature);

	g_signal_emit (combo_box, signal_ids[REFRESHED], 0);
}

static GObject *
signature_combo_box_constructor (GType type,
                                 guint n_construct_properties,
                                 GObjectConstructParam *construct_properties)
{
	GObject *object;
	GtkCellRenderer *renderer;

	/* Chain up to parent's constructor() method. */
	object = G_OBJECT_CLASS (parent_class)->constructor (
		type, n_construct_properties, construct_properties);

	renderer = gtk_cell_renderer_text_new ();

	gtk_cell_layout_pack_start (
		GTK_CELL_LAYOUT (object), renderer, TRUE);
	gtk_cell_layout_add_attribute (
		GTK_CELL_LAYOUT (object), renderer, "text", COLUMN_STRING);

	e_signature_combo_box_set_signature_list (
		E_SIGNATURE_COMBO_BOX (object), NULL);

	return object;
}

static void
signature_combo_box_dispose (GObject *object)
{
	ESignatureComboBoxPrivate *priv;

	priv = E_SIGNATURE_COMBO_BOX_GET_PRIVATE (object);

	if (priv->signature_list != NULL) {
		g_signal_handlers_disconnect_by_func (
			priv->signature_list,
			signature_combo_box_refresh_cb, object);
		g_object_unref (priv->signature_list);
		priv->signature_list = NULL;
	}

	g_hash_table_remove_all (priv->index);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
signature_combo_box_finalize (GObject *object)
{
	ESignatureComboBoxPrivate *priv;

	priv = E_SIGNATURE_COMBO_BOX_GET_PRIVATE (object);

	g_hash_table_destroy (priv->index);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
signature_combo_box_class_init (ESignatureComboBoxClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (ESignatureComboBoxPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->constructor = signature_combo_box_constructor;
	object_class->dispose = signature_combo_box_dispose;
	object_class->finalize = signature_combo_box_finalize;

	signal_ids[REFRESHED] = g_signal_new (
		"refreshed",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
signature_combo_box_init (ESignatureComboBox *combo_box)
{
	GHashTable *index;

	/* Reverse-lookup index */
	index = g_hash_table_new_full (
		g_direct_hash, g_direct_equal,
		(GDestroyNotify) g_object_unref,
		(GDestroyNotify) gtk_tree_row_reference_free);

	combo_box->priv = E_SIGNATURE_COMBO_BOX_GET_PRIVATE (combo_box);
	combo_box->priv->index = index;
}

GType
e_signature_combo_box_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (ESignatureComboBoxClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) signature_combo_box_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (ESignatureComboBox),
			0,     /* n_preallocs */
			(GInstanceInitFunc) signature_combo_box_init,
			NULL  /* value_table */
		};

		type = g_type_register_static (
			GTK_TYPE_COMBO_BOX, "ESignatureComboBox",
			&type_info, 0);
	}

	return type;
}

GtkWidget *
e_signature_combo_box_new (void)
{
	return g_object_new (E_TYPE_SIGNATURE_COMBO_BOX, NULL);
}

ESignatureList *
e_signature_combo_box_get_signature_list (ESignatureComboBox *combo_box)
{
	g_return_val_if_fail (E_IS_SIGNATURE_COMBO_BOX (combo_box), NULL);

	return combo_box->priv->signature_list;
}

void
e_signature_combo_box_set_signature_list (ESignatureComboBox *combo_box,
                                          ESignatureList *signature_list)
{
	ESignatureComboBoxPrivate *priv;

	g_return_if_fail (E_IS_SIGNATURE_COMBO_BOX (combo_box));

	if (signature_list != NULL)
		g_return_if_fail (E_IS_SIGNATURE_LIST (signature_list));

	priv = E_SIGNATURE_COMBO_BOX_GET_PRIVATE (combo_box);

	if (priv->signature_list != NULL) {
		g_signal_handlers_disconnect_by_func (
			priv->signature_list,
			signature_combo_box_refresh_cb, combo_box);
		g_object_unref (priv->signature_list);
		priv->signature_list = NULL;
	}

	if (signature_list != NULL) {
		priv->signature_list = g_object_ref (signature_list);

		/* Listen for changes to the signature list. */
		g_signal_connect (
			priv->signature_list, "signature-added",
			G_CALLBACK (signature_combo_box_refresh_cb),
			combo_box);
		g_signal_connect (
			priv->signature_list, "signature-changed",
			G_CALLBACK (signature_combo_box_refresh_cb),
			combo_box);
		g_signal_connect (
			priv->signature_list, "signature-removed",
			G_CALLBACK (signature_combo_box_refresh_cb),
			combo_box);
	}

	signature_combo_box_refresh_cb (signature_list, NULL, combo_box);
}

ESignature *
e_signature_combo_box_get_active (ESignatureComboBox *combo_box)
{
	ESignature *signature;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean iter_set;

	g_return_val_if_fail (E_IS_SIGNATURE_COMBO_BOX (combo_box), NULL);

	iter_set = gtk_combo_box_get_active_iter (
		GTK_COMBO_BOX (combo_box), &iter);
	if (!iter_set)
		return NULL;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo_box));
	gtk_tree_model_get (model, &iter, COLUMN_SIGNATURE, &signature, -1);

	return signature;
}

gboolean
e_signature_combo_box_set_active (ESignatureComboBox *combo_box,
                                  ESignature *signature)
{
	GtkTreeRowReference *reference;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	gboolean iter_set;

	g_return_val_if_fail (E_IS_SIGNATURE_COMBO_BOX (combo_box), FALSE);

	if (signature != NULL)
		g_return_val_if_fail (E_IS_SIGNATURE (signature), FALSE);

	/* NULL means select "None" (always the first item). */
	if (signature == NULL) {
		gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), 0);
		return TRUE;
	}

	/* Lookup the tree row reference for the signature. */
	reference = g_hash_table_lookup (combo_box->priv->index, signature);
	if (reference == NULL)
		return FALSE;

	/* Convert the reference to a tree iterator. */
	path = gtk_tree_row_reference_get_path (reference);
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo_box));
	iter_set = gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_path_free (path);

	if (!iter_set)
		return FALSE;

	/* Activate the corresponding combo box item. */
	gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combo_box), &iter);

	return TRUE;
}
