/*
 * e-attachment.c
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

#include "e-attachment.h"

#include <errno.h>
#include <config.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <camel/camel-iconv.h>
#include <camel/camel-data-wrapper.h>
#include <camel/camel-mime-message.h>
#include <camel/camel-stream-filter.h>
#include <camel/camel-stream-mem.h>
#include <camel/camel-stream-null.h>
#include <camel/camel-stream-vfs.h>
#include <camel/camel-stream-fs.h>

#include <libedataserver/e-data-server-util.h>

#include "e-util/e-icon-factory.h"
#include "e-util/e-util.h"
#include "e-util/e-mktemp.h"
#include "e-attachment-store.h"

#define E_ATTACHMENT_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_ATTACHMENT, EAttachmentPrivate))

/* Fallback Icon */
#define DEFAULT_ICON_NAME	"mail-attachment"

/* Emblems */
#define EMBLEM_CANCELLED	"gtk-cancel"
#define EMBLEM_LOADING		"emblem-downloads"
#define EMBLEM_SAVING		"document-save"
#define EMBLEM_ENCRYPT_WEAK	"security-low"
#define EMBLEM_ENCRYPT_STRONG	"security-high"
#define EMBLEM_ENCRYPT_UNKNOWN	"security-medium"
#define EMBLEM_SIGN_BAD		"stock_signature_bad"
#define EMBLEM_SIGN_GOOD	"stock_signature-ok"
#define EMBLEM_SIGN_UNKNOWN	"stock_signature"

/* Attributes needed for EAttachmentStore columns. */
#define ATTACHMENT_QUERY "standard::*,preview::*,thumbnail::*"

struct _EAttachmentPrivate {
	GFile *file;
	GFileInfo *file_info;
	GCancellable *cancellable;
	CamelMimePart *mime_part;
	guint emblem_timeout_id;
	gchar *disposition;
	gint percent;

	guint can_show : 1;
	guint loading  : 1;
	guint saving   : 1;
	guint shown    : 1;

	camel_cipher_validity_encrypt_t encrypted;
	camel_cipher_validity_sign_t signed_;

	/* This is a reference to our row in an EAttachmentStore,
	 * serving as a means of broadcasting "row-changed" signals.
	 * If we are removed from the store, we lazily free the
	 * reference when it is found to be to be invalid. */
	GtkTreeRowReference *reference;
};

enum {
	PROP_0,
	PROP_CAN_SHOW,
	PROP_DISPOSITION,
	PROP_ENCRYPTED,
	PROP_FILE,
	PROP_FILE_INFO,
	PROP_LOADING,
	PROP_MIME_PART,
	PROP_PERCENT,
	PROP_REFERENCE,
	PROP_SAVING,
	PROP_SHOWN,
	PROP_SIGNED
};

static gpointer parent_class;

static gboolean
create_system_thumbnail (EAttachment *attachment, GIcon **icon)
{
	GFile *file;
	gchar *thumbnail = NULL;

	g_return_val_if_fail (attachment != NULL, FALSE);
	g_return_val_if_fail (icon != NULL, FALSE);

	file = e_attachment_get_file (attachment);

	if (file && g_file_has_uri_scheme (file, "file")) {
		gchar *path = g_file_get_path (file);
		if (path) {
			thumbnail = e_icon_factory_create_thumbnail (path);
			g_free (path);
		}
	}

	if (thumbnail) {
		GFile *gf = g_file_new_for_path (thumbnail);

		g_return_val_if_fail (gf != NULL, FALSE);
		if (*icon)
			g_object_unref (*icon);

		*icon = g_file_icon_new (gf);
		g_object_unref (gf);

		if (file) {
			GFileInfo *fi = e_attachment_get_file_info (attachment);

			if (fi)
				g_file_info_set_attribute_byte_string (fi, G_FILE_ATTRIBUTE_THUMBNAIL_PATH, thumbnail);
		}
	}

	g_free (thumbnail);

	return thumbnail != NULL;
}

static gchar *
attachment_get_default_charset (void)
{
	GConfClient *client;
	const gchar *key;
	gchar *charset;

	/* XXX This doesn't really belong here. */

	client = gconf_client_get_default ();
	key = "/apps/evolution/mail/composer/charset";
	charset = gconf_client_get_string (client, key, NULL);
	if (charset == NULL || *charset == '\0') {
		g_free (charset);
		key = "/apps/evolution/mail/format/charset";
		charset = gconf_client_get_string (client, key, NULL);
		if (charset == NULL || *charset == '\0') {
			g_free (charset);
			charset = NULL;
		}
	}
	g_object_unref (client);

	if (charset == NULL)
		charset = g_strdup (camel_iconv_locale_charset ());

	if (charset == NULL)
		charset = g_strdup ("us-ascii");

	return charset;
}

static void
attachment_update_file_info_columns (EAttachment *attachment)
{
	GtkTreeRowReference *reference;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	GFileInfo *file_info;
	const gchar *content_type;
	const gchar *description;
	const gchar *display_name;
	gchar *content_desc;
	gchar *display_size;
	gchar *caption;
	goffset size;

	reference = e_attachment_get_reference (attachment);
	if (!gtk_tree_row_reference_valid (reference))
		return;

	file_info = e_attachment_get_file_info (attachment);
	if (file_info == NULL)
		return;

	model = gtk_tree_row_reference_get_model (reference);
	path = gtk_tree_row_reference_get_path (reference);
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_path_free (path);

	content_type = g_file_info_get_content_type (file_info);
	display_name = g_file_info_get_display_name (file_info);
	size = g_file_info_get_size (file_info);

	content_desc = g_content_type_get_description (content_type);
	display_size = g_format_size_for_display (size);

	description = e_attachment_get_description (attachment);
	if (description == NULL || *description == '\0')
		description = display_name;

	if (size > 0)
		caption = g_strdup_printf (
			"%s\n(%s)", description, display_size);
	else
		caption = g_strdup (description);

	gtk_list_store_set (
		GTK_LIST_STORE (model), &iter,
		E_ATTACHMENT_STORE_COLUMN_CAPTION, caption,
		E_ATTACHMENT_STORE_COLUMN_CONTENT_TYPE, content_desc,
		E_ATTACHMENT_STORE_COLUMN_DESCRIPTION, description,
		E_ATTACHMENT_STORE_COLUMN_SIZE, size,
		-1);

	g_free (content_desc);
	g_free (display_size);
	g_free (caption);
}

static void
attachment_update_icon_column (EAttachment *attachment)
{
	GtkTreeRowReference *reference;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	GFileInfo *file_info;
	GCancellable *cancellable;
	GIcon *icon = NULL;
	const gchar *emblem_name = NULL;
	const gchar *thumbnail_path = NULL;

	reference = e_attachment_get_reference (attachment);
	if (!gtk_tree_row_reference_valid (reference))
		return;

	model = gtk_tree_row_reference_get_model (reference);
	path = gtk_tree_row_reference_get_path (reference);
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_path_free (path);

	cancellable = attachment->priv->cancellable;
	file_info = e_attachment_get_file_info (attachment);

	if (file_info != NULL) {
		icon = g_file_info_get_icon (file_info);
		thumbnail_path = g_file_info_get_attribute_byte_string (
			file_info, G_FILE_ATTRIBUTE_THUMBNAIL_PATH);
	}

	/* Prefer the thumbnail if we have one. */
	if (thumbnail_path != NULL && *thumbnail_path != '\0') {
		GFile *file;

		file = g_file_new_for_path (thumbnail_path);
		icon = g_file_icon_new (file);
		g_object_unref (file);

	/* try the system thumbnailer */
	} else if (create_system_thumbnail (attachment, &icon)) {
		/* actually do nothing, just use the icon */

	/* Else use the standard icon for the content type. */
	} else if (icon != NULL)
		g_object_ref (icon);

	/* Last ditch fallback.  (GFileInfo not yet loaded?) */
	else
		icon = g_themed_icon_new (DEFAULT_ICON_NAME);

	/* Pick an emblem, limit one.  Choices listed by priority. */

	if (g_cancellable_is_cancelled (cancellable))
		emblem_name = EMBLEM_CANCELLED;

	else if (e_attachment_get_loading (attachment))
		emblem_name = EMBLEM_LOADING;

	else if (e_attachment_get_saving (attachment))
		emblem_name = EMBLEM_SAVING;

	else if (e_attachment_get_encrypted (attachment))
		switch (e_attachment_get_encrypted (attachment)) {
			case CAMEL_CIPHER_VALIDITY_ENCRYPT_WEAK:
				emblem_name = EMBLEM_ENCRYPT_WEAK;
				break;

			case CAMEL_CIPHER_VALIDITY_ENCRYPT_ENCRYPTED:
				emblem_name = EMBLEM_ENCRYPT_UNKNOWN;
				break;

			case CAMEL_CIPHER_VALIDITY_ENCRYPT_STRONG:
				emblem_name = EMBLEM_ENCRYPT_STRONG;
				break;

			default:
				g_warn_if_reached ();
				break;
		}

	else if (e_attachment_get_signed (attachment))
		switch (e_attachment_get_signed (attachment)) {
			case CAMEL_CIPHER_VALIDITY_SIGN_GOOD:
				emblem_name = EMBLEM_SIGN_GOOD;
				break;

			case CAMEL_CIPHER_VALIDITY_SIGN_BAD:
				emblem_name = EMBLEM_SIGN_BAD;
				break;

			case CAMEL_CIPHER_VALIDITY_SIGN_UNKNOWN:
			case CAMEL_CIPHER_VALIDITY_SIGN_NEED_PUBLIC_KEY:
				emblem_name = EMBLEM_SIGN_UNKNOWN;
				break;

			default:
				g_warn_if_reached ();
				break;
		}

	if (emblem_name != NULL) {
		GIcon *emblemed_icon;
		GEmblem *emblem;

		emblemed_icon = g_themed_icon_new (emblem_name);
		emblem = g_emblem_new (emblemed_icon);
		g_object_unref (emblemed_icon);

		emblemed_icon = g_emblemed_icon_new (icon, emblem);
		g_object_unref (emblem);
		g_object_unref (icon);

		icon = emblemed_icon;
	}

	gtk_list_store_set (
		GTK_LIST_STORE (model), &iter,
		E_ATTACHMENT_STORE_COLUMN_ICON, icon,
		-1);

	g_object_unref (icon);
}

