/*
 * e-attachment-store.c
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

#include "e-attachment-store.h"

#include <glib/gi18n.h>

#include "e-util/e-util.h"
#include "e-util/gconf-bridge.h"

#define E_ATTACHMENT_STORE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_ATTACHMENT_STORE, EAttachmentStorePrivate))

struct _EAttachmentStorePrivate {
	GHashTable *attachment_index;
	gchar *background_filename;
	gchar *background_options;
	gchar *current_folder;

	guint ignore_row_changed : 1;
};

enum {
	PROP_0,
	PROP_BACKGROUND_FILENAME,
	PROP_BACKGROUND_OPTIONS,
	PROP_CURRENT_FOLDER,
	PROP_NUM_ATTACHMENTS,
	PROP_NUM_LOADING,
	PROP_TOTAL_SIZE
};

static gpointer parent_class;

static const gchar *
attachment_store_get_background_filename (EAttachmentStore *store)
{
	return store->priv->background_filename;
}

static void
attachment_store_set_background_filename (EAttachmentStore *store,
                                          const gchar *background_filename)
{
	if (background_filename == NULL)
		background_filename = "";

	g_free (store->priv->background_filename);
	store->priv->background_filename = g_strdup (background_filename);

	g_object_notify (G_OBJECT (store), "background-filename");
}

static const gchar *
attachment_store_get_background_options (EAttachmentStore *store)
{
	return store->priv->background_options;
}

static void
attachment_store_set_background_options (EAttachmentStore *store,
                                         const gchar *background_options)
{
	if (background_options == NULL)
		background_options = "";

	g_free (store->priv->background_options);
	store->priv->background_options = g_strdup (background_options);

	g_object_notify (G_OBJECT (store), "background-options");
}

static void
attachment_store_set_property (GObject *object,
                               guint property_id,
                               const GValue *value,
                               GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_BACKGROUND_FILENAME:
			attachment_store_set_background_filename (
				E_ATTACHMENT_STORE (object),
				g_value_get_string (value));
			return;

		case PROP_BACKGROUND_OPTIONS:
			attachment_store_set_background_options (
				E_ATTACHMENT_STORE (object),
				g_value_get_string (value));
			return;

		case PROP_CURRENT_FOLDER:
			e_attachment_store_set_current_folder (
				E_ATTACHMENT_STORE (object),
				g_value_get_string (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
attachment_store_get_property (GObject *object,
                               guint property_id,
                               GValue *value,
                               GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_BACKGROUND_FILENAME:
			g_value_set_string (
				value,
				attachment_store_get_background_filename (
				E_ATTACHMENT_STORE (object)));
			return;

		case PROP_BACKGROUND_OPTIONS:
			g_value_set_string (
				value,
				attachment_store_get_background_options (
				E_ATTACHMENT_STORE (object)));
			return;

		case PROP_CURRENT_FOLDER:
			g_value_set_string (
				value,
				e_attachment_store_get_current_folder (
				E_ATTACHMENT_STORE (object)));
			return;

		case PROP_NUM_ATTACHMENTS:
			g_value_set_uint (
				value,
				e_attachment_store_get_num_attachments (
				E_ATTACHMENT_STORE (object)));
			return;

		case PROP_NUM_LOADING:
			g_value_set_uint (
				value,
				e_attachment_store_get_num_loading (
				E_ATTACHMENT_STORE (object)));
			return;

		case PROP_TOTAL_SIZE:
			g_value_set_uint64 (
				value,
				e_attachment_store_get_total_size (
				E_ATTACHMENT_STORE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
attachment_store_dispose (GObject *object)
{
	EAttachmentStorePrivate *priv;

	priv = E_ATTACHMENT_STORE_GET_PRIVATE (object);

	g_hash_table_remove_all (priv->attachment_index);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
attachment_store_finalize (GObject *object)
{
	EAttachmentStorePrivate *priv;

	priv = E_ATTACHMENT_STORE_GET_PRIVATE (object);

	g_hash_table_destroy (priv->attachment_index);

	g_free (priv->background_filename);
	g_free (priv->background_options);
	g_free (priv->current_folder);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
attachment_store_constructed (GObject *object)
{
	EAttachmentStorePrivate *priv;
	GConfBridge *bridge;
	const gchar *prop;
	const gchar *key;

	priv = E_ATTACHMENT_STORE_GET_PRIVATE (object);
	bridge = gconf_bridge_get ();

	prop = "background-filename";
	key = "/desktop/gnome/background/picture_filename";
	gconf_bridge_bind_property (bridge, key, object, prop);

	prop = "background-options";
	key = "/desktop/gnome/background/picture_options";
	gconf_bridge_bind_property (bridge, key, object, prop);
}

static void
attachment_store_class_init (EAttachmentStoreClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EAttachmentStorePrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = attachment_store_set_property;
	object_class->get_property = attachment_store_get_property;
	object_class->dispose = attachment_store_dispose;
	object_class->finalize = attachment_store_finalize;
	object_class->constructed = attachment_store_constructed;

	g_object_class_install_property (
		object_class,
		PROP_BACKGROUND_FILENAME,
		g_param_spec_string (
			"background-filename",
			"Background Filename",
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_BACKGROUND_OPTIONS,
		g_param_spec_string (
			"background-options",
			"Background Options",
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_CURRENT_FOLDER,
		g_param_spec_string (
			"current-folder",
			"Current Folder",
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_NUM_ATTACHMENTS,
		g_param_spec_uint (
			"num-attachments",
			"Num Attachments",
			NULL,
			0,
			G_MAXUINT,
			0,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_NUM_LOADING,
		g_param_spec_uint (
			"num-loading",
			"Num Loading",
			NULL,
			0,
			G_MAXUINT,
			0,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_TOTAL_SIZE,
		g_param_spec_uint64 (
			"total-size",
			"Total Size",
			NULL,
			0,
			G_MAXUINT64,
			0,
			G_PARAM_READABLE));
}

static void
attachment_store_init (EAttachmentStore *store)
{
	GType types[E_ATTACHMENT_STORE_NUM_COLUMNS];
	GHashTable *attachment_index;
	gint column = 0;

	attachment_index = g_hash_table_new_full (
		g_direct_hash, g_direct_equal,
		(GDestroyNotify) g_object_unref,
		(GDestroyNotify) gtk_tree_row_reference_free);

	store->priv = E_ATTACHMENT_STORE_GET_PRIVATE (store);
	store->priv->attachment_index = attachment_index;

	types[column++] = E_TYPE_ATTACHMENT;	/* COLUMN_ATTACHMENT */
	types[column++] = G_TYPE_STRING;	/* COLUMN_CAPTION */
	types[column++] = G_TYPE_STRING;	/* COLUMN_CONTENT_TYPE */
	types[column++] = G_TYPE_STRING;	/* COLUMN_DISPLAY_NAME */
	types[column++] = G_TYPE_ICON;		/* COLUMN_ICON */
	types[column++] = G_TYPE_BOOLEAN;	/* COLUMN_LOADING */
	types[column++] = G_TYPE_INT;		/* COLUMN_PERCENT */
	types[column++] = G_TYPE_BOOLEAN;	/* COLUMN_SAVING */
	types[column++] = G_TYPE_UINT64;	/* COLUMN_SIZE */

	g_assert (column == E_ATTACHMENT_STORE_NUM_COLUMNS);

	gtk_list_store_set_column_types (
		GTK_LIST_STORE (store), G_N_ELEMENTS (types), types);
}

