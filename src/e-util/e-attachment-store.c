/*
 * e-attachment-store.c
 *
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <errno.h>
#include <glib/gi18n.h>

#ifdef HAVE_AUTOAR
#include <gnome-autoar/gnome-autoar.h>
#include <gnome-autoar/autoar-gtk.h>
#endif

#include "e-attachment-bar.h"
#include "e-icon-factory.h"
#include "e-misc-utils.h"
#include "e-mktemp.h"

#include "e-attachment-store.h"

struct _EAttachmentStorePrivate {
	GHashTable *attachment_index;

	guint ignore_row_changed : 1;
};

enum {
	PROP_0,
	PROP_NUM_ATTACHMENTS,
	PROP_NUM_LOADING,
	PROP_TOTAL_SIZE
};

enum {
	ATTACHMENT_ADDED,
	ATTACHMENT_REMOVED,
	LAST_SIGNAL
};

static gulong signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_PRIVATE (EAttachmentStore, e_attachment_store, GTK_TYPE_LIST_STORE)

static void
attachment_store_update_file_info_cb (EAttachment *attachment,
				      const gchar *caption,
				      const gchar *content_type,
				      const gchar *description,
				      gint64 size,
				      gpointer user_data)
{
	EAttachmentStore *store = user_data;
	GtkTreeIter iter;

	g_return_if_fail (E_IS_ATTACHMENT (attachment));
	g_return_if_fail (E_IS_ATTACHMENT_STORE (store));

	if (e_attachment_store_find_attachment_iter (store, attachment, &iter)) {
		gtk_list_store_set (
			GTK_LIST_STORE (store), &iter,
			E_ATTACHMENT_STORE_COLUMN_CAPTION, caption,
			E_ATTACHMENT_STORE_COLUMN_CONTENT_TYPE, content_type,
			E_ATTACHMENT_STORE_COLUMN_DESCRIPTION, description,
			E_ATTACHMENT_STORE_COLUMN_SIZE, size,
			-1);
	}
}

static void
attachment_store_update_icon_cb (EAttachment *attachment,
				 GIcon *icon,
				 gpointer user_data)
{
	EAttachmentStore *store = user_data;
	GtkTreeIter iter;

	g_return_if_fail (E_IS_ATTACHMENT (attachment));
	g_return_if_fail (E_IS_ATTACHMENT_STORE (store));

	if (e_attachment_store_find_attachment_iter (store, attachment, &iter)) {
		gtk_list_store_set (
			GTK_LIST_STORE (store), &iter,
			E_ATTACHMENT_STORE_COLUMN_ICON, icon,
			-1);
	}
}

static void
attachment_store_update_progress_cb (EAttachment *attachment,
				     gboolean loading,
				     gboolean saving,
				     gint percent,
				     gpointer user_data)
{
	EAttachmentStore *store = user_data;
	GtkTreeIter iter;

	g_return_if_fail (E_IS_ATTACHMENT (attachment));
	g_return_if_fail (E_IS_ATTACHMENT_STORE (store));

	if (e_attachment_store_find_attachment_iter (store, attachment, &iter)) {
		gtk_list_store_set (
			GTK_LIST_STORE (store), &iter,
			E_ATTACHMENT_STORE_COLUMN_LOADING, loading,
			E_ATTACHMENT_STORE_COLUMN_SAVING, saving,
			E_ATTACHMENT_STORE_COLUMN_PERCENT, percent,
			-1);
	}
}

static void
attachment_store_load_failed_cb (EAttachment *attachment,
				 gpointer user_data)
{
	EAttachmentStore *store = user_data;

	g_return_if_fail (E_IS_ATTACHMENT (attachment));
	g_return_if_fail (E_IS_ATTACHMENT_STORE (store));

	e_attachment_store_remove_attachment (store, attachment);
}

static void
attachment_store_attachment_notify_cb (GObject *attachment,
				       GParamSpec *param,
				       gpointer user_data)
{
	EAttachmentStore *store = user_data;

	g_return_if_fail (E_IS_ATTACHMENT (attachment));
	g_return_if_fail (param != NULL);
	g_return_if_fail (E_IS_ATTACHMENT_STORE (store));

	if (g_str_equal (param->name, "loading")) {
		g_object_notify (G_OBJECT (store), "num-loading");
	} else if (g_str_equal (param->name, "file-info")) {
		g_object_notify (G_OBJECT (store), "total-size");
	}
}

static void
attachment_store_attachment_added (EAttachmentStore *store,
				   EAttachment *attachment)
{
	g_return_if_fail (E_IS_ATTACHMENT_STORE (store));
	g_return_if_fail (E_IS_ATTACHMENT (attachment));

	g_signal_connect (attachment, "update-file-info",
		G_CALLBACK (attachment_store_update_file_info_cb), store);
	g_signal_connect (attachment, "update-icon",
		G_CALLBACK (attachment_store_update_icon_cb), store);
	g_signal_connect (attachment, "update-progress",
		G_CALLBACK (attachment_store_update_progress_cb), store);
	g_signal_connect (attachment, "load-failed",
		G_CALLBACK (attachment_store_load_failed_cb), store);
	g_signal_connect (attachment, "notify",
		G_CALLBACK (attachment_store_attachment_notify_cb), store);

	e_attachment_update_store_columns (attachment);
}

static void
attachment_store_attachment_removed (EAttachmentStore *store,
				     EAttachment *attachment)
{
	g_return_if_fail (E_IS_ATTACHMENT_STORE (store));
	g_return_if_fail (E_IS_ATTACHMENT (attachment));

	g_signal_handlers_disconnect_by_func (attachment,
		G_CALLBACK (attachment_store_update_file_info_cb), store);
	g_signal_handlers_disconnect_by_func (attachment,
		G_CALLBACK (attachment_store_update_icon_cb), store);
	g_signal_handlers_disconnect_by_func (attachment,
		G_CALLBACK (attachment_store_update_progress_cb), store);
	g_signal_handlers_disconnect_by_func (attachment,
		G_CALLBACK (attachment_store_load_failed_cb), store);
	g_signal_handlers_disconnect_by_func (attachment,
		G_CALLBACK (attachment_store_attachment_notify_cb), store);
}

static void
attachment_store_get_property (GObject *object,
                               guint property_id,
                               GValue *value,
                               GParamSpec *pspec)
{
	switch (property_id) {
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
	e_attachment_store_remove_all (E_ATTACHMENT_STORE (object));

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_attachment_store_parent_class)->dispose (object);
}

static void
attachment_store_finalize (GObject *object)
{
	EAttachmentStore *self = E_ATTACHMENT_STORE (object);

	g_hash_table_destroy (self->priv->attachment_index);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_attachment_store_parent_class)->finalize (object);
}

static void
e_attachment_store_class_init (EAttachmentStoreClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = attachment_store_get_property;
	object_class->dispose = attachment_store_dispose;
	object_class->finalize = attachment_store_finalize;

	class->attachment_added = attachment_store_attachment_added;
	class->attachment_removed = attachment_store_attachment_removed;

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

	signals[ATTACHMENT_ADDED] = g_signal_new (
		"attachment-added",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EAttachmentStoreClass, attachment_added),
		NULL, NULL, NULL,
		G_TYPE_NONE, 1, E_TYPE_ATTACHMENT);

	signals[ATTACHMENT_REMOVED] = g_signal_new (
		"attachment-removed",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EAttachmentStoreClass, attachment_removed),
		NULL, NULL, NULL,
		G_TYPE_NONE, 1, E_TYPE_ATTACHMENT);
}

static void
e_attachment_store_init (EAttachmentStore *store)
{
	GType types[E_ATTACHMENT_STORE_NUM_COLUMNS];
	GHashTable *attachment_index;
	gint column = 0;

	attachment_index = g_hash_table_new_full (
		g_direct_hash, g_direct_equal,
		(GDestroyNotify) g_object_unref,
		(GDestroyNotify) gtk_tree_row_reference_free);

	store->priv = e_attachment_store_get_instance_private (store);
	store->priv->attachment_index = attachment_index;

	types[column++] = E_TYPE_ATTACHMENT;	/* COLUMN_ATTACHMENT */
	types[column++] = G_TYPE_STRING;	/* COLUMN_CAPTION */
	types[column++] = G_TYPE_STRING;	/* COLUMN_CONTENT_TYPE */
	types[column++] = G_TYPE_STRING;	/* COLUMN_DESCRIPTION */
	types[column++] = G_TYPE_ICON;		/* COLUMN_ICON */
	types[column++] = G_TYPE_BOOLEAN;	/* COLUMN_LOADING */
	types[column++] = G_TYPE_INT;		/* COLUMN_PERCENT */
	types[column++] = G_TYPE_BOOLEAN;	/* COLUMN_SAVING */
	types[column++] = G_TYPE_UINT64;	/* COLUMN_SIZE */

	g_return_if_fail (column == E_ATTACHMENT_STORE_NUM_COLUMNS);

	gtk_list_store_set_column_types (
		GTK_LIST_STORE (store), G_N_ELEMENTS (types), types);
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

	g_object_freeze_notify (G_OBJECT (store));
	g_object_notify (G_OBJECT (store), "num-attachments");
	g_object_notify (G_OBJECT (store), "total-size");
	g_object_thaw_notify (G_OBJECT (store));

	g_signal_emit (store, signals[ATTACHMENT_ADDED], 0, attachment);
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
	gboolean removed;

	g_return_val_if_fail (E_IS_ATTACHMENT_STORE (store), FALSE);
	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), FALSE);

	hash_table = store->priv->attachment_index;
	reference = g_hash_table_lookup (hash_table, attachment);

	if (reference == NULL)
		return FALSE;

	if (!gtk_tree_row_reference_valid (reference)) {
		if (g_hash_table_remove (hash_table, attachment))
			g_signal_emit (store, signals[ATTACHMENT_REMOVED], 0, attachment);
		return FALSE;
	}

	e_attachment_cancel (attachment);

	model = gtk_tree_row_reference_get_model (reference);
	path = gtk_tree_row_reference_get_path (reference);
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_path_free (path);

	gtk_list_store_remove (GTK_LIST_STORE (store), &iter);
	removed = g_hash_table_remove (hash_table, attachment);

	g_object_freeze_notify (G_OBJECT (store));
	g_object_notify (G_OBJECT (store), "num-attachments");
	g_object_notify (G_OBJECT (store), "total-size");
	g_object_thaw_notify (G_OBJECT (store));

	if (removed)
		g_signal_emit (store, signals[ATTACHMENT_REMOVED], 0, attachment);

	return TRUE;
}