static void
attachment_update_progress_columns (EAttachment *attachment)
{
	GtkTreeRowReference *reference;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	gboolean loading;
	gboolean saving;
	gint percent;

	reference = e_attachment_get_reference (attachment);
	if (!gtk_tree_row_reference_valid (reference))
		return;

	model = gtk_tree_row_reference_get_model (reference);
	path = gtk_tree_row_reference_get_path (reference);
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_path_free (path);

	/* Don't show progress bars until we have progress to report. */
	percent = e_attachment_get_percent (attachment);
	loading = e_attachment_get_loading (attachment) && (percent > 0);
	saving = e_attachment_get_saving (attachment) && (percent > 0);

	gtk_list_store_set (
		GTK_LIST_STORE (model), &iter,
		E_ATTACHMENT_STORE_COLUMN_LOADING, loading,
		E_ATTACHMENT_STORE_COLUMN_PERCENT, percent,
		E_ATTACHMENT_STORE_COLUMN_SAVING, saving,
		-1);
}

static void
attachment_set_file_info (EAttachment *attachment,
                          GFileInfo *file_info)
{
	GtkTreeRowReference *reference;
	GIcon *icon;

	reference = e_attachment_get_reference (attachment);

	if (file_info != NULL)
		g_object_ref (file_info);

	if (attachment->priv->file_info != NULL)
		g_object_unref (attachment->priv->file_info);

	attachment->priv->file_info = file_info;

	/* If the GFileInfo contains a GThemedIcon, append a
	 * fallback icon name to ensure we display something. */
	icon = g_file_info_get_icon (file_info);
	if (G_IS_THEMED_ICON (icon))
		g_themed_icon_append_name (
			G_THEMED_ICON (icon), DEFAULT_ICON_NAME);

	g_object_notify (G_OBJECT (attachment), "file-info");

	/* Tell the EAttachmentStore its total size changed. */
	if (gtk_tree_row_reference_valid (reference)) {
		GtkTreeModel *model;
		model = gtk_tree_row_reference_get_model (reference);
		g_object_notify (G_OBJECT (model), "total-size");
	}
}

static void
attachment_set_loading (EAttachment *attachment,
                        gboolean loading)
{
	GtkTreeRowReference *reference;

	reference = e_attachment_get_reference (attachment);

	attachment->priv->percent = 0;
	attachment->priv->loading = loading;

	g_object_freeze_notify (G_OBJECT (attachment));
	g_object_notify (G_OBJECT (attachment), "percent");
	g_object_notify (G_OBJECT (attachment), "loading");
	g_object_thaw_notify (G_OBJECT (attachment));

	if (gtk_tree_row_reference_valid (reference)) {
		GtkTreeModel *model;
		model = gtk_tree_row_reference_get_model (reference);
		g_object_notify (G_OBJECT (model), "num-loading");
	}
}

static void
attachment_set_saving (EAttachment *attachment,
                       gboolean saving)
{
	attachment->priv->percent = 0;
	attachment->priv->saving = saving;

	g_object_freeze_notify (G_OBJECT (attachment));
	g_object_notify (G_OBJECT (attachment), "percent");
	g_object_notify (G_OBJECT (attachment), "saving");
	g_object_thaw_notify (G_OBJECT (attachment));
}

static void
attachment_progress_cb (goffset current_num_bytes,
                        goffset total_num_bytes,
                        EAttachment *attachment)
{
	/* Avoid dividing by zero. */
	if (total_num_bytes == 0)
		return;

	attachment->priv->percent =
		(current_num_bytes * 100) / total_num_bytes;

	g_object_notify (G_OBJECT (attachment), "percent");
}

static gboolean
attachment_cancelled_timeout_cb (EAttachment *attachment)
{
	attachment->priv->emblem_timeout_id = 0;
	g_cancellable_reset (attachment->priv->cancellable);

	attachment_update_icon_column (attachment);

	return FALSE;
}

static void
attachment_cancelled_cb (EAttachment *attachment)
{
	/* Reset the GCancellable after one second.  This causes a
	 * cancel emblem to be briefly shown on the attachment icon
	 * as visual feedback that an operation was cancelled. */

	if (attachment->priv->emblem_timeout_id > 0)
		g_source_remove (attachment->priv->emblem_timeout_id);

	attachment->priv->emblem_timeout_id = g_timeout_add_seconds (
		1, (GSourceFunc) attachment_cancelled_timeout_cb, attachment);

	attachment_update_icon_column (attachment);
}