GType
e_attachment_store_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EAttachmentStoreClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) attachment_store_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EAttachmentStore),
			0,     /* n_preallocs */
			(GInstanceInitFunc) attachment_store_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			GTK_TYPE_LIST_STORE, "EAttachmentStore",
			&type_info, 0);
	}

	return type;
}

GtkTreeModel *
e_attachment_store_new (void)
{
	return g_object_new (E_TYPE_ATTACHMENT_STORE, NULL);
}

void
e_attachment_store_add_attachment (EAttachmentStore *store,
                                   EAttachment *attachment)
{
	GtkTreeRowReference *reference;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	GFile *file;

	g_return_if_fail (E_IS_ATTACHMENT_STORE (store));
	g_return_if_fail (E_IS_ATTACHMENT (attachment));

	gtk_list_store_append (GTK_LIST_STORE (store), &iter);

	gtk_list_store_set (
		GTK_LIST_STORE (store), &iter,
		E_ATTACHMENT_STORE_COLUMN_ATTACHMENT, attachment, -1);

	model = GTK_TREE_MODEL (store);
	path = gtk_tree_model_get_path (model, &iter);
	reference = gtk_tree_row_reference_new (model, path);
	gtk_tree_path_free (path);

	g_hash_table_insert (
		store->priv->attachment_index,
		g_object_ref (attachment), reference);

	file = e_attachment_get_file (attachment);

	/* This lets the attachment tell us when to update. */
	e_attachment_set_reference (attachment, reference);

	g_object_freeze_notify (G_OBJECT (store));
	g_object_notify (G_OBJECT (store), "num-attachments");
	g_object_notify (G_OBJECT (store), "total-size");
	g_object_thaw_notify (G_OBJECT (store));
}