void
e_attachment_store_remove_all (EAttachmentStore *store)
{
	GList *list, *iter;

	g_return_if_fail (E_IS_ATTACHMENT_STORE (store));

	if (!g_hash_table_size (store->priv->attachment_index))
		return;

	g_object_freeze_notify (G_OBJECT (store));

	/* Get the list of attachments before clearing the list store,
	   otherwise there would be returned no attachments. */
	list = e_attachment_store_get_attachments (store);

	/* Clear the list store before cancelling EAttachment load/save
	 * operations.  This will invalidate the EAttachment's tree row
	 * reference so it won't try to update the row's icon column in
	 * response to the cancellation.  That can create problems when
	 * the list store is being disposed. */
	gtk_list_store_clear (GTK_LIST_STORE (store));

	for (iter = list; iter; iter = iter->next) {
		EAttachment *attachment = iter->data;

		e_attachment_cancel (attachment);

		g_warn_if_fail (g_hash_table_remove (store->priv->attachment_index, attachment));

		g_signal_emit (store, signals[ATTACHMENT_REMOVED], 0, attachment);
	}

	g_list_foreach (list, (GFunc) g_object_unref, NULL);
	g_list_free (list);

	g_object_notify (G_OBJECT (store), "num-attachments");
	g_object_notify (G_OBJECT (store), "total-size");
	g_object_thaw_notify (G_OBJECT (store));
}