static void
attachment_set_property (GObject *object,
                         guint property_id,
                         const GValue *value,
                         GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CAN_SHOW:
			e_attachment_set_can_show (
				E_ATTACHMENT (object),
				g_value_get_boolean (value));
			return;

		case PROP_DISPOSITION:
			e_attachment_set_disposition (
				E_ATTACHMENT (object),
				g_value_get_string (value));
			return;

		case PROP_ENCRYPTED:
			e_attachment_set_encrypted (
				E_ATTACHMENT (object),
				g_value_get_int (value));
			return;

		case PROP_FILE:
			e_attachment_set_file (
				E_ATTACHMENT (object),
				g_value_get_object (value));
			return;

		case PROP_SHOWN:
			e_attachment_set_shown (
				E_ATTACHMENT (object),
				g_value_get_boolean (value));
			return;

		case PROP_MIME_PART:
			e_attachment_set_mime_part (
				E_ATTACHMENT (object),
				g_value_get_boxed (value));
			return;

		case PROP_REFERENCE:
			e_attachment_set_reference (
				E_ATTACHMENT (object),
				g_value_get_boxed (value));
			return;

		case PROP_SIGNED:
			e_attachment_set_signed (
				E_ATTACHMENT (object),
				g_value_get_int (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
attachment_get_property (GObject *object,
                         guint property_id,
                         GValue *value,
                         GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CAN_SHOW:
			g_value_set_boolean (
				value, e_attachment_get_can_show (
				E_ATTACHMENT (object)));
			return;

		case PROP_DISPOSITION:
			g_value_set_string (
				value, e_attachment_get_disposition (
				E_ATTACHMENT (object)));
			return;

		case PROP_ENCRYPTED:
			g_value_set_int (
				value, e_attachment_get_encrypted (
				E_ATTACHMENT (object)));
			return;

		case PROP_FILE:
			g_value_set_object (
				value, e_attachment_get_file (
				E_ATTACHMENT (object)));
			return;

		case PROP_FILE_INFO:
			g_value_set_object (
				value, e_attachment_get_file_info (
				E_ATTACHMENT (object)));
			return;

		case PROP_SHOWN:
			g_value_set_boolean (
				value, e_attachment_get_shown (
				E_ATTACHMENT (object)));
			return;

		case PROP_LOADING:
			g_value_set_boolean (
				value, e_attachment_get_loading (
				E_ATTACHMENT (object)));
			return;

		case PROP_MIME_PART:
			g_value_set_boxed (
				value, e_attachment_get_mime_part (
				E_ATTACHMENT (object)));
			return;

		case PROP_PERCENT:
			g_value_set_int (
				value, e_attachment_get_percent (
				E_ATTACHMENT (object)));
			return;

		case PROP_REFERENCE:
			g_value_set_boxed (
				value, e_attachment_get_reference (
				E_ATTACHMENT (object)));
			return;

		case PROP_SAVING:
			g_value_set_boolean (
				value, e_attachment_get_saving (
				E_ATTACHMENT (object)));
			return;

		case PROP_SIGNED:
			g_value_set_int (
				value, e_attachment_get_signed (
				E_ATTACHMENT (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
attachment_dispose (GObject *object)
{
	EAttachmentPrivate *priv;

	priv = E_ATTACHMENT_GET_PRIVATE (object);

	if (priv->file != NULL) {
		g_object_unref (priv->file);
		priv->file = NULL;
	}

	if (priv->file_info != NULL) {
		g_object_unref (priv->file_info);
		priv->file_info = NULL;
	}

	if (priv->cancellable != NULL) {
		g_object_unref (priv->cancellable);
		priv->cancellable = NULL;
	}

	if (priv->mime_part != NULL) {
		camel_object_unref (priv->mime_part);
		priv->mime_part = NULL;
	}

	if (priv->emblem_timeout_id > 0) {
		g_source_remove (priv->emblem_timeout_id);
		priv->emblem_timeout_id = 0;
	}

	/* This accepts NULL arguments. */
	gtk_tree_row_reference_free (priv->reference);
	priv->reference = NULL;

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
attachment_finalize (GObject *object)
{
	EAttachmentPrivate *priv;

	priv = E_ATTACHMENT_GET_PRIVATE (object);

	g_free (priv->disposition);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
attachment_class_init (EAttachmentClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EAttachmentPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = attachment_set_property;
	object_class->get_property = attachment_get_property;
	object_class->dispose = attachment_dispose;
	object_class->finalize = attachment_finalize;

	g_object_class_install_property (
		object_class,
		PROP_CAN_SHOW,
		g_param_spec_boolean (
			"can-show",
			"Can Show",
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_DISPOSITION,
		g_param_spec_string (
			"disposition",
			"Disposition",
			NULL,
			"attachment",
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	/* FIXME Define a GEnumClass for this. */
	g_object_class_install_property (
		object_class,
		PROP_ENCRYPTED,
		g_param_spec_int (
			"encrypted",
			"Encrypted",
			NULL,
			CAMEL_CIPHER_VALIDITY_ENCRYPT_NONE,
			CAMEL_CIPHER_VALIDITY_ENCRYPT_STRONG,
			CAMEL_CIPHER_VALIDITY_ENCRYPT_NONE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_FILE,
		g_param_spec_object (
			"file",
			"File",
			NULL,
			G_TYPE_FILE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_FILE_INFO,
		g_param_spec_object (
			"file-info",
			"File Info",
			NULL,
			G_TYPE_FILE_INFO,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_LOADING,
		g_param_spec_boolean (
			"loading",
			"Loading",
			NULL,
			FALSE,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_MIME_PART,
		g_param_spec_boxed (
			"mime-part",
			"MIME Part",
			NULL,
			E_TYPE_CAMEL_OBJECT,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_PERCENT,
		g_param_spec_int (
			"percent",
			"Percent",
			NULL,
			0,
			100,
			0,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_REFERENCE,
		g_param_spec_boxed (
			"reference",
			"Reference",
			NULL,
			GTK_TYPE_TREE_ROW_REFERENCE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_SAVING,
		g_param_spec_boolean (
			"saving",
			"Saving",
			NULL,
			FALSE,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_SHOWN,
		g_param_spec_boolean (
			"shown",
			"Shown",
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	/* FIXME Define a GEnumClass for this. */
	g_object_class_install_property (
		object_class,
		PROP_SIGNED,
		g_param_spec_int (
			"signed",
			"Signed",
			NULL,
			CAMEL_CIPHER_VALIDITY_SIGN_NONE,
			CAMEL_CIPHER_VALIDITY_SIGN_NEED_PUBLIC_KEY,
			CAMEL_CIPHER_VALIDITY_SIGN_NONE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));
}

static void
attachment_init (EAttachment *attachment)
{
	attachment->priv = E_ATTACHMENT_GET_PRIVATE (attachment);
	attachment->priv->cancellable = g_cancellable_new ();
	attachment->priv->encrypted = CAMEL_CIPHER_VALIDITY_ENCRYPT_NONE;
	attachment->priv->signed_ = CAMEL_CIPHER_VALIDITY_SIGN_NONE;

	g_signal_connect (
		attachment, "notify::encrypted",
		G_CALLBACK (attachment_update_icon_column), NULL);

	g_signal_connect (
		attachment, "notify::file-info",
		G_CALLBACK (attachment_update_file_info_columns), NULL);

	g_signal_connect (
		attachment, "notify::file-info",
		G_CALLBACK (attachment_update_icon_column), NULL);

	g_signal_connect (
		attachment, "notify::loading",
		G_CALLBACK (attachment_update_icon_column), NULL);

	g_signal_connect (
		attachment, "notify::loading",
		G_CALLBACK (attachment_update_progress_columns), NULL);

	g_signal_connect (
		attachment, "notify::percent",
		G_CALLBACK (attachment_update_progress_columns), NULL);

	g_signal_connect (
		attachment, "notify::reference",
		G_CALLBACK (attachment_update_file_info_columns), NULL);

	g_signal_connect (
		attachment, "notify::reference",
		G_CALLBACK (attachment_update_icon_column), NULL);

	g_signal_connect (
		attachment, "notify::reference",
		G_CALLBACK (attachment_update_progress_columns), NULL);

	g_signal_connect (
		attachment, "notify::saving",
		G_CALLBACK (attachment_update_icon_column), NULL);

	g_signal_connect (
		attachment, "notify::saving",
		G_CALLBACK (attachment_update_progress_columns), NULL);

	g_signal_connect (
		attachment, "notify::signed",
		G_CALLBACK (attachment_update_icon_column), NULL);

	g_signal_connect_swapped (
		attachment->priv->cancellable, "cancelled",
		G_CALLBACK (attachment_cancelled_cb), attachment);
}

GType
e_attachment_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EAttachmentClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) attachment_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EAttachment),
			0,     /* n_preallocs */
			(GInstanceInitFunc) attachment_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			G_TYPE_OBJECT, "EAttachment", &type_info, 0);
	}

	return type;
}

EAttachment *
e_attachment_new (void)
{
	return g_object_new (E_TYPE_ATTACHMENT, NULL);
}

EAttachment *
e_attachment_new_for_path (const gchar *path)
{
	EAttachment *attachment;
	GFile *file;

	g_return_val_if_fail (path != NULL, NULL);

	file = g_file_new_for_path (path);
	attachment = g_object_new (E_TYPE_ATTACHMENT, "file", file, NULL);
	g_object_unref (file);

	return attachment;
}

EAttachment *
e_attachment_new_for_uri (const gchar *uri)
{
	EAttachment *attachment;
	GFile *file;

	g_return_val_if_fail (uri != NULL, NULL);

	file = g_file_new_for_uri (uri);
	attachment = g_object_new (E_TYPE_ATTACHMENT, "file", file, NULL);
	g_object_unref (file);

	return attachment;
}

EAttachment *
e_attachment_new_for_message (CamelMimeMessage *message)
{
	CamelDataWrapper *wrapper;
	CamelMimePart *mime_part;
	EAttachment *attachment;
	GString *description;
	const gchar *subject;

	g_return_val_if_fail (CAMEL_IS_MIME_MESSAGE (message), NULL);

	mime_part = camel_mime_part_new ();
	camel_mime_part_set_disposition (mime_part, "inline");
	subject = camel_mime_message_get_subject (message);

	description = g_string_new (_("Attached message"));
	if (subject != NULL)
		g_string_append_printf (description, " - %s", subject);
	camel_mime_part_set_description (mime_part, description->str);
	g_string_free (description, TRUE);

	wrapper = CAMEL_DATA_WRAPPER (message);
	camel_medium_set_content_object (CAMEL_MEDIUM (mime_part), wrapper);
	camel_mime_part_set_content_type (mime_part, "message/rfc822");

	attachment = e_attachment_new ();
	e_attachment_set_mime_part (attachment, mime_part);
	camel_object_unref (mime_part);

	return attachment;
}

void
e_attachment_add_to_multipart (EAttachment *attachment,
                               CamelMultipart *multipart,
                               const gchar *default_charset)
{
	CamelContentType *content_type;
	CamelDataWrapper *wrapper;
	CamelMimePart *mime_part;

	/* XXX EMsgComposer might be a better place for this function. */

	g_return_if_fail (E_IS_ATTACHMENT (attachment));
	g_return_if_fail (CAMEL_IS_MULTIPART (multipart));

	/* Still loading?  Too bad. */
	mime_part = e_attachment_get_mime_part (attachment);
	if (mime_part == NULL)
		return;

	content_type = camel_mime_part_get_content_type (mime_part);
	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));

	if (CAMEL_IS_MULTIPART (wrapper))
		goto exit;

	/* For text content, determine the best encoding and character set. */
	if (camel_content_type_is (content_type, "text", "*")) {
		CamelTransferEncoding encoding;
		CamelStreamFilter *filtered_stream;
		CamelMimeFilterBestenc *filter;
		CamelStream *stream;
		const gchar *charset;

		charset = camel_content_type_param (content_type, "charset");

		/* Determine the best encoding by writing the MIME
		 * part to a NULL stream with a "bestenc" filter. */
		stream = camel_stream_null_new ();
		filtered_stream = camel_stream_filter_new_with_stream (stream);
		filter = camel_mime_filter_bestenc_new (
			CAMEL_BESTENC_GET_ENCODING);
		camel_stream_filter_add (
			filtered_stream, CAMEL_MIME_FILTER (filter));
		camel_data_wrapper_decode_to_stream (
			wrapper, CAMEL_STREAM (filtered_stream));
		camel_object_unref (filtered_stream);
		camel_object_unref (stream);

		/* Retrieve the best encoding from the filter. */
		encoding = camel_mime_filter_bestenc_get_best_encoding (
			filter, CAMEL_BESTENC_8BIT);
		camel_mime_part_set_encoding (mime_part, encoding);
		camel_object_unref (filter);

		if (encoding == CAMEL_TRANSFER_ENCODING_7BIT) {
			/* The text fits within us-ascii, so this is safe.
			 * FIXME Check that this isn't iso-2022-jp? */
			default_charset = "us-ascii";

		} else if (charset == NULL && default_charset == NULL) {
			default_charset = attachment_get_default_charset ();
			/* FIXME Check that this fits within the
			 *       default_charset and if not, find one
			 *       that does and/or allow the user to
			 *       specify. */
		}

		if (charset == NULL) {
			gchar *type;

			camel_content_type_set_param (
				content_type, "charset", default_charset);
			type = camel_content_type_format (content_type);
			camel_mime_part_set_content_type (mime_part, type);
			g_free (type);
		}

	/* Otherwise, unless it's a message/rfc822, Base64 encode it. */
	} else if (!CAMEL_IS_MIME_MESSAGE (wrapper))
		camel_mime_part_set_encoding (
			mime_part, CAMEL_TRANSFER_ENCODING_BASE64);

exit:
	camel_multipart_add_part (multipart, mime_part);
}

void
e_attachment_cancel (EAttachment *attachment)
{
	g_return_if_fail (E_IS_ATTACHMENT (attachment));

	g_cancellable_cancel (attachment->priv->cancellable);
}

gboolean
e_attachment_get_can_show (EAttachment *attachment)
{
	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), FALSE);

	return attachment->priv->can_show;
}

void
e_attachment_set_can_show (EAttachment *attachment,
                           gboolean can_show)
{
	g_return_if_fail (E_IS_ATTACHMENT (attachment));

	attachment->priv->can_show = can_show;

	g_object_notify (G_OBJECT (attachment), "can-show");
}

const gchar *
e_attachment_get_disposition (EAttachment *attachment)
{
	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), NULL);

	return attachment->priv->disposition;
}

void
e_attachment_set_disposition (EAttachment *attachment,
                              const gchar *disposition)
{
	g_return_if_fail (E_IS_ATTACHMENT (attachment));

	g_free (attachment->priv->disposition);
	attachment->priv->disposition = g_strdup (disposition);

	g_object_notify (G_OBJECT (attachment), "disposition");
}

GFile *
e_attachment_get_file (EAttachment *attachment)
{
	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), NULL);

	return attachment->priv->file;
}

void
e_attachment_set_file (EAttachment *attachment,
                       GFile *file)
{
	g_return_if_fail (E_IS_ATTACHMENT (attachment));

	if (file != NULL) {
		g_return_if_fail (G_IS_FILE (file));
		g_object_ref (file);
	}

	if (attachment->priv->file != NULL)
		g_object_unref (attachment->priv->file);

	attachment->priv->file = file;

	g_object_notify (G_OBJECT (attachment), "file");
}

GFileInfo *
e_attachment_get_file_info (EAttachment *attachment)
{
	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), NULL);

	return attachment->priv->file_info;
}

gboolean
e_attachment_get_loading (EAttachment *attachment)
{
	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), FALSE);

	return attachment->priv->loading;
}

CamelMimePart *
e_attachment_get_mime_part (EAttachment *attachment)
{
	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), NULL);

	return attachment->priv->mime_part;
}

void
e_attachment_set_mime_part (EAttachment *attachment,
                            CamelMimePart *mime_part)
{
	g_return_if_fail (E_IS_ATTACHMENT (attachment));

	if (mime_part != NULL) {
		g_return_if_fail (CAMEL_IS_MIME_PART (mime_part));
		camel_object_ref (mime_part);
	}

	if (attachment->priv->mime_part != NULL)
		camel_object_unref (attachment->priv->mime_part);

	attachment->priv->mime_part = mime_part;

	g_object_notify (G_OBJECT (attachment), "mime-part");
}

gint
e_attachment_get_percent (EAttachment *attachment)
{
	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), 0);

	return attachment->priv->percent;
}

GtkTreeRowReference *
e_attachment_get_reference (EAttachment *attachment)
{
	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), NULL);

	return attachment->priv->reference;
}