gboolean
e_attachment_store_remove_attachment (EAttachmentStore *store,
                                      EAttachment *attachment)
{
	GtkTreeRowReference *reference;
	GHashTable *hash_table;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;

	g_return_val_if_fail (E_IS_ATTACHMENT_STORE (store), FALSE);
	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), FALSE);

	hash_table = store->priv->attachment_index;
	reference = g_hash_table_lookup (hash_table, attachment);

	if (reference == NULL)
		return FALSE;

	if (!gtk_tree_row_reference_valid (reference)) {
		g_hash_table_remove (hash_table, attachment);
		return FALSE;
	}

	e_attachment_cancel (attachment);
	e_attachment_set_reference (attachment, NULL);

	model = gtk_tree_row_reference_get_model (reference);
	path = gtk_tree_row_reference_get_path (reference);
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_path_free (path);

	gtk_list_store_remove (GTK_LIST_STORE (store), &iter);
	g_hash_table_remove (hash_table, attachment);

	g_object_freeze_notify (G_OBJECT (store));
	g_object_notify (G_OBJECT (store), "num-attachments");
	g_object_notify (G_OBJECT (store), "total-size");
	g_object_thaw_notify (G_OBJECT (store));

	return TRUE;
}

void
e_attachment_store_add_to_multipart (EAttachmentStore *store,
                                     CamelMultipart *multipart,
                                     const gchar *default_charset)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean valid;

	g_return_if_fail (E_IS_ATTACHMENT_STORE (store));
	g_return_if_fail (CAMEL_MULTIPART (multipart));

	model = GTK_TREE_MODEL (store);
	valid = gtk_tree_model_get_iter_first (model, &iter);

	while (valid) {
		EAttachment *attachment;
		gint column_id;

		column_id = E_ATTACHMENT_STORE_COLUMN_ATTACHMENT;
		gtk_tree_model_get (model, &iter, column_id, &attachment, -1);

		/* Skip the attachment if it's still loading. */
		if (!e_attachment_get_loading (attachment))
			e_attachment_add_to_multipart (
				attachment, multipart, default_charset);

		g_object_unref (attachment);

		valid = gtk_tree_model_iter_next (model, &iter);
	}
}

const gchar *
e_attachment_store_get_current_folder (EAttachmentStore *store)
{
	g_return_val_if_fail (E_IS_ATTACHMENT_STORE (store), NULL);

	return store->priv->current_folder;
}