void
e_attachment_store_add_to_multipart (EAttachmentStore *store,
                                     CamelMultipart *multipart,
                                     const gchar *default_charset)
{
	GList *list, *iter;

	g_return_if_fail (E_IS_ATTACHMENT_STORE (store));
	g_return_if_fail (CAMEL_MULTIPART (multipart));

	list = e_attachment_store_get_attachments (store);

	for (iter = list; iter != NULL; iter = iter->next) {
		EAttachment *attachment = iter->data;

		/* Skip the attachment if it's still loading. */
		if (!e_attachment_get_loading (attachment))
			e_attachment_add_to_multipart (
				attachment, multipart, default_charset);
	}

	g_list_foreach (list, (GFunc) g_object_unref, NULL);
	g_list_free (list);
}

GList *
e_attachment_store_get_attachments (EAttachmentStore *store)
{
	GList *list = NULL;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean valid;

	g_return_val_if_fail (E_IS_ATTACHMENT_STORE (store), NULL);

	model = GTK_TREE_MODEL (store);
	valid = gtk_tree_model_get_iter_first (model, &iter);

	while (valid) {
		EAttachment *attachment;
		gint column_id;

		column_id = E_ATTACHMENT_STORE_COLUMN_ATTACHMENT;
		gtk_tree_model_get (model, &iter, column_id, &attachment, -1);

		list = g_list_prepend (list, attachment);

		valid = gtk_tree_model_iter_next (model, &iter);
	}

	return g_list_reverse (list);
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
	GList *list, *iter;
	guint num_loading = 0;

	g_return_val_if_fail (E_IS_ATTACHMENT_STORE (store), 0);

	list = e_attachment_store_get_attachments (store);

	for (iter = list; iter != NULL; iter = iter->next) {
		EAttachment *attachment = iter->data;

		if (e_attachment_get_loading (attachment))
			num_loading++;
	}

	g_list_foreach (list, (GFunc) g_object_unref, NULL);
	g_list_free (list);

	return num_loading;
}

goffset
e_attachment_store_get_total_size (EAttachmentStore *store)
{
	GList *list, *iter;
	goffset total_size = 0;

	g_return_val_if_fail (E_IS_ATTACHMENT_STORE (store), 0);

	list = e_attachment_store_get_attachments (store);

	for (iter = list; iter != NULL; iter = iter->next) {
		EAttachment *attachment = iter->data;
		GFileInfo *file_info;

		file_info = e_attachment_ref_file_info (attachment);
		if (file_info != NULL) {
			/* URL attachments do not have the size */
			if (g_file_info_has_attribute (file_info, G_FILE_ATTRIBUTE_STANDARD_SIZE))
				total_size += g_file_info_get_size (file_info);
			g_object_unref (file_info);
		}
	}

	g_list_foreach (list, (GFunc) g_object_unref, NULL);
	g_list_free (list);

	return total_size;
}

static void
update_preview_cb (GtkFileChooser *file_chooser,
                   gpointer data)
{
	GtkWidget *preview;
	gchar *filename = NULL;
	GdkPixbuf *pixbuf;

	gtk_file_chooser_set_preview_widget_active (file_chooser, FALSE);
	gtk_image_clear (GTK_IMAGE (data));
	preview = GTK_WIDGET (data);
	filename = gtk_file_chooser_get_preview_filename (file_chooser);
	if (!e_util_can_preview_filename (filename)) {
		g_free (filename);
		return;
	}

	pixbuf = gdk_pixbuf_new_from_file_at_size (filename, 128, 128, NULL);
	g_free (filename);
	if (!pixbuf)
		return;

	gtk_file_chooser_set_preview_widget_active (file_chooser, TRUE);
	gtk_image_set_from_pixbuf (GTK_IMAGE (preview), pixbuf);
	g_object_unref (pixbuf);
}