void
e_attachment_set_reference (EAttachment *attachment,
                            GtkTreeRowReference *reference)
{
	g_return_if_fail (E_IS_ATTACHMENT (attachment));

	if (reference != NULL)
		reference = gtk_tree_row_reference_copy (reference);

	gtk_tree_row_reference_free (attachment->priv->reference);
	attachment->priv->reference = reference;

	g_object_notify (G_OBJECT (attachment), "reference");
}

gboolean
e_attachment_get_saving (EAttachment *attachment)
{
	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), FALSE);

	return attachment->priv->saving;
}

gboolean
e_attachment_get_shown (EAttachment *attachment)
{
	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), FALSE);

	return attachment->priv->shown;
}

void
e_attachment_set_shown (EAttachment *attachment,
                        gboolean shown)
{
	g_return_if_fail (E_IS_ATTACHMENT (attachment));

	attachment->priv->shown = shown;

	g_object_notify (G_OBJECT (attachment), "shown");
}

camel_cipher_validity_encrypt_t
e_attachment_get_encrypted (EAttachment *attachment)
{
	g_return_val_if_fail (
		E_IS_ATTACHMENT (attachment),
		CAMEL_CIPHER_VALIDITY_ENCRYPT_NONE);

	return attachment->priv->encrypted;
}

void
e_attachment_set_encrypted (EAttachment *attachment,
                            camel_cipher_validity_encrypt_t encrypted)
{
	g_return_if_fail (E_IS_ATTACHMENT (attachment));

	attachment->priv->encrypted = encrypted;

	g_object_notify (G_OBJECT (attachment), "encrypted");
}

camel_cipher_validity_sign_t
e_attachment_get_signed (EAttachment *attachment)
{
	g_return_val_if_fail (
		E_IS_ATTACHMENT (attachment),
		CAMEL_CIPHER_VALIDITY_SIGN_NONE);

	return attachment->priv->signed_;
}

void
e_attachment_set_signed (EAttachment *attachment,
                         camel_cipher_validity_sign_t signed_)
{
	g_return_if_fail (E_IS_ATTACHMENT (attachment));

	attachment->priv->signed_ = signed_;

	g_object_notify (G_OBJECT (attachment), "signed");
}

const gchar *
e_attachment_get_description (EAttachment *attachment)
{
	GFileInfo *file_info;
	const gchar *attribute;

	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), NULL);

	attribute = G_FILE_ATTRIBUTE_STANDARD_DESCRIPTION;
	file_info = e_attachment_get_file_info (attachment);

	if (file_info == NULL)
		return NULL;

	return g_file_info_get_attribute_string (file_info, attribute);
}

const gchar *
e_attachment_get_thumbnail_path (EAttachment *attachment)
{
	GFileInfo *file_info;
	const gchar *attribute;

	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), NULL);

	attribute = G_FILE_ATTRIBUTE_THUMBNAIL_PATH;
	file_info = e_attachment_get_file_info (attachment);

	if (file_info == NULL)
		return NULL;

	return g_file_info_get_attribute_byte_string (file_info, attribute);
}

gboolean
e_attachment_is_rfc822 (EAttachment *attachment)
{
	GFileInfo *file_info;
	const gchar *content_type;
	gchar *mime_type;
	gboolean is_rfc822;

	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), FALSE);

	file_info = e_attachment_get_file_info (attachment);
	if (file_info == NULL)
		return FALSE;

	content_type = g_file_info_get_content_type (file_info);
	if (content_type == NULL)
		return FALSE;

	mime_type = g_content_type_get_mime_type (content_type);
	is_rfc822 = (g_ascii_strcasecmp (mime_type, "message/rfc822") == 0);
	g_free (mime_type);

	return is_rfc822;
}

GList *
e_attachment_list_apps (EAttachment *attachment)
{
	GList *app_info_list;
	GList *guessed_infos;
	GFileInfo *file_info;
	const gchar *content_type;
	const gchar *display_name;
	gboolean type_is_unknown;
	gchar *allocated;

	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), NULL);

	file_info = e_attachment_get_file_info (attachment);
	if (file_info == NULL)
		return NULL;

	content_type = g_file_info_get_content_type (file_info);
	display_name = g_file_info_get_display_name (file_info);
	g_return_val_if_fail (content_type != NULL, NULL);

	app_info_list = g_app_info_get_all_for_type (content_type);
	type_is_unknown = g_content_type_is_unknown (content_type);

	if (app_info_list != NULL && !type_is_unknown)
		goto exit;

	if (display_name == NULL)
		goto exit;

	allocated = g_content_type_guess (display_name, NULL, 0, NULL);
	guessed_infos = g_app_info_get_all_for_type (allocated);
	app_info_list = g_list_concat (guessed_infos, app_info_list);
	g_free (allocated);

exit:
	return app_info_list;
}

/************************* e_attachment_load_async() *************************/

typedef struct _LoadContext LoadContext;

struct _LoadContext {
	EAttachment *attachment;
	GSimpleAsyncResult *simple;

	GInputStream *input_stream;
	GOutputStream *output_stream;
	GFileInfo *file_info;
	goffset total_num_bytes;
	gssize bytes_read;
	gchar buffer[4096];
};