void
e_attachment_store_set_current_folder (EAttachmentStore *store,
                                       const gchar *current_folder)
{
	g_return_if_fail (E_IS_ATTACHMENT_STORE (store));

	if (current_folder == NULL)
		current_folder = g_get_home_dir ();

	g_free (store->priv->current_folder);
	store->priv->current_folder = g_strdup (current_folder);

	g_object_notify (G_OBJECT (store), "current-folder");
}

guint
e_attachment_store_get_num_attachments (EAttachmentStore *store)
{
	g_return_val_if_fail (E_IS_ATTACHMENT_STORE (store), 0);

	return g_hash_table_size (store->priv->attachment_index);
}

guint
e_attachment_store_get_num_loading (EAttachmentStore *store)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	guint num_loading = 0;
	gboolean valid;

	g_return_val_if_fail (E_IS_ATTACHMENT_STORE (store), 0);

	model = GTK_TREE_MODEL (store);
	valid = gtk_tree_model_get_iter_first (model, &iter);

	while (valid) {
		EAttachment *attachment;
		gint column_id;

		column_id = E_ATTACHMENT_STORE_COLUMN_ATTACHMENT;
		gtk_tree_model_get (model, &iter, column_id, &attachment, -1);
		if (e_attachment_get_loading (attachment))
			num_loading++;
		g_object_unref (attachment);

		valid = gtk_tree_model_iter_next (model, &iter);
	}

	return num_loading;
}

goffset
e_attachment_store_get_total_size (EAttachmentStore *store)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	goffset total_size = 0;
	gboolean valid;

	g_return_val_if_fail (E_IS_ATTACHMENT_STORE (store), 0);

	model = GTK_TREE_MODEL (store);
	valid = gtk_tree_model_get_iter_first (model, &iter);

	while (valid) {
		EAttachment *attachment;
		GFileInfo *file_info;
		gint column_id;

		column_id = E_ATTACHMENT_STORE_COLUMN_ATTACHMENT;
		gtk_tree_model_get (model, &iter, column_id, &attachment, -1);
		file_info = e_attachment_get_file_info (attachment);
		if (file_info != NULL)
			total_size += g_file_info_get_size (file_info);
		g_object_unref (attachment);

		valid = gtk_tree_model_iter_next (model, &iter);
	}

	return total_size;
}

gint
e_attachment_store_run_file_chooser_dialog (EAttachmentStore *store,
                                            GtkWidget *dialog)
{
	GtkFileChooser *file_chooser;
	gint response = GTK_RESPONSE_NONE;
	const gchar *current_folder;
	gboolean update_folder;

	g_return_val_if_fail (E_IS_ATTACHMENT_STORE (store), response);
	g_return_val_if_fail (GTK_IS_FILE_CHOOSER_DIALOG (dialog), response);

	file_chooser = GTK_FILE_CHOOSER (dialog);
	current_folder = e_attachment_store_get_current_folder (store);
	gtk_file_chooser_set_current_folder (file_chooser, current_folder);

	response = gtk_dialog_run (GTK_DIALOG (dialog));

	update_folder =
		(response == GTK_RESPONSE_ACCEPT) ||
		(response == GTK_RESPONSE_OK) ||
		(response == GTK_RESPONSE_YES) ||
		(response == GTK_RESPONSE_APPLY);

	if (update_folder) {
		gchar *folder;

		folder = gtk_file_chooser_get_current_folder (file_chooser);
		e_attachment_store_set_current_folder (store, folder);
		g_free (folder);
	}

	return response;
}