void
e_attachment_store_run_load_dialog (EAttachmentStore *store,
                                    GtkWindow *parent)
{
	GtkFileChooser *file_chooser;
	GtkWidget *dialog = NULL;
	GtkFileChooserNative *native = NULL;
	GtkBox *extra_box;
	GtkWidget *extra_box_widget;
	GtkWidget *option_display = NULL;
	GtkImage *preview;
	GSList *files, *iter;
	const gchar *disposition;
	gboolean active;
	gint response;
#ifdef HAVE_AUTOAR
	GtkBox *option_format_box;
	GtkWidget *option_format_box_widget;
	GtkWidget *option_format_label;
	GtkWidget *option_format_combo;
	GSettings *settings = NULL;
	gchar *format_string = NULL;
	gchar *filter_string = NULL;
	gint format;
	gint filter;
#endif

	g_return_if_fail (E_IS_ATTACHMENT_STORE (store));
	g_return_if_fail (GTK_IS_WINDOW (parent));

	if (e_util_is_running_flatpak ()) {
		native = gtk_file_chooser_native_new (
			_("Add Attachment"), parent,
			GTK_FILE_CHOOSER_ACTION_OPEN,
			_("A_ttach"), _("_Cancel"));

		file_chooser = GTK_FILE_CHOOSER (native);
	} else {
		dialog = gtk_file_chooser_dialog_new (
			_("Add Attachment"), parent,
			GTK_FILE_CHOOSER_ACTION_OPEN,
#ifdef HAVE_AUTOAR
			_("_Open"), GTK_RESPONSE_ACCEPT,
#endif
			_("_Cancel"), GTK_RESPONSE_CANCEL,
#ifdef HAVE_AUTOAR
			_("A_ttach"), GTK_RESPONSE_CLOSE,
#else
			_("A_ttach"), GTK_RESPONSE_ACCEPT,
#endif
			NULL);

		file_chooser = GTK_FILE_CHOOSER (dialog);
	}

	gtk_file_chooser_set_local_only (file_chooser, FALSE);
	gtk_file_chooser_set_select_multiple (file_chooser, TRUE);

	if (dialog) {
		gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);

		gtk_window_set_icon_name (GTK_WINDOW (dialog), "mail-attachment");

		preview = GTK_IMAGE (gtk_image_new ());
		gtk_file_chooser_set_preview_widget (file_chooser, GTK_WIDGET (preview));
		g_signal_connect (
			file_chooser, "update-preview",
			G_CALLBACK (update_preview_cb), preview);

		extra_box_widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
		extra_box = GTK_BOX (extra_box_widget);

		option_display = gtk_check_button_new_with_mnemonic (
			_("_Suggest automatic display of attachment"));
		gtk_box_pack_start (extra_box, option_display, FALSE, FALSE, 0);

#ifdef HAVE_AUTOAR
		option_format_box_widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
		option_format_box = GTK_BOX (option_format_box_widget);
		gtk_box_pack_start (extra_box, option_format_box_widget, FALSE, FALSE, 0);

		settings = e_util_ref_settings ("org.gnome.evolution.shell");

		format_string = g_settings_get_string (settings, "autoar-format");
		filter_string = g_settings_get_string (settings, "autoar-filter");

		if (!e_enum_from_string (AUTOAR_TYPE_FORMAT, format_string, &format)) {
			format = AUTOAR_FORMAT_ZIP;
		}
		if (!e_enum_from_string (AUTOAR_TYPE_FILTER, filter_string, &filter)) {
			filter = AUTOAR_FILTER_NONE;
		}

		option_format_label = gtk_label_new (
			_("Archive selected directories using this format:"));
		option_format_combo = autoar_gtk_chooser_simple_new (
			format,
			filter);
		gtk_box_pack_start (option_format_box, option_format_label, FALSE, FALSE, 0);
		gtk_box_pack_start (option_format_box, option_format_combo, FALSE, FALSE, 0);
#endif

		gtk_file_chooser_set_extra_widget (file_chooser, extra_box_widget);
		gtk_widget_show_all (extra_box_widget);
	}

	e_util_load_file_chooser_folder (file_chooser);

	if (dialog)
		response = gtk_dialog_run (GTK_DIALOG (dialog));
	else
		response = gtk_native_dialog_run (GTK_NATIVE_DIALOG (native));

#ifdef HAVE_AUTOAR
	if (response != GTK_RESPONSE_ACCEPT && response != GTK_RESPONSE_CLOSE)
#else
	if (response != GTK_RESPONSE_ACCEPT)
#endif
		goto exit;

	e_util_save_file_chooser_folder (file_chooser);

	files = gtk_file_chooser_get_files (file_chooser);
	active = option_display ? gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (option_display)) : FALSE;
	disposition = active ? "inline" : "attachment";

#ifdef HAVE_AUTOAR
	if (dialog) {
		autoar_gtk_chooser_simple_get (option_format_combo, &format, &filter);

		if (!e_enum_to_string (AUTOAR_TYPE_FORMAT, format)) {
			format = AUTOAR_FORMAT_ZIP;
		}

		if (!e_enum_to_string (AUTOAR_TYPE_FORMAT, filter)) {
			filter = AUTOAR_FILTER_NONE;
		}

		g_settings_set_string (
			settings,
			"autoar-format",
			e_enum_to_string (AUTOAR_TYPE_FORMAT, format));
		g_settings_set_string (
			settings,
			"autoar-filter",
			e_enum_to_string (AUTOAR_TYPE_FILTER, filter));
	}
#endif

	for (iter = files; iter != NULL; iter = g_slist_next (iter)) {
		EAttachment *attachment;
		GFile *file = iter->data;

		attachment = e_attachment_new ();
		e_attachment_set_file (attachment, file);
		e_attachment_set_disposition (attachment, disposition);
		e_attachment_store_add_attachment (store, attachment);

		e_attachment_load_async (
			attachment, (GAsyncReadyCallback)
			e_attachment_load_handle_error, parent);
		g_object_unref (attachment);
	}

	g_slist_foreach (files, (GFunc) g_object_unref, NULL);
	g_slist_free (files);

 exit:
	if (dialog)
		gtk_widget_destroy (dialog);
	else
		g_clear_object (&native);

#ifdef HAVE_AUTOAR
	g_clear_object (&settings);
	g_free (format_string);
	g_free (filter_string);
#endif
}

GFile *
e_attachment_store_run_save_dialog (EAttachmentStore *store,
                                    GList *attachment_list,
                                    GtkWindow *parent)
{
	GtkFileChooser *file_chooser;
	GtkFileChooserAction action;
	GtkWidget *dialog = NULL;
	GtkFileChooserNative *native = NULL;
	GFile *destination;
	const gchar *title;
	gint response;
	guint length;
#ifdef HAVE_AUTOAR
	GtkBox *extra_box;
	GtkWidget *extra_box_widget = NULL;

	GtkBox *extract_box;
	GtkWidget *extract_box_widget;

	GSList *extract_group;
	GtkWidget *extract_dont, *extract_only, *extract_org;
#endif

	g_return_val_if_fail (E_IS_ATTACHMENT_STORE (store), NULL);

	length = g_list_length (attachment_list);

	if (length == 0)
		return NULL;

	title = ngettext ("Save Attachment", "Save Attachments", length);

	if (length == 1)
		action = GTK_FILE_CHOOSER_ACTION_SAVE;
	else
		action = GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;

	if (e_util_is_running_flatpak ()) {
		native = gtk_file_chooser_native_new (
			title, GTK_WINDOW (parent), action,
			_("_Save"), _("_Cancel"));

		file_chooser = GTK_FILE_CHOOSER (native);
	} else {
		dialog = gtk_file_chooser_dialog_new (
			title, parent, action,
			_("_Cancel"), GTK_RESPONSE_CANCEL,
			_("_Save"), GTK_RESPONSE_ACCEPT, NULL);

		file_chooser = GTK_FILE_CHOOSER (dialog);
	}

	gtk_file_chooser_set_local_only (file_chooser, FALSE);
	gtk_file_chooser_set_do_overwrite_confirmation (file_chooser, TRUE);

	if (dialog) {
		gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
		gtk_window_set_icon_name (GTK_WINDOW (dialog), "mail-attachment");

#ifdef HAVE_AUTOAR
		extra_box_widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
		extra_box = GTK_BOX (extra_box_widget);

		extract_box_widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
		extract_box = GTK_BOX (extract_box_widget);
		gtk_box_pack_start (extra_box, extract_box_widget, FALSE, FALSE, 5);

		extract_dont = gtk_radio_button_new_with_mnemonic (NULL,
			_("Do _not extract files from the attachment"));
		gtk_box_pack_start (extract_box, extract_dont, FALSE, FALSE, 0);

		extract_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (extract_dont));
		extract_only = gtk_radio_button_new_with_mnemonic (extract_group,
			_("Save extracted files _only"));
		gtk_box_pack_start (extract_box, extract_only, FALSE, FALSE, 0);

		extract_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (extract_only));
		extract_org = gtk_radio_button_new_with_mnemonic (extract_group,
			_("Save extracted files and the original _archive"));
		gtk_box_pack_start (extract_box, extract_org, FALSE, FALSE, 0);

		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (extract_dont), TRUE);

		gtk_widget_show_all (extra_box_widget);
		gtk_file_chooser_set_extra_widget (file_chooser, extra_box_widget);