/* Forward Declaration */
static void
attachment_load_stream_read_cb (GInputStream *input_stream,
                                GAsyncResult *result,
                                LoadContext *load_context);

static LoadContext *
attachment_load_context_new (EAttachment *attachment,
                             GAsyncReadyCallback callback,
                             gpointer user_data)
{
	LoadContext *load_context;
	GSimpleAsyncResult *simple;

	simple = g_simple_async_result_new (
		G_OBJECT (attachment), callback,
		user_data, e_attachment_load_async);

	load_context = g_slice_new0 (LoadContext);
	load_context->attachment = g_object_ref (attachment);
	load_context->simple = simple;

	attachment_set_loading (load_context->attachment, TRUE);

	return load_context;
}

static void
attachment_load_context_free (LoadContext *load_context)
{
	/* Do not free the GSimpleAsyncResult. */
	g_object_unref (load_context->attachment);

	if (load_context->input_stream != NULL)
		g_object_unref (load_context->input_stream);

	if (load_context->output_stream != NULL)
		g_object_unref (load_context->output_stream);

	if (load_context->file_info != NULL)
		g_object_unref (load_context->file_info);

	g_slice_free (LoadContext, load_context);
}

static gboolean
attachment_load_check_for_error (LoadContext *load_context,
                                 GError *error)
{
	GSimpleAsyncResult *simple;

	if (error == NULL)
		return FALSE;

	/* Steal the result. */
	simple = load_context->simple;
	load_context->simple = NULL;

	g_simple_async_result_set_from_error (simple, error);
	g_simple_async_result_complete (simple);
	g_error_free (error);

	attachment_load_context_free (load_context);

	return TRUE;
}

static void
attachment_load_finish (LoadContext *load_context)
{
	GFileInfo *file_info;
	EAttachment *attachment;
	GMemoryOutputStream *output_stream;
	GSimpleAsyncResult *simple;
	CamelDataWrapper *wrapper;
	CamelMimePart *mime_part;
	CamelStream *stream;
	const gchar *attribute;
	const gchar *content_type;
	const gchar *display_name;
	const gchar *description;
	const gchar *disposition;
	gchar *mime_type;
	gpointer data;
	gsize size;

	/* Steal the result. */
	simple = load_context->simple;
	load_context->simple = NULL;

	file_info = load_context->file_info;
	attachment = load_context->attachment;
	output_stream = G_MEMORY_OUTPUT_STREAM (load_context->output_stream);

	if (e_attachment_is_rfc822 (attachment))
		wrapper = (CamelDataWrapper *) camel_mime_message_new ();
	else
		wrapper = camel_data_wrapper_new ();

	content_type = g_file_info_get_content_type (file_info);
	mime_type = g_content_type_get_mime_type (content_type);

	data = g_memory_output_stream_get_data (output_stream);
	size = g_memory_output_stream_get_data_size (output_stream);

	stream = camel_stream_mem_new_with_buffer (data, size);
	camel_data_wrapper_construct_from_stream (wrapper, stream);
	camel_data_wrapper_set_mime_type (wrapper, mime_type);
	camel_stream_close (stream);
	camel_object_unref (stream);

	mime_part = camel_mime_part_new ();
	camel_medium_set_content_object (CAMEL_MEDIUM (mime_part), wrapper);

	camel_object_unref (wrapper);
	g_free (mime_type);

	display_name = g_file_info_get_display_name (file_info);
	if (display_name != NULL)
		camel_mime_part_set_filename (mime_part, display_name);

	attribute = G_FILE_ATTRIBUTE_STANDARD_DESCRIPTION;
	description = g_file_info_get_attribute_string (file_info, attribute);
	if (description != NULL)
		camel_mime_part_set_description (mime_part, description);

	disposition = e_attachment_get_disposition (attachment);
	if (disposition != NULL)
		camel_mime_part_set_disposition (mime_part, disposition);

	/* Correctly report the size of zero length special files. */
	if (g_file_info_get_size (file_info) == 0) {
		g_file_info_set_size (file_info, size);
		attachment_set_file_info (attachment, file_info);
	}

	g_simple_async_result_set_op_res_gpointer (
		simple, mime_part, (GDestroyNotify) camel_object_unref);

	g_simple_async_result_complete (simple);

	attachment_load_context_free (load_context);
}

static void
attachment_load_write_cb (GOutputStream *output_stream,
                          GAsyncResult *result,
                          LoadContext *load_context)
{
	EAttachment *attachment;
	GCancellable *cancellable;
	GInputStream *input_stream;
	gssize bytes_written;
	GError *error = NULL;

	bytes_written = g_output_stream_write_finish (
		output_stream, result, &error);

	if (attachment_load_check_for_error (load_context, error))
		return;

	attachment = load_context->attachment;
	cancellable = attachment->priv->cancellable;
	input_stream = load_context->input_stream;

	attachment_progress_cb (
		g_seekable_tell (G_SEEKABLE (output_stream)),
		load_context->total_num_bytes, attachment);

	if (bytes_written < load_context->bytes_read) {
		g_memmove (
			load_context->buffer,
			load_context->buffer + bytes_written,
			load_context->bytes_read - bytes_written);
		load_context->bytes_read -= bytes_written;

		g_output_stream_write_async (
			output_stream,
			load_context->buffer,
			load_context->bytes_read,
			G_PRIORITY_DEFAULT, cancellable,
			(GAsyncReadyCallback) attachment_load_write_cb,
			load_context);
	} else
		g_input_stream_read_async (
			input_stream,
			load_context->buffer,
			sizeof (load_context->buffer),
			G_PRIORITY_DEFAULT, cancellable,
			(GAsyncReadyCallback) attachment_load_stream_read_cb,
			load_context);
}

static void
attachment_load_stream_read_cb (GInputStream *input_stream,
                                GAsyncResult *result,
                                LoadContext *load_context)
{
	EAttachment *attachment;
	GCancellable *cancellable;
	GOutputStream *output_stream;
	gssize bytes_read;
	GError *error = NULL;

	bytes_read = g_input_stream_read_finish (
		input_stream, result, &error);

	if (attachment_load_check_for_error (load_context, error))
		return;

	if (bytes_read == 0) {
		attachment_load_finish (load_context);
		return;
	}

	attachment = load_context->attachment;
	cancellable = attachment->priv->cancellable;
	output_stream = load_context->output_stream;
	load_context->bytes_read = bytes_read;

	g_output_stream_write_async (
		output_stream,
		load_context->buffer,
		load_context->bytes_read,
		G_PRIORITY_DEFAULT, cancellable,
		(GAsyncReadyCallback) attachment_load_write_cb,
		load_context);
}

static void
attachment_load_file_read_cb (GFile *file,
                              GAsyncResult *result,
                              LoadContext *load_context)
{
	EAttachment *attachment;
	GCancellable *cancellable;
	GFileInputStream *input_stream;
	GOutputStream *output_stream;
	GError *error = NULL;

	/* Input stream might be NULL, so don't use cast macro. */
	input_stream = g_file_read_finish (file, result, &error);
	load_context->input_stream = (GInputStream *) input_stream;

	if (attachment_load_check_for_error (load_context, error))
		return;

	/* Load the contents into a GMemoryOutputStream. */
	output_stream = g_memory_output_stream_new (
		NULL, 0, g_realloc, g_free);

	attachment = load_context->attachment;
	cancellable = attachment->priv->cancellable;
	load_context->output_stream = output_stream;

	g_input_stream_read_async (
		load_context->input_stream,
		load_context->buffer,
		sizeof (load_context->buffer),
		G_PRIORITY_DEFAULT, cancellable,
		(GAsyncReadyCallback) attachment_load_stream_read_cb,
		load_context);
}

static void
attachment_load_query_info_cb (GFile *file,
                               GAsyncResult *result,
                               LoadContext *load_context)
{
	EAttachment *attachment;
	GCancellable *cancellable;
	GFileInfo *file_info;
	GError *error = NULL;

	attachment = load_context->attachment;
	cancellable = attachment->priv->cancellable;

	file_info = g_file_query_info_finish (file, result, &error);
	if (attachment_load_check_for_error (load_context, error))
		return;

	attachment_set_file_info (attachment, file_info);
	load_context->file_info = file_info;

	load_context->total_num_bytes = g_file_info_get_size (file_info);

	g_file_read_async (
		file, G_PRIORITY_DEFAULT,
		cancellable, (GAsyncReadyCallback)
		attachment_load_file_read_cb, load_context);
}