void
e_attachment_store_run_load_dialog (EAttachmentStore *store,
                                    GtkWindow *parent)
{
	GtkFileChooser *file_chooser;
	GtkWidget *dialog;
	GtkWidget *option;
	GSList *files, *iter;
	const gchar *disposition;
	gboolean active;
	gint response;

	g_return_if_fail (E_IS_ATTACHMENT_STORE (store));
	g_return_if_fail (GTK_IS_WINDOW (parent));

	dialog = gtk_file_chooser_dialog_new (
		_("Add Attachment"), parent,
		GTK_FILE_CHOOSER_ACTION_OPEN,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		_("A_ttach"), GTK_RESPONSE_OK, NULL);

	file_chooser = GTK_FILE_CHOOSER (dialog);
	gtk_file_chooser_set_local_only (file_chooser, FALSE);
	gtk_file_chooser_set_select_multiple (file_chooser, TRUE);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), "mail-attachment");

	option = gtk_check_button_new_with_mnemonic (
		_("_Suggest automatic display of attachment"));
	gtk_file_chooser_set_extra_widget (file_chooser, option);
	gtk_widget_show (option);

	response = e_attachment_store_run_file_chooser_dialog (store, dialog);

	if (response != GTK_RESPONSE_OK)
		goto exit;

	files = gtk_file_chooser_get_files (file_chooser);
	active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (option));
	disposition = active ? "inline" : "attachment";

	for (iter = files; iter != NULL; iter = g_slist_next (iter)) {
		EAttachment *attachment;
		GFile *file = iter->data;

		attachment = e_attachment_new ();
		e_attachment_set_file (attachment, file);
		e_attachment_store_add_attachment (store, attachment);
		e_attachment_load_async (
			attachment, (GAsyncReadyCallback)
			e_attachment_load_handle_error, parent);
		g_object_unref (attachment);
	}

	g_slist_foreach (files, (GFunc) g_object_unref, NULL);
	g_slist_free (files);

exit:
	gtk_widget_destroy (dialog);
}

void
e_attachment_store_run_save_dialog (EAttachmentStore *store,
                                    GList *attachment_list,
                                    GtkWindow *parent)
{
	GtkFileChooser *file_chooser;
	GtkFileChooserAction action;
	GtkWidget *dialog;
	GFile *destination;
	const gchar *title;
	gint response;
	guint length;

	g_return_if_fail (E_IS_ATTACHMENT_STORE (store));
	g_return_if_fail (GTK_IS_WINDOW (parent));

	length = g_list_length (attachment_list);

	if (length == 0)
		return;

	title = ngettext ("Save Attachment", "Save Attachments", length);

	if (length == 1)
		action = GTK_FILE_CHOOSER_ACTION_SAVE;
	else
		action = GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;

	dialog = gtk_file_chooser_dialog_new (
		title, parent, action,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_SAVE, GTK_RESPONSE_OK, NULL);

	file_chooser = GTK_FILE_CHOOSER (dialog);
	gtk_file_chooser_set_local_only (file_chooser, FALSE);
	gtk_file_chooser_set_do_overwrite_confirmation (file_chooser, TRUE);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), "mail-attachment");

	if (action == GTK_FILE_CHOOSER_ACTION_SAVE) {
		EAttachment *attachment;
		GFileInfo *file_info;
		const gchar *name = NULL;

		attachment = attachment_list->data;
		file_info = e_attachment_get_file_info (attachment);
		if (file_info != NULL)
			name = g_file_info_get_display_name (file_info);
		if (name == NULL)
			/* Translators: Default attachment filename. */
			name = _("attachment.dat");
		gtk_file_chooser_set_current_name (file_chooser, name);
	}

	response = e_attachment_store_run_file_chooser_dialog (store, dialog);

	if (response != GTK_RESPONSE_OK)
		goto exit;

	destination = gtk_file_chooser_get_file (file_chooser);

	while (attachment_list != NULL) {
		e_attachment_save_async (
			attachment_list->data,
			destination, (GAsyncReadyCallback)
			e_attachment_save_handle_error, parent);
		attachment_list = g_list_next (attachment_list);
	}

	g_object_unref (destination);

exit:
	gtk_widget_destroy (dialog);
}