#endif
	}

	if (action == GTK_FILE_CHOOSER_ACTION_SAVE) {
		EAttachment *attachment;
		GFileInfo *file_info;
		const gchar *name = NULL;
		gchar *allocated;

#ifdef HAVE_AUTOAR
		gchar *mime_type;
#endif

		attachment = attachment_list->data;
		file_info = e_attachment_ref_file_info (attachment);

		if (file_info != NULL && g_file_info_has_attribute (file_info, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME))
			name = g_file_info_get_display_name (file_info);

		if (name == NULL)
			/* Translators: Default attachment filename. */
			name = _("attachment.dat");

		allocated = g_strdup (name);
		e_util_make_safe_filename (allocated);

		gtk_file_chooser_set_current_name (file_chooser, allocated);

		g_free (allocated);

#ifdef HAVE_AUTOAR
		mime_type = e_attachment_dup_mime_type (attachment);
		if (dialog && !autoar_check_mime_type_supported (mime_type)) {
			gtk_widget_hide (extra_box_widget);
		}

		g_free (mime_type);
#endif

		g_clear_object (&file_info);
#ifdef HAVE_AUTOAR
	} else if (dialog) {
		GList *iter;
		gboolean any_supported = FALSE;

		for (iter = attachment_list; iter && !any_supported; iter = iter->next) {
			EAttachment *attachment = iter->data;
			gchar *mime_type;

			mime_type = e_attachment_dup_mime_type (attachment);

			any_supported = autoar_check_mime_type_supported (mime_type);

			g_free (mime_type);
		}

		gtk_widget_set_visible (extra_box_widget, any_supported);
#endif
	}

	e_util_load_file_chooser_folder (file_chooser);

	if (dialog)
		response = gtk_dialog_run (GTK_DIALOG (dialog));
	else
		response = gtk_native_dialog_run (GTK_NATIVE_DIALOG (native));

	if (response == GTK_RESPONSE_ACCEPT) {
#ifdef HAVE_AUTOAR
		gboolean save_self, save_extracted;
#endif

		e_util_save_file_chooser_folder (file_chooser);
		destination = gtk_file_chooser_get_file (file_chooser);

		if (dialog) {
#ifdef HAVE_AUTOAR
			save_self =
				gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (extract_dont)) ||
				gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (extract_org));
			save_extracted =
				gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (extract_only)) ||
				gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (extract_org));

			if (action == GTK_FILE_CHOOSER_ACTION_SAVE) {
				e_attachment_set_save_self (attachment_list->data, save_self);
				e_attachment_set_save_extracted (attachment_list->data, save_extracted);
			} else {
				GList *iter;

				for (iter = attachment_list; iter != NULL; iter = iter->next) {
					EAttachment *attachment;
					gchar *mime_type;

					attachment = iter->data;
					mime_type = e_attachment_dup_mime_type (attachment);

					if (autoar_check_mime_type_supported (mime_type)) {
						e_attachment_set_save_self (attachment, save_self);
						e_attachment_set_save_extracted (attachment, save_extracted);
					} else {
						e_attachment_set_save_self (attachment, TRUE);
						e_attachment_set_save_extracted (attachment, FALSE);
					}

					g_free (mime_type);
				}
			}
#endif
		}
	} else {
		destination = NULL;
	}

	if (dialog)
		gtk_widget_destroy (dialog);
	else
		g_clear_object (&native);

	return destination;
}

gboolean
e_attachment_store_transform_num_attachments_to_visible_boolean (GBinding *binding,
								 const GValue *from_value,
								 GValue *to_value,
								 gpointer user_data)
{
	gboolean visible;

	g_return_val_if_fail (from_value != NULL, FALSE);
	g_return_val_if_fail (to_value != NULL, FALSE);
	g_return_val_if_fail (G_VALUE_HOLDS_UINT (from_value), FALSE);
	g_return_val_if_fail (G_VALUE_HOLDS_BOOLEAN (to_value), FALSE);

	visible = g_value_get_uint (from_value) != 0;

	if (!visible) {
		GObject *target = g_binding_dup_target (binding);

		if (E_IS_ATTACHMENT_BAR (target))
			visible = e_attachment_bar_get_n_possible_attachments (E_ATTACHMENT_BAR (target)) > 0;
	}

	g_value_set_boolean (to_value, visible);

	return TRUE;
}