static void
attachment_load_from_mime_part (LoadContext *load_context)
{
	GFileInfo *file_info;
	EAttachment *attachment;
	GSimpleAsyncResult *simple;
	CamelContentType *content_type;
	CamelMimePart *mime_part;
	const gchar *attribute;
	const gchar *string;
	gchar *allocated;
	goffset size;

	attachment = load_context->attachment;
	mime_part = e_attachment_get_mime_part (attachment);

	file_info = g_file_info_new ();
	load_context->file_info = file_info;

	content_type = camel_mime_part_get_content_type (mime_part);
	allocated = camel_content_type_simple (content_type);
	if (allocated != NULL) {
		GIcon *icon;
		gchar *cp;

		/* GIO expects lowercase MIME types. */
		for (cp = allocated; *cp != '\0'; cp++)
			*cp = g_ascii_tolower (*cp);

		/* Swap the MIME type for a content type. */
		cp = g_content_type_from_mime_type (allocated);
		g_free (allocated);
		allocated = cp;

		/* Use the MIME part's filename if we have to. */
		if (g_content_type_is_unknown (allocated)) {
			string = camel_mime_part_get_filename (mime_part);
			if (string != NULL) {
				g_free (allocated);
				allocated = g_content_type_guess (
					string, NULL, 0, NULL);
			}
		}

		g_file_info_set_content_type (file_info, allocated);

		icon = g_content_type_get_icon (allocated);
		if (icon != NULL) {
			g_file_info_set_icon (file_info, icon);
			g_object_unref (icon);
		}
	}
	g_free (allocated);

	string = camel_mime_part_get_filename (mime_part);
	if (string == NULL)
		/* Translators: Default attachment filename. */
		string = _("attachment.dat");
	g_file_info_set_display_name (file_info, string);

	attribute = G_FILE_ATTRIBUTE_STANDARD_DESCRIPTION;
	string = camel_mime_part_get_description (mime_part);
	if (string != NULL)
		g_file_info_set_attribute_string (
			file_info, attribute, string);

	/* FIXME This can cause Camel to block while downloading the
	 *       MIME part in order to determine the content size. */
	size = (goffset) camel_mime_part_get_content_size (mime_part);
	g_file_info_set_size (file_info, size);

	string = camel_mime_part_get_disposition (mime_part);
	e_attachment_set_disposition (attachment, string);

	attachment_set_file_info (attachment, file_info);

	/* Steal the result. */
	simple = load_context->simple;
	load_context->simple = NULL;

	camel_object_ref (mime_part);
	g_simple_async_result_set_op_res_gpointer (
		simple, mime_part,
		(GDestroyNotify) camel_object_unref);
	g_simple_async_result_complete_in_idle (simple);

	attachment_load_context_free (load_context);
}

void
e_attachment_load_async (EAttachment *attachment,
                         GAsyncReadyCallback callback,
                         gpointer user_data)
{
	LoadContext *load_context;
	GCancellable *cancellable;
	CamelMimePart *mime_part;
	GFile *file;

	g_return_if_fail (E_IS_ATTACHMENT (attachment));
	g_return_if_fail (callback != NULL);

	if (e_attachment_get_loading (attachment)) {
		g_simple_async_report_error_in_idle (
			G_OBJECT (attachment), callback, user_data,
			G_IO_ERROR, G_IO_ERROR_BUSY,
			_("A load operation is already in progress"));
		return;
	}

	if (e_attachment_get_saving (attachment)) {
		g_simple_async_report_error_in_idle (
			G_OBJECT (attachment), callback, user_data,
			G_IO_ERROR, G_IO_ERROR_BUSY,
			_("A save operation is already in progress"));
		return;
	}

	file = e_attachment_get_file (attachment);
	mime_part = e_attachment_get_mime_part (attachment);
	g_return_if_fail (file != NULL || mime_part != NULL);

	load_context = attachment_load_context_new (
		attachment, callback, user_data);

	cancellable = attachment->priv->cancellable;
	g_cancellable_reset (cancellable);

	if (file != NULL)
		g_file_query_info_async (
			file, ATTACHMENT_QUERY,
			G_FILE_QUERY_INFO_NONE,G_PRIORITY_DEFAULT,
			cancellable, (GAsyncReadyCallback)
			attachment_load_query_info_cb, load_context);

	else if (mime_part != NULL)
		attachment_load_from_mime_part (load_context);

}

gboolean
e_attachment_load_finish (EAttachment *attachment,
                          GAsyncResult *result,
                          GError **error)
{
	GSimpleAsyncResult *simple;
	CamelMimePart *mime_part;

	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), FALSE);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (result), FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (result);
	mime_part = g_simple_async_result_get_op_res_gpointer (simple);
	if (mime_part != NULL)
		e_attachment_set_mime_part (attachment, mime_part);
	g_simple_async_result_propagate_error (simple, error);
	g_object_unref (simple);

	attachment_set_loading (attachment, FALSE);

	return (mime_part != NULL);
}

void
e_attachment_load_handle_error (EAttachment *attachment,
                                GAsyncResult *result,
                                GtkWindow *parent)
{
	GtkWidget *dialog;
	GFileInfo *file_info;
	GtkTreeRowReference *reference;
	const gchar *display_name;
	const gchar *primary_text;
	GError *error = NULL;

	g_return_if_fail (E_IS_ATTACHMENT (attachment));
	g_return_if_fail (G_IS_ASYNC_RESULT (result));
	g_return_if_fail (GTK_IS_WINDOW (parent));

	if (e_attachment_load_finish (attachment, result, &error))
		return;

	/* XXX Calling EAttachmentStore functions from here violates
	 *     the abstraction, but for now it's not hurting anything. */
	reference = e_attachment_get_reference (attachment);
	if (gtk_tree_row_reference_valid (reference)) {
		GtkTreeModel *model;

		model = gtk_tree_row_reference_get_model (reference);

		e_attachment_store_remove_attachment (
			E_ATTACHMENT_STORE (model), attachment);
	}

	/* Ignore cancellations. */
	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
		return;

	file_info = e_attachment_get_file_info (attachment);

	if (file_info != NULL)
		display_name = g_file_info_get_display_name (file_info);
	else
		display_name = NULL;

	if (display_name != NULL)
		primary_text = g_strdup_printf (
			_("Could not load '%s'"), display_name);
	else
		primary_text = g_strdup_printf (
			_("Could not load the attachment"));

	dialog = gtk_message_dialog_new_with_markup (
		parent, GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
		"<big><b>%s</b></big>", primary_text);

	gtk_message_dialog_format_secondary_text (
		GTK_MESSAGE_DIALOG (dialog), "%s", error->message);

	gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (dialog);
	g_error_free (error);
}

/************************* e_attachment_open_async() *************************/

typedef struct _OpenContext OpenContext;

struct _OpenContext {
	EAttachment *attachment;
	GSimpleAsyncResult *simple;

	GAppInfo *app_info;
};

static OpenContext *
attachment_open_context_new (EAttachment *attachment,
                             GAsyncReadyCallback callback,
                             gpointer user_data)
{
	OpenContext *open_context;
	GSimpleAsyncResult *simple;

	simple = g_simple_async_result_new (
		G_OBJECT (attachment), callback,
		user_data, e_attachment_open_async);

	open_context = g_slice_new0 (OpenContext);
	open_context->attachment = g_object_ref (attachment);
	open_context->simple = simple;

	return open_context;
}

static void
attachment_open_context_free (OpenContext *open_context)
{
	/* Do not free the GSimpleAsyncResult. */
	g_object_unref (open_context->attachment);

	if (open_context->app_info != NULL)
		g_object_unref (open_context->app_info);

	g_slice_free (OpenContext, open_context);
}

static gboolean
attachment_open_check_for_error (OpenContext *open_context,
                                 GError *error)
{
	GSimpleAsyncResult *simple;

	if (error == NULL)
		return FALSE;

	/* Steal the result. */
	simple = open_context->simple;
	open_context->simple = NULL;

	g_simple_async_result_set_from_error (simple, error);
	g_simple_async_result_complete (simple);
	g_error_free (error);

	attachment_open_context_free (open_context);

	return TRUE;
}

static void
attachment_open_file (GFile *file,
                      OpenContext *open_context)
{
	GdkAppLaunchContext *context;
	GSimpleAsyncResult *simple;
	gboolean success;
	GError *error = NULL;

	/* Steal the result. */
	simple = open_context->simple;
	open_context->simple = NULL;

	context = gdk_app_launch_context_new ();

	if (open_context->app_info != NULL) {
		GList *file_list;

		file_list = g_list_prepend (NULL, file);
		success = g_app_info_launch (
			open_context->app_info, file_list,
			G_APP_LAUNCH_CONTEXT (context), &error);
		g_list_free (file_list);
	} else {
		gchar *uri;

		uri = g_file_get_uri (file);
		success = g_app_info_launch_default_for_uri (
			uri, G_APP_LAUNCH_CONTEXT (context), &error);
		g_free (uri);
	}

	g_object_unref (context);

	g_simple_async_result_set_op_res_gboolean (simple, success);

	if (error != NULL) {
		g_simple_async_result_set_from_error (simple, error);
		g_error_free (error);
	}

	g_simple_async_result_complete (simple);
	attachment_open_context_free (open_context);
}

static void
attachment_open_save_finished_cb (EAttachment *attachment,
                                  GAsyncResult *result,
                                  OpenContext *open_context)
{
	GFile *file;
	gchar *path;
	GError *error = NULL;

	file = e_attachment_save_finish (attachment, result, &error);

	if (attachment_open_check_for_error (open_context, error))
		return;

	/* Make the temporary file read-only.
	 *
	 * This step is non-critical, so if an error occurs just
	 * emit a warning and move on.
	 *
	 * XXX I haven't figured out how to do this through GIO.
	 *     Attempting to set the "access::can-write" attribute via
	 *     g_file_set_attribute() returned G_IO_ERROR_NOT_SUPPORTED
	 *     and the only other possibility I see is "unix::mode",
	 *     which is obviously not portable.
	 */
	path = g_file_get_path (file);
	if (g_chmod (path, S_IRUSR | S_IRGRP | S_IROTH) < 0)
		g_warning ("%s", g_strerror (errno));
	g_free (path);

	attachment_open_file (file, open_context);
	g_object_unref (file);
}