gboolean
e_attachment_store_find_attachment_iter (EAttachmentStore *store,
					 EAttachment *attachment,
					 GtkTreeIter *out_iter)
{
	GtkTreeRowReference *reference;
	GtkTreeModel *model;
	GtkTreePath *path;
	gboolean found;

	g_return_val_if_fail (E_IS_ATTACHMENT_STORE (store), FALSE);
	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), FALSE);
	g_return_val_if_fail (out_iter != NULL, FALSE);

	reference = g_hash_table_lookup (store->priv->attachment_index, attachment);

	if (!reference || !gtk_tree_row_reference_valid (reference))
		return FALSE;

	model = gtk_tree_row_reference_get_model (reference);
	g_return_val_if_fail (model == GTK_TREE_MODEL (store), FALSE);

	path = gtk_tree_row_reference_get_path (reference);
	found = gtk_tree_model_get_iter (model, out_iter, path);
	gtk_tree_path_free (path);

	return found;
}

/******************** e_attachment_store_get_uris_async() ********************/

typedef struct _UriContext UriContext;

struct _UriContext {
	GList *attachment_list;
	gchar **uris;
	gint index;
};

static void
attachment_store_uri_context_free (gpointer data)
{
	UriContext *uri_context = data;

	g_list_free_full (uri_context->attachment_list, g_object_unref);
	g_clear_pointer (&uri_context->uris, g_strfreev);

	g_free (uri_context);
}