static void
attachment_open_save_temporary (OpenContext *open_context)
{
	GFile *temp_directory;
	gchar *template;
	gchar *path;
	GError *error = NULL;

	errno = 0;

	/* Save the file to a temporary directory.
	 * We use a directory so the files can retain their basenames.
	 * XXX This could trigger a blocking temp directory cleanup. */
	template = g_strdup_printf (PACKAGE "-%s-XXXXXX", g_get_user_name ());
	path = e_mkdtemp (template);
	g_free (template);

	/* XXX Let's hope errno got set properly. */
	if (path == NULL)
		g_set_error (
			&error, G_FILE_ERROR,
			g_file_error_from_errno (errno),
			"%s", g_strerror (errno));

	/* We already know if there's an error, but this does the cleanup. */
	if (attachment_open_check_for_error (open_context, error))
		return;

	temp_directory = g_file_new_for_path (path);

	e_attachment_save_async (
		open_context->attachment,
		temp_directory, (GAsyncReadyCallback)
		attachment_open_save_finished_cb, open_context);

	g_object_unref (temp_directory);
	g_free (path);
}

void
e_attachment_open_async (EAttachment *attachment,
                         GAppInfo *app_info,
                         GAsyncReadyCallback callback,
                         gpointer user_data)
{
	OpenContext *open_context;
	CamelMimePart *mime_part;
	GFile *file;

	g_return_if_fail (E_IS_ATTACHMENT (attachment));
	g_return_if_fail (callback != NULL);

	file = e_attachment_get_file (attachment);
	mime_part = e_attachment_get_mime_part (attachment);
	g_return_if_fail (file != NULL || mime_part != NULL);

	open_context = attachment_open_context_new (
		attachment, callback, user_data);

	if (G_IS_APP_INFO (app_info))
		open_context->app_info = g_object_ref (app_info);

	/* If the attachment already references a GFile, we can launch
	 * the application directly.  Otherwise we have to save the MIME
	 * part to a temporary file and launch the application from that. */
	if (file != NULL) {
		attachment_open_file (file, open_context);

	} else if (mime_part != NULL)
		attachment_open_save_temporary (open_context);
}

gboolean
e_attachment_open_finish (EAttachment *attachment,
                          GAsyncResult *result,
                          GError **error)
{
	GSimpleAsyncResult *simple;
	gboolean success;

	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), FALSE);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (result), FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (result);
	success = g_simple_async_result_get_op_res_gboolean (simple);
	g_simple_async_result_propagate_error (simple, error);
	g_object_unref (simple);

	return success;
}

void
e_attachment_open_handle_error (EAttachment *attachment,
                                GAsyncResult *result,
                                GtkWindow *parent)
{
	GtkWidget *dialog;
	GFileInfo *file_info;
	const gchar *display_name;
	const gchar *primary_text;
	GError *error = NULL;

	g_return_if_fail (E_IS_ATTACHMENT (attachment));
	g_return_if_fail (G_IS_ASYNC_RESULT (result));
	g_return_if_fail (GTK_IS_WINDOW (parent));

	if (e_attachment_open_finish (attachment, result, &error))
		return;

	/* Ignore cancellations. */
	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
		return;

	file_info = e_attachment_get_file_info (attachment);

	if (file_info != NULL)
		display_name = g_file_info_get_display_name (file_info);
	else
		display_name = NULL;

	if (display_name != NULL)
		primary_text = g_strdup_printf (
			_("Could not open '%s'"), display_name);
	else
		primary_text = g_strdup_printf (
			_("Could not open the attachment"));

	dialog = gtk_message_dialog_new_with_markup (
		parent, GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
		"<big><b>%s</b></big>", primary_text);

	gtk_message_dialog_format_secondary_text (
		GTK_MESSAGE_DIALOG (dialog), "%s", error->message);

	gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (dialog);
	g_error_free (error);
}

/************************* e_attachment_save_async() *************************/

typedef struct _SaveContext SaveContext;

struct _SaveContext {
	EAttachment *attachment;
	GSimpleAsyncResult *simple;

	GFile *directory;
	GFile *destination;
	GInputStream *input_stream;
	GOutputStream *output_stream;
	goffset total_num_bytes;
	gssize bytes_read;
	gchar buffer[4096];
	gint count;
};

/* Forward Declaration */
static void
attachment_save_read_cb (GInputStream *input_stream,
                         GAsyncResult *result,
                         SaveContext *save_context);

static SaveContext *
attachment_save_context_new (EAttachment *attachment,
                             GAsyncReadyCallback callback,
                             gpointer user_data)
{
	SaveContext *save_context;
	GSimpleAsyncResult *simple;

	simple = g_simple_async_result_new (
		G_OBJECT (attachment), callback,
		user_data, e_attachment_save_async);

	save_context = g_slice_new0 (SaveContext);
	save_context->attachment = g_object_ref (attachment);
	save_context->simple = simple;

	attachment_set_saving (save_context->attachment, TRUE);

	return save_context;
}

static void
attachment_save_context_free (SaveContext *save_context)
{
	/* Do not free the GSimpleAsyncResult. */
	g_object_unref (save_context->attachment);

	if (save_context->directory != NULL)
		g_object_unref (save_context->directory);

	if (save_context->destination != NULL)
		g_object_unref (save_context->destination);

	if (save_context->input_stream != NULL)
		g_object_unref (save_context->input_stream);

	if (save_context->output_stream != NULL)
		g_object_unref (save_context->output_stream);

	g_slice_free (SaveContext, save_context);
}

static gboolean
attachment_save_check_for_error (SaveContext *save_context,
                                 GError *error)
{
	GSimpleAsyncResult *simple;

	if (error == NULL)
		return FALSE;

	/* Steal the result. */
	simple = save_context->simple;
	save_context->simple = NULL;

	g_simple_async_result_set_from_error (simple, error);
	g_simple_async_result_complete (simple);
	g_error_free (error);

	attachment_save_context_free (save_context);

	return TRUE;
}

static GFile *
attachment_save_new_candidate (SaveContext *save_context)
{
	GFile *candidate;
	GFileInfo *file_info;
	EAttachment *attachment;
	const gchar *display_name = NULL;
	gchar *basename;

	attachment = save_context->attachment;
	file_info = e_attachment_get_file_info (attachment);

	if (file_info != NULL)
		display_name = g_file_info_get_display_name (file_info);
	if (display_name == NULL)
		/* Translators: Default attachment filename. */
		display_name = _("attachment.dat");

	if (save_context->count == 0)
		basename = g_strdup (display_name);
	else {
		GString *string;
		const gchar *ext;
		gsize length;

		string = g_string_sized_new (strlen (display_name));
		ext = g_utf8_strchr (display_name, -1, '.');

		if (ext != NULL)
			length = ext - display_name;
		else
			length = strlen (display_name);

		g_string_append_len (string, display_name, length);
		g_string_append_printf (string, " (%d)", save_context->count);
		g_string_append (string, (ext != NULL) ? ext : "");

		basename = g_string_free (string, FALSE);
	}

	save_context->count++;

	candidate = g_file_get_child (save_context->directory, basename);

	g_free (basename);

	return candidate;
}

static void
attachment_save_write_cb (GOutputStream *output_stream,
                          GAsyncResult *result,
                          SaveContext *save_context)
{
	EAttachment *attachment;
	GCancellable *cancellable;
	GInputStream *input_stream;
	gssize bytes_written;
	GError *error = NULL;

	bytes_written = g_output_stream_write_finish (
		output_stream, result, &error);

	if (attachment_save_check_for_error (save_context, error))
		return;

	attachment = save_context->attachment;
	cancellable = attachment->priv->cancellable;
	input_stream = save_context->input_stream;

	if (bytes_written < save_context->bytes_read) {
		g_memmove (
			save_context->buffer,
			save_context->buffer + bytes_written,
			save_context->bytes_read - bytes_written);
		save_context->bytes_read -= bytes_written;

		g_output_stream_write_async (
			output_stream,
			save_context->buffer,
			save_context->bytes_read,
			G_PRIORITY_DEFAULT, cancellable,
			(GAsyncReadyCallback) attachment_save_write_cb,
			save_context);
	} else
		g_input_stream_read_async (
			input_stream,
			save_context->buffer,
			sizeof (save_context->buffer),
			G_PRIORITY_DEFAULT, cancellable,
			(GAsyncReadyCallback) attachment_save_read_cb,
			save_context);
}

static void
attachment_save_read_cb (GInputStream *input_stream,
                         GAsyncResult *result,
                         SaveContext *save_context)
{
	EAttachment *attachment;
	GCancellable *cancellable;
	GOutputStream *output_stream;
	gssize bytes_read;
	GError *error = NULL;

	bytes_read = g_input_stream_read_finish (
		input_stream, result, &error);

	if (attachment_save_check_for_error (save_context, error))
		return;

	if (bytes_read == 0) {
		GSimpleAsyncResult *simple;
		GFile *destination;

		/* Steal the result. */
		simple = save_context->simple;
		save_context->simple = NULL;

		/* Steal the destination. */
		destination = save_context->destination;
		save_context->destination = NULL;

		g_simple_async_result_set_op_res_gpointer (
			simple, destination, (GDestroyNotify) g_object_unref);
		g_simple_async_result_complete (simple);

		attachment_save_context_free (save_context);

		return;
	}

	attachment = save_context->attachment;
	cancellable = attachment->priv->cancellable;
	output_stream = save_context->output_stream;
	save_context->bytes_read = bytes_read;

	attachment_progress_cb (
		g_seekable_tell (G_SEEKABLE (input_stream)),
		save_context->total_num_bytes, attachment);

	g_output_stream_write_async (
		output_stream,
		save_context->buffer,
		save_context->bytes_read,
		G_PRIORITY_DEFAULT, cancellable,
		(GAsyncReadyCallback) attachment_save_write_cb,
		save_context);
}

static void
attachment_save_got_output_stream (SaveContext *save_context)
{
	GCancellable *cancellable;
	GInputStream *input_stream;
	CamelDataWrapper *wrapper;
	CamelMimePart *mime_part;
	CamelStream *stream;
	EAttachment *attachment;
	GByteArray *buffer;

	attachment = save_context->attachment;
	cancellable = attachment->priv->cancellable;
	mime_part = e_attachment_get_mime_part (attachment);

	/* Decode the MIME part to an in-memory buffer.  We have to do
	 * this because CamelStream is synchronous-only, and using threads
	 * is dangerous because CamelDataWrapper is not reentrant. */
	buffer = g_byte_array_new ();
	stream = camel_stream_mem_new ();
	camel_stream_mem_set_byte_array (CAMEL_STREAM_MEM (stream), buffer);
	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));
	camel_data_wrapper_decode_to_stream (wrapper, stream);
	camel_object_unref (stream);

	/* Load the buffer into a GMemoryInputStream.
	 * But watch out for zero length MIME parts. */
	input_stream = g_memory_input_stream_new ();
	if (buffer->len > 0)
		g_memory_input_stream_add_data (
			G_MEMORY_INPUT_STREAM (input_stream),
			buffer->data, (gssize) buffer->len,
			(GDestroyNotify) g_free);
	save_context->input_stream = input_stream;
	save_context->total_num_bytes = (goffset) buffer->len;
	g_byte_array_free (buffer, FALSE);

	g_input_stream_read_async (
		input_stream,
		save_context->buffer,
		sizeof (save_context->buffer),
		G_PRIORITY_DEFAULT, cancellable,
		(GAsyncReadyCallback) attachment_save_read_cb,
		save_context);
}

static void
attachment_save_create_cb (GFile *destination,
                           GAsyncResult *result,
                           SaveContext *save_context)
{
	EAttachment *attachment;
	GCancellable *cancellable;
	GFileOutputStream *output_stream;
	GError *error = NULL;

	/* Output stream might be NULL, so don't use cast macro. */
	output_stream = g_file_create_finish (destination, result, &error);
	save_context->output_stream = (GOutputStream *) output_stream;

	attachment = save_context->attachment;
	cancellable = attachment->priv->cancellable;

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS)) {
		destination = attachment_save_new_candidate (save_context);

		g_file_create_async (
			destination, G_FILE_CREATE_NONE,
			G_PRIORITY_DEFAULT, cancellable,
			(GAsyncReadyCallback) attachment_save_create_cb,
			save_context);

		g_object_unref (destination);
		g_error_free (error);
		return;
	}

	if (attachment_save_check_for_error (save_context, error))
		return;

	save_context->destination = g_object_ref (destination);
	attachment_save_got_output_stream (save_context);
}

static void
attachment_save_replace_cb (GFile *destination,
                            GAsyncResult *result,
                            SaveContext *save_context)
{
	GFileOutputStream *output_stream;
	GError *error = NULL;

	/* Output stream might be NULL, so don't use cast macro. */
	output_stream = g_file_replace_finish (destination, result, &error);
	save_context->output_stream = (GOutputStream *) output_stream;

	if (attachment_save_check_for_error (save_context, error))
		return;

	save_context->destination = g_object_ref (destination);
	attachment_save_got_output_stream (save_context);
}

static void
attachment_save_query_info_cb (GFile *destination,
                               GAsyncResult *result,
                               SaveContext *save_context)
{
	EAttachment *attachment;
	GCancellable *cancellable;
	GFileInfo *file_info;
	GFileType file_type;
	GError *error = NULL;

	attachment = save_context->attachment;
	cancellable = attachment->priv->cancellable;

	file_info = g_file_query_info_finish (destination, result, &error);

	/* G_IO_ERROR_NOT_FOUND just means we're creating a new file. */
	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND)) {
		g_error_free (error);
		goto replace;
	}

	if (attachment_save_check_for_error (save_context, error))
		return;

	file_type = g_file_info_get_file_type (file_info);
	g_object_unref (file_info);

	if (file_type == G_FILE_TYPE_DIRECTORY) {
		save_context->directory = g_object_ref (destination);
		destination = attachment_save_new_candidate (save_context);

		g_file_create_async (
			destination, G_FILE_CREATE_NONE,
			G_PRIORITY_DEFAULT, cancellable,
			(GAsyncReadyCallback) attachment_save_create_cb,
			save_context);

		g_object_unref (destination);

		return;
	}

replace:
	g_file_replace_async (
		destination, NULL, FALSE,
		G_FILE_CREATE_REPLACE_DESTINATION,
		G_PRIORITY_DEFAULT, cancellable,
		(GAsyncReadyCallback) attachment_save_replace_cb,
		save_context);
}

void
e_attachment_save_async (EAttachment *attachment,
                         GFile *destination,
                         GAsyncReadyCallback callback,
                         gpointer user_data)
{
	SaveContext *save_context;
	GCancellable *cancellable;

	g_return_if_fail (E_IS_ATTACHMENT (attachment));
	g_return_if_fail (G_IS_FILE (destination));
	g_return_if_fail (callback != NULL);

	if (e_attachment_get_loading (attachment)) {
		g_simple_async_report_error_in_idle (
			G_OBJECT (attachment), callback, user_data,
			G_IO_ERROR, G_IO_ERROR_BUSY,
			_("A load operation is already in progress"));
		return;
	}

	if (e_attachment_get_saving (attachment)) {
		g_simple_async_report_error_in_idle (
			G_OBJECT (attachment), callback, user_data,
			G_IO_ERROR, G_IO_ERROR_BUSY,
			_("A save operation is already in progress"));
		return;
	}

	if (e_attachment_get_mime_part (attachment) == NULL) {
		g_simple_async_report_error_in_idle (
			G_OBJECT (attachment), callback, user_data,
			G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
			_("Attachment contents not loaded"));
		return;
	}

	save_context = attachment_save_context_new (
		attachment, callback, user_data);

	cancellable = attachment->priv->cancellable;
	g_cancellable_reset (cancellable);

	/* First we need to know if destination is a directory. */
	g_file_query_info_async (
		destination, G_FILE_ATTRIBUTE_STANDARD_TYPE,
		G_FILE_QUERY_INFO_NONE, G_PRIORITY_DEFAULT,
		cancellable, (GAsyncReadyCallback)
		attachment_save_query_info_cb, save_context);
}

GFile *
e_attachment_save_finish (EAttachment *attachment,
                          GAsyncResult *result,
                          GError **error)
{
	GSimpleAsyncResult *simple;
	GFile *destination;

	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), NULL);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (result), NULL);

	simple = G_SIMPLE_ASYNC_RESULT (result);
	destination = g_simple_async_result_get_op_res_gpointer (simple);
	if (destination != NULL)
		g_object_ref (destination);
	g_simple_async_result_propagate_error (simple, error);
	g_object_unref (simple);

	attachment_set_saving (attachment, FALSE);

	return destination;
}

void
e_attachment_save_handle_error (EAttachment *attachment,
                                GAsyncResult *result,
                                GtkWindow *parent)
{
	GFile *file;
	GFileInfo *file_info;
	GtkWidget *dialog;
	const gchar *display_name;
	const gchar *primary_text;
	GError *error = NULL;

	g_return_if_fail (E_IS_ATTACHMENT (attachment));
	g_return_if_fail (G_IS_ASYNC_RESULT (result));
	g_return_if_fail (GTK_IS_WINDOW (parent));

	file = e_attachment_save_finish (attachment, result, &error);

	if (file != NULL) {
		g_object_unref (file);
		return;
	}

	/* Ignore cancellations. */
	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
		return;

	file_info = e_attachment_get_file_info (attachment);

	if (file_info != NULL)
		display_name = g_file_info_get_display_name (file_info);
	else
		display_name = NULL;

	if (display_name != NULL)
		primary_text = g_strdup_printf (
			_("Could not save '%s'"), display_name);
	else
		primary_text = g_strdup_printf (
			_("Could not save the attachment"));

	dialog = gtk_message_dialog_new_with_markup (
		parent, GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
		"<big><b>%s</b></big>", primary_text);

	gtk_message_dialog_format_secondary_text (
		GTK_MESSAGE_DIALOG (dialog), "%s", error->message);

	gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (dialog);
	g_error_free (error);
}