static void
attachment_store_get_uris_save_cb (GObject *source_object,
                                   GAsyncResult *result,
                                   gpointer user_data)
{
	EAttachment *attachment;
	GTask *task;
	UriContext *uri_context;
	GFile *file;
	GError *error = NULL;

	attachment = E_ATTACHMENT (source_object);
	task = G_TASK (user_data);
	file = e_attachment_save_finish (attachment, result, &error);

	uri_context = g_task_get_task_data (task);

	/* Remove the attachment from the list. */
	uri_context->attachment_list = g_list_remove (
		uri_context->attachment_list, attachment);
	g_object_unref (attachment);

	if (file != NULL) {
		uri_context->uris[uri_context->index++] = g_file_get_uri (file);
		g_object_unref (file);

	} else if (error != NULL) {
		/* If this is the first error, cancel the other jobs. */
		if (!g_task_had_error (task)) {
			g_list_foreach (
				uri_context->attachment_list,
				(GFunc) e_attachment_cancel, NULL);
			g_task_return_error (task, g_steal_pointer (&error));

		/* Otherwise, we can only report back one error.  So if
		 * this is something other than cancellation, dump it to
		 * the terminal. */
		} else if (!g_error_matches (
			error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
			g_warning ("%s", error->message);
	}

	g_clear_error (&error);

	/* If there's still jobs running, let them finish. */
	if (!uri_context->attachment_list && !g_task_had_error (task)) {
		g_task_return_pointer (task,
			g_steal_pointer (&uri_context->uris),
			(GDestroyNotify) g_strfreev);
	}

	g_object_unref (task);
}

void
e_attachment_store_get_uris_async (EAttachmentStore *store,
                                   GList *attachment_list,
                                   GAsyncReadyCallback callback,
                                   gpointer user_data)
{
	GTask *task;
	GFile *temp_directory;
	UriContext *uri_context;
	GList *attachment_list_copy, *iter, *trash = NULL;
	gchar *template;
	gchar *path;
	gchar **uris;
	guint index = 0;

	g_return_if_fail (E_IS_ATTACHMENT_STORE (store));

	attachment_list_copy = g_list_copy_deep (attachment_list, (GCopyFunc) g_object_ref, NULL);
	/* Create a NULL-terminated array */
	uris = g_new0 (gchar *, g_list_length (attachment_list_copy) + 1);
	/* First scan the list for attachments with a GFile. */
	for (iter = attachment_list_copy; iter != NULL; iter = iter->next) {
		EAttachment *attachment = iter->data;
		GFile *file;

		file = e_attachment_ref_file (attachment);
		if (file != NULL) {
			gchar *uri;

			uri = g_file_get_uri (file);
			uris[index++] = g_steal_pointer (&uri);

			/* Mark the list node for deletion. */
			trash = g_list_prepend (trash, iter);
			g_object_unref (attachment);

			g_object_unref (file);
		}
	}

	/* Expunge the list. */
	for (iter = trash; iter != NULL; iter = iter->next) {
		GList *link = iter->data;
		attachment_list_copy = g_list_delete_link (attachment_list_copy, link);
	}
	g_list_free (trash);

	task = g_task_new (store, NULL, callback, user_data);
	g_task_set_source_tag (task, e_attachment_store_get_uris_async);

	/* If we got them all then we're done. */
	if (attachment_list_copy == NULL && !g_task_had_error (task)) {
		g_task_return_pointer (task, g_steal_pointer (&uris), (GDestroyNotify) g_strfreev);
		g_object_unref (task);
		return;
	}

	/* Any remaining attachments in the list should have MIME parts
	 * only, so we need to save them all to a temporary directory.
	 * We use a directory so the files can retain their basenames.
	 * XXX This could trigger a blocking temp directory cleanup. */
	template = g_strdup_printf (PACKAGE "-%s-XXXXXX", g_get_user_name ());
	path = e_mkdtemp (template);
	g_free (template);

	/* XXX Let's hope errno got set properly. */
	if (path == NULL) {
		gint err = errno;
		g_task_return_new_error (task,
			G_FILE_ERROR,
			g_file_error_from_errno (err),
			"%s",
			g_strerror (err));
		g_clear_object (&task);
		g_clear_pointer (&uris, g_strfreev);
		g_list_free_full (attachment_list_copy, g_object_unref);
		return;
	}

	temp_directory = g_file_new_for_path (path);
	g_free (path);

	uri_context = g_new0 (UriContext, 1);
	uri_context->attachment_list = g_steal_pointer (&attachment_list_copy);
	uri_context->uris = g_steal_pointer (&uris);
	uri_context->index = index;
	g_task_set_task_data (task, uri_context, attachment_store_uri_context_free);

	for (iter = uri_context->attachment_list; iter != NULL; iter = iter->next)
		e_attachment_save_async (
			E_ATTACHMENT (iter->data),
			temp_directory,
			attachment_store_get_uris_save_cb,
			g_object_ref (task));

	g_object_unref (temp_directory);
	g_object_unref (task);
}

gchar **
e_attachment_store_get_uris_finish (EAttachmentStore *store,
                                    GAsyncResult *result,
                                    GError **error)
{
	g_return_val_if_fail (g_task_is_valid (result, store), NULL);
	g_return_val_if_fail (g_async_result_is_tagged (result, e_attachment_store_get_uris_async), NULL);

	return g_task_propagate_pointer (G_TASK (result), error);
}

/********************** e_attachment_store_load_async() **********************/

typedef struct _LoadContext LoadContext;

struct _LoadContext {
	GList *attachment_list;
};

static void
attachment_store_load_context_free (gpointer data)
{
	LoadContext *load_context = data;

	g_list_free_full (load_context->attachment_list, g_object_unref);

	g_free (load_context);
}

static void
attachment_store_load_ready_cb (GObject *source_object,
                                GAsyncResult *result,
                                gpointer user_data)
{
	EAttachment *attachment;
	GTask *task;
	LoadContext *load_context;
	GError *error = NULL;

	attachment = E_ATTACHMENT (source_object);
	task = G_TASK (user_data);

	e_attachment_load_finish (attachment, result, &error);

	load_context = g_task_get_task_data (task);
	/* Remove the attachment from the list. */
	load_context->attachment_list = g_list_remove (
		load_context->attachment_list, attachment);

	if (error != NULL) {
		/* If this is the first error, cancel the other jobs. */
		if (!g_task_had_error (task)) {
			g_list_foreach (
				load_context->attachment_list,
				(GFunc) e_attachment_cancel, NULL);
			g_task_return_error (task, g_steal_pointer (&error));

		/* Otherwise, we can only report back one error.  So if
		 * this is something other than cancellation, dump it to
		 * the terminal. */
		} else if (!g_error_matches (
			error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
			g_warning ("%s", error->message);
	}

	g_clear_error (&error);

	/* If there's still jobs running, let them finish. */
	if (!load_context->attachment_list && !g_task_had_error (task))
		g_task_return_boolean (task, TRUE);

	g_object_unref (task);
}

void
e_attachment_store_load_async (EAttachmentStore *store,
                               GList *attachment_list,
                               GAsyncReadyCallback callback,
                               gpointer user_data)
{
	GTask *task;
	LoadContext *load_context;
	GList *iter;

	g_return_if_fail (E_IS_ATTACHMENT_STORE (store));

	task = g_task_new (store, NULL, callback, user_data);
	g_task_set_source_tag (task, e_attachment_store_load_async);

	if (attachment_list == NULL) {
		g_task_return_boolean (task, TRUE);
		g_object_unref (task);
		return;
	}

	load_context = g_new0 (LoadContext, 1);
	load_context->attachment_list = g_list_copy_deep (attachment_list, (GCopyFunc) g_object_ref, NULL);
	g_task_set_task_data (task, load_context, attachment_store_load_context_free);
	for (iter = attachment_list; iter != NULL; iter = iter->next) {
		EAttachment *attachment = E_ATTACHMENT (iter->data);

		e_attachment_store_add_attachment (store, attachment);

		e_attachment_load_async (
			attachment, (GAsyncReadyCallback)
			attachment_store_load_ready_cb,
			g_object_ref (task));
	}

	g_object_unref (task);
}

gboolean
e_attachment_store_load_finish (EAttachmentStore *store,
                                GAsyncResult *result,
                                GError **error)
{
	g_return_val_if_fail (g_task_is_valid (result, store), FALSE);
	g_return_val_if_fail (g_async_result_is_tagged (result, e_attachment_store_load_async), FALSE);

	return g_task_propagate_boolean (G_TASK (result), error);
}

/********************** e_attachment_store_save_async() **********************/

typedef struct _SaveContext SaveContext;

struct _SaveContext {
	GFile *destination;
	gchar *filename_prefix;
	GFile *fresh_directory;
	GFile *trash_directory;
	GList *attachment_list;
	gchar **uris;
	gint index;
};

static void
attachment_store_save_context_free (SaveContext *save_context)
{
	g_file_delete (save_context->fresh_directory, NULL, NULL);

	/* The attachment list should be empty now. */
	g_warn_if_fail (save_context->attachment_list == NULL);
	g_list_free_full (save_context->attachment_list, g_object_unref);

	g_clear_object (&save_context->destination);
	g_clear_object (&save_context->fresh_directory);
	g_clear_object (&save_context->trash_directory);
	g_clear_pointer (&save_context->filename_prefix, g_free);
	g_clear_pointer (&save_context->uris, g_strfreev);
	g_free (save_context);
}

static void
attachment_store_move_file (SaveContext *save_context,
                            GFile *source,
                            GFile *destination,
                            GError **error)
{
	gchar *tmpl;
	gchar *path;
	GError *local_error = NULL;

	g_return_if_fail (save_context != NULL);
	g_return_if_fail (source != NULL);
	g_return_if_fail (destination != NULL);
	g_return_if_fail (error != NULL);

	/* Attachments are all saved to a temporary directory.
	 * Now we need to move the existing destination directory
	 * out of the way (if it exists).  Instead of testing for
	 * existence we'll just attempt the move and ignore any
	 * G_IO_ERROR_NOT_FOUND errors. */

	/* First, however, we need another temporary directory to
	 * move the existing destination directory to.  Note we're
	 * not actually creating the directory yet, just picking a
	 * name for it.  The usual raciness with this approach
	 * applies here (read up on mktemp(3)), but worst case is
	 * we get a spurious G_IO_ERROR_WOULD_MERGE error and the
	 * user has to try saving attachments again. */
	tmpl = g_strdup_printf (PACKAGE "-%s-XXXXXX", g_get_user_name ());
	path = e_mkdtemp (tmpl);
	g_free (tmpl);

	save_context->trash_directory = g_file_new_for_path (path);
	g_free (path);

	/* XXX No asynchronous move operation in GIO? */
	g_file_move (
		destination,
		save_context->trash_directory,
		G_FILE_COPY_NONE, NULL, NULL,
		NULL, &local_error);

	if (g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
		g_clear_error (&local_error);

	if (local_error != NULL) {
		g_propagate_error (error, local_error);
		return;
	}

	/* Now we can move the file from the temporary directory
	 * to the user-specified destination. */
	g_file_move (
		source,
		destination,
		G_FILE_COPY_NONE, NULL, NULL, NULL, error);
}

static void
attachment_store_save_cb (GObject *source_object,
                          GAsyncResult *result,
                          gpointer user_data)
{
	EAttachment *attachment;
	SaveContext *save_context;
	GTask *task;
	GFile *file;
	GError *error = NULL;

	attachment = E_ATTACHMENT (source_object);
	task = G_TASK (user_data);
	file = e_attachment_save_finish (attachment, result, &error);

	save_context = g_task_get_task_data (task);
	/* Remove the attachment from the list. */
	save_context->attachment_list = g_list_remove (
		save_context->attachment_list, attachment);
	g_object_unref (attachment);

	if (file != NULL) {
		/* Assemble the file's final URI from its basename. */
		GFile *source = NULL;
		GFile *destination = NULL;
		gchar *basename;
		const gchar *prefix;

		basename = g_file_get_basename (file);
		g_clear_object (&file);

		source = g_file_get_child (
			save_context->fresh_directory, basename);

		prefix = save_context->filename_prefix;

		if (prefix != NULL && *prefix != '\0') {
			gchar *tmp = basename;
			basename = g_strconcat (prefix, basename, NULL);
			g_free (tmp);
		}

		file = save_context->destination;
		destination = g_file_get_child (file, basename);

		/* move them file-by-file */
		attachment_store_move_file (
			save_context, source, destination, &error);

		if (error == NULL)
			save_context->uris[save_context->index++] = g_file_get_uri (destination);

		g_object_unref (source);
		g_object_unref (destination);
	}

	if (error != NULL) {
		/* If this is the first error, cancel the other jobs. */
		if (!g_task_had_error (task)) {
			g_list_foreach (
				save_context->attachment_list,
				(GFunc) e_attachment_cancel, NULL);
			g_task_return_error (task, g_steal_pointer (&error));

		/* Otherwise, we can only report back one error.  So if
		 * this is something other than cancellation, dump it to
		 * the terminal. */
		} else if (!g_error_matches (
			error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
			g_warning ("%s", error->message);
	}

	g_clear_error (&error);

	/* If there's still jobs running, let them finish. */
	if (!save_context->attachment_list && !g_task_had_error (task))
		g_task_return_pointer (task,
			g_steal_pointer (&save_context->uris),
			(GDestroyNotify) g_strfreev);
	g_object_unref (task);
}
/*
 * @filename_prefix: prefix to use for a file name; can be %NULL for none
 **/
void
e_attachment_store_save_async (EAttachmentStore *store,
                               GFile *destination,
                               const gchar *filename_prefix,
                               GAsyncReadyCallback callback,
                               gpointer user_data)
{
	SaveContext *save_context;
	GList *attachment_list, *iter;
	GTask *task;
	GFile *temp_directory;
	gchar *template;
	gchar *path;

	g_return_if_fail (E_IS_ATTACHMENT_STORE (store));
	g_return_if_fail (G_IS_FILE (destination));

	task = g_task_new (store, NULL, callback, user_data);
	g_task_set_source_tag (task, e_attachment_store_save_async);

	attachment_list = e_attachment_store_get_attachments (store);

	/* Deal with an empty attachment store.  The caller will get
	 * an empty NULL-terminated list as opposed to NULL, to help
	 * distinguish it from an error. */
	if (attachment_list == NULL) {
		g_task_return_pointer (task, NULL, NULL);
		g_object_unref (task);
		return;
	}

	/* Save all attachments to a temporary directory, which we'll
	 * then move to its proper location.  We use a directory so
	 * files can retain their basenames.
	 * XXX This could trigger a blocking temp directory cleanup. */
	template = g_strdup_printf (PACKAGE "-%s-XXXXXX", g_get_user_name ());
	path = e_mkdtemp (template);
	g_free (template);

	/* XXX Let's hope errno got set properly. */
	if (path == NULL) {
		gint err = errno;
		g_task_return_new_error (
			task, G_FILE_ERROR,
			g_file_error_from_errno (err),
			"%s", g_strerror (err));
		g_object_unref (task);
		g_list_free_full (attachment_list, g_object_unref);
		return;
	}

	save_context = g_new0 (SaveContext, 1);
	save_context->destination = g_object_ref (destination);
	save_context->filename_prefix = g_strdup (filename_prefix);
	save_context->attachment_list = g_steal_pointer (&attachment_list);
	/* Add one for NULL terminator. */
	save_context->uris = g_new0 (gchar *, g_list_length (save_context->attachment_list) + 1);

	temp_directory = g_file_new_for_path (path);
	save_context->fresh_directory = temp_directory;
	g_free (path);

	g_task_set_task_data (task, save_context, (GDestroyNotify) attachment_store_save_context_free);
	for (iter = save_context->attachment_list; iter != NULL; iter = iter->next)
		e_attachment_save_async (
			E_ATTACHMENT (iter->data), temp_directory,
			attachment_store_save_cb, g_object_ref (task));

	g_object_unref (task);
}

gchar **
e_attachment_store_save_finish (EAttachmentStore *store,
                                GAsyncResult *result,
                                GError **error)
{
	g_return_val_if_fail (E_IS_ATTACHMENT_STORE (store), NULL);
	g_return_val_if_fail (g_task_is_valid (result, store), NULL);
	g_return_val_if_fail (g_async_result_is_tagged (result, e_attachment_store_save_async), NULL);

	return g_task_propagate_pointer (G_TASK (result), error);
}
