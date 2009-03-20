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
#include <glib/gi18n.h>
#include <camel/camel-iconv.h>
#include <camel/camel-data-wrapper.h>
#include <camel/camel-mime-message.h>
#include <camel/camel-stream-filter.h>
#include <camel/camel-stream-null.h>
#include <camel/camel-stream-vfs.h>

#include "e-util/e-util.h"

#define E_ATTACHMENT_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_ATTACHMENT, EAttachmentPrivate))

/* Attributes needed by EAttachmentStore, et al. */
#define ATTACHMENT_QUERY "standard::*,preview::*,thumbnail::*"

struct _EAttachmentPrivate {
	GFile *file;
	GFileInfo *file_info;
	GCancellable *cancellable;
	CamelMimePart *mime_part;
	gchar *disposition;

	/* This is a reference to our row in an EAttachmentStore,
	 * serving as a means of broadcasting "row-changed" signals.
	 * If we are removed from the store, we lazily free the
	 * reference when it is found to be to be invalid. */
	GtkTreeRowReference *reference;
};

enum {
	PROP_0,
	PROP_DISPOSITION,
	PROP_FILE,
	PROP_FILE_INFO,
	PROP_MIME_PART
};

static gpointer parent_class;

static void
attachment_notify_model (EAttachment *attachment)
{
	GtkTreeRowReference *reference;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;

	reference = attachment->priv->reference;

	if (reference == NULL)
		return;

	/* Free the reference if it's no longer valid.
	 * It means we've been removed from the store. */
	if (!gtk_tree_row_reference_valid (reference)) {
		gtk_tree_row_reference_free (reference);
		attachment->priv->reference = NULL;
		return;
	}

	model = gtk_tree_row_reference_get_model (reference);
	path = gtk_tree_row_reference_get_path (reference);

	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_row_changed (model, path, &iter);
	g_object_notify (G_OBJECT (model), "total-size");

	gtk_tree_path_free (path);
}

static gchar *
attachment_get_default_charset (void)
{
	GConfClient *client;
	const gchar *key;
	gchar *charset;

	/* XXX This function doesn't really belong here. */

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
attachment_set_file_info (EAttachment *attachment,
                          GFileInfo *file_info)
{
	GCancellable *cancellable;

	cancellable = attachment->priv->cancellable;

	/* Cancel any query operations in progress. */
	if (!g_cancellable_is_cancelled (cancellable)) {
		g_cancellable_cancel (cancellable);
		g_cancellable_reset (cancellable);
	}

	if (file_info != NULL)
		g_object_ref (file_info);

	if (attachment->priv->file_info != NULL)
		g_object_unref (attachment->priv->file_info);

	attachment->priv->file_info = file_info;

	g_object_notify (G_OBJECT (attachment), "file-info");

	attachment_notify_model (attachment);
}

static void
attachment_reset (EAttachment *attachment)
{
	GCancellable *cancellable;

	cancellable = attachment->priv->cancellable;

	g_object_freeze_notify (G_OBJECT (attachment));

	/* Cancel any query operations in progress. */
	if (!g_cancellable_is_cancelled (cancellable)) {
		g_cancellable_cancel (cancellable);
		g_cancellable_reset (cancellable);
	}

	if (attachment->priv->file != NULL) {
		g_object_notify (G_OBJECT (attachment), "file");
		g_object_unref (attachment->priv->file);
		attachment->priv->file = NULL;
	}

	if (attachment->priv->mime_part != NULL) {
		g_object_notify (G_OBJECT (attachment), "mime-part");
		g_object_unref (attachment->priv->mime_part);
		attachment->priv->mime_part = NULL;
	}

	attachment_set_file_info (attachment, NULL);

	g_object_thaw_notify (G_OBJECT (attachment));
}

static void
attachment_file_info_ready_cb (GFile *file,
                               GAsyncResult *result,
                               EAttachment *attachment)
{
	GFileInfo *file_info;
	GError *error = NULL;

	/* Even if we failed to obtain a GFileInfo, we still emit a
	 * "notify::file-info" to signal the async operation finished. */
	file_info = g_file_query_info_finish (file, result, &error);
	attachment_set_file_info (attachment, file_info);

	if (file_info != NULL)
		g_object_unref (file_info);
	else {
		g_warning ("%s", error->message);
		g_error_free (error);
	}
}

static void
attachment_file_info_to_mime_part (EAttachment *attachment,
                                   CamelMimePart *mime_part)
{
	GFileInfo *file_info;
	const gchar *attribute;
	const gchar *string;
	gchar *allocated;

	file_info = e_attachment_get_file_info (attachment);

	if (file_info == NULL || mime_part == NULL)
		return;

	/* XXX Note that we skip "standard::size" here. */

	attribute = G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE;
	string = g_file_info_get_attribute_string (file_info, attribute);
	allocated = g_content_type_get_mime_type (string);
	camel_mime_part_set_content_type (mime_part, allocated);
	g_free (allocated);

	attribute = G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME;
	string = g_file_info_get_attribute_string (file_info, attribute);
	camel_mime_part_set_filename (mime_part, string);

	attribute = G_FILE_ATTRIBUTE_STANDARD_DESCRIPTION;
	string = g_file_info_get_attribute_string (file_info, attribute);
	camel_mime_part_set_description (mime_part, string);

	string = e_attachment_get_disposition (attachment);
	camel_mime_part_set_disposition (mime_part, string);
}

static void
attachment_mime_part_to_file_info (EAttachment *attachment)
{
	CamelContentType *content_type;
	CamelMimePart *mime_part;
	GFileInfo *file_info;
	const gchar *attribute;
	const gchar *string;
	gchar *allocated;
	guint64 v_uint64;

	file_info = e_attachment_get_file_info (attachment);
	mime_part = e_attachment_get_mime_part (attachment);

	if (file_info == NULL || mime_part == NULL)
		return;

	attribute = G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE;
	content_type = camel_mime_part_get_content_type (mime_part);
	allocated = camel_content_type_simple (content_type);
	if (allocated != NULL)
		g_file_info_set_attribute_string (
			file_info, attribute, allocated);
	g_free (allocated);

	attribute = G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME;
	string = camel_mime_part_get_filename (mime_part);
	if (string != NULL)
		g_file_info_set_attribute_string (
			file_info, attribute, string);

	attribute = G_FILE_ATTRIBUTE_STANDARD_DESCRIPTION;
	string = camel_mime_part_get_description (mime_part);
	if (string != NULL)
		g_file_info_set_attribute_string (
			file_info, attribute, string);

	attribute = G_FILE_ATTRIBUTE_STANDARD_SIZE;
	v_uint64 = camel_mime_part_get_content_size (mime_part);
	g_file_info_set_attribute_uint64 (file_info, attribute, v_uint64);

	string = camel_mime_part_get_disposition (mime_part);
	e_attachment_set_disposition (attachment, string);
}

static void
attachment_set_property (GObject *object,
                         guint property_id,
                         const GValue *value,
                         GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_DISPOSITION:
			e_attachment_set_disposition (
				E_ATTACHMENT (object),
				g_value_get_string (value));
			return;

		case PROP_FILE:
			e_attachment_set_file (
				E_ATTACHMENT (object),
				g_value_get_object (value));
			return;

		case PROP_MIME_PART:
			e_attachment_set_mime_part (
				E_ATTACHMENT (object),
				g_value_get_boxed (value));
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
		case PROP_DISPOSITION:
			g_value_set_string (
				value, e_attachment_get_disposition (
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

		case PROP_MIME_PART:
			g_value_set_boxed (
				value, e_attachment_get_mime_part (
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

	if (priv->cancellable != NULL) {
		g_cancellable_cancel (priv->cancellable);
		g_object_unref (priv->cancellable);
		priv->cancellable = NULL;
	}

	if (priv->file != NULL) {
		g_object_unref (priv->file);
		priv->file = NULL;
	}

	if (priv->file_info != NULL) {
		g_object_unref (priv->file_info);
		priv->file_info = NULL;
	}

	if (priv->mime_part != NULL) {
		camel_object_unref (priv->mime_part);
		priv->mime_part = NULL;
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
		PROP_DISPOSITION,
		g_param_spec_string (
			"disposition",
			"Disposition",
			NULL,
			"attachment",
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
		PROP_MIME_PART,
		g_param_spec_boxed (
			"mime-part",
			"MIME Part",
			NULL,
			E_TYPE_CAMEL_OBJECT,
			G_PARAM_READWRITE));
}

static void
attachment_init (EAttachment *attachment)
{
	attachment->priv = E_ATTACHMENT_GET_PRIVATE (attachment);
	attachment->priv->cancellable = g_cancellable_new ();
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
	GCancellable *cancellable;

	g_return_if_fail (E_IS_ATTACHMENT (attachment));

	g_object_freeze_notify (G_OBJECT (attachment));

	if (file != NULL) {
		g_return_if_fail (G_IS_FILE (file));
		g_object_ref (file);
	}

	attachment_reset (attachment);
	attachment->priv->file = file;

	cancellable = attachment->priv->cancellable;

	if (file != NULL)
		g_file_query_info_async (
			file, ATTACHMENT_QUERY,
			G_FILE_QUERY_INFO_NONE,
			G_PRIORITY_DEFAULT, cancellable,
			(GAsyncReadyCallback)
			attachment_file_info_ready_cb,
			attachment);

	g_object_notify (G_OBJECT (attachment), "file");

	g_object_thaw_notify (G_OBJECT (attachment));
}

GFileInfo *
e_attachment_get_file_info (EAttachment *attachment)
{
	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), NULL);

	return attachment->priv->file_info;
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
	GFileInfo *file_info;

	g_return_if_fail (E_IS_ATTACHMENT (attachment));

	g_object_freeze_notify (G_OBJECT (attachment));

	if (mime_part != NULL) {
		g_return_if_fail (CAMEL_IS_MIME_PART (mime_part));
		camel_object_ref (mime_part);
	}

	attachment_reset (attachment);
	attachment->priv->mime_part = mime_part;

	file_info = g_file_info_new ();
	attachment_set_file_info (attachment, file_info);
	attachment_mime_part_to_file_info (attachment);
	g_object_unref (file_info);

	g_object_notify (G_OBJECT (attachment), "mime-part");

	g_object_thaw_notify (G_OBJECT (attachment));
}

const gchar *
e_attachment_get_content_type (EAttachment *attachment)
{
	GFileInfo *file_info;
	const gchar *attribute;

	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), NULL);

	attribute = G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE;
	file_info = e_attachment_get_file_info (attachment);

	if (file_info == NULL)
		return NULL;

	return g_file_info_get_attribute_string (file_info, attribute);
}

const gchar *
e_attachment_get_display_name (EAttachment *attachment)
{
	GFileInfo *file_info;
	const gchar *attribute;

	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), NULL);

	attribute = G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME;
	file_info = e_attachment_get_file_info (attachment);

	if (file_info == NULL)
		return NULL;

	return g_file_info_get_attribute_string (file_info, attribute);
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

GIcon *
e_attachment_get_icon (EAttachment *attachment)
{
	GFileInfo *file_info;
	const gchar *attribute;

	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), NULL);

	attribute = G_FILE_ATTRIBUTE_STANDARD_ICON;
	file_info = e_attachment_get_file_info (attachment);

	if (file_info == NULL)
		return NULL;

	return (GIcon *)
		g_file_info_get_attribute_object (file_info, attribute);
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

guint64
e_attachment_get_size (EAttachment *attachment)
{
	GFileInfo *file_info;
	const gchar *attribute;

	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), 0);

	attribute = G_FILE_ATTRIBUTE_STANDARD_SIZE;
	file_info = e_attachment_get_file_info (attachment);

	if (file_info == NULL)
		return 0;

	return g_file_info_get_attribute_uint64 (file_info, attribute);
}

gboolean
e_attachment_is_image (EAttachment *attachment)
{
	const gchar *content_type;

	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), FALSE);

	content_type = e_attachment_get_content_type (attachment);

	if (content_type == NULL)
		return FALSE;

	return g_content_type_is_a (content_type, "image");
}

gboolean
e_attachment_is_rfc822 (EAttachment *attachment)
{
	const gchar *content_type;

	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), FALSE);

	content_type = e_attachment_get_content_type (attachment);

	if (content_type == NULL)
		return FALSE;

	return g_content_type_equals (content_type, "message/rfc822");
}

static void
attachment_save_file_cb (GFile *source,
                         GAsyncResult *result,
                         EActivity *activity)
{
	GError *error = NULL;

	if (!g_file_copy_finish (source, result, &error)) {
		e_activity_set_error (activity, error);
		g_error_free (error);
	}

	e_activity_complete (activity);
	g_object_unref (activity);
}

static gpointer
attachment_save_part_thread (EActivity *activity)
{
	GObject *object;
	EAttachment *attachment;
	GCancellable *cancellable;
	GOutputStream *output_stream;
	EFileActivity *file_activity;
	CamelDataWrapper *wrapper;
	CamelMimePart *mime_part;
	CamelStream *stream;
	GError *error = NULL;

	object = G_OBJECT (activity);
	attachment = g_object_get_data (object, "attachment");
	output_stream = g_object_get_data (object, "output-stream");

	/* Last chance to cancel. */
	file_activity = E_FILE_ACTIVITY (activity);
	cancellable = e_file_activity_get_cancellable (file_activity);
	if (g_cancellable_set_error_if_cancelled (cancellable, &error))
		goto exit;

	object = g_object_ref (output_stream);
	stream = camel_stream_vfs_new_with_stream (object);
	mime_part = e_attachment_get_mime_part (attachment);
	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));

	if (camel_data_wrapper_decode_to_stream (wrapper, stream) < 0)
		g_set_error (
			&error, G_IO_ERROR,
			g_io_error_from_errno (errno),
			g_strerror (errno));

	else if (camel_stream_flush (stream) < 0)
		g_set_error (
			&error, G_IO_ERROR,
			g_io_error_from_errno (errno),
			g_strerror (errno));

	camel_object_unref (stream);

exit:
	if (error != NULL) {
		e_activity_set_error (activity, error);
		g_error_free (error);
	}

	e_activity_complete_in_idle (activity);
	g_object_unref (activity);

	return NULL;
}

static void
attachment_save_part_cb (GFile *destination,
                         GAsyncResult *result,
                         EActivity *activity)
{
	GFileOutputStream *output_stream;
	GError *error = NULL;

	output_stream = g_file_replace_finish (destination, result, &error);

	if (output_stream != NULL) {
		g_object_set_data_full (
			G_OBJECT (activity),
			"output-stream", output_stream,
			(GDestroyNotify) g_object_unref);
		g_thread_create (
			(GThreadFunc) attachment_save_part_thread,
			activity, FALSE, &error);
	}

	if (error != NULL) {
		e_activity_set_error (activity, error);
		e_activity_complete (activity);
		g_object_unref (activity);
		g_error_free (error);
	}

}

void
e_attachment_save_async (EAttachment *attachment,
                         EFileActivity *file_activity,
                         GFile *destination)
{
	GFileProgressCallback progress_callback;
	GCancellable *cancellable;
	CamelMimePart *mime_part;
	GFile *source;

	g_return_if_fail (E_IS_ATTACHMENT (attachment));
	g_return_if_fail (G_IS_FILE (destination));
	g_return_if_fail (E_IS_FILE_ACTIVITY (file_activity));

	/* The attachment content is either a GFile (on disk) or a
	 * CamelMimePart (in memory).  Each is saved differently. */

	source = e_attachment_get_file (attachment);
	mime_part = e_attachment_get_mime_part (attachment);
	g_return_if_fail (source != NULL || mime_part != NULL);

	cancellable = e_file_activity_get_cancellable (file_activity);
	progress_callback = e_file_activity_progress;

	/* GFile is the easier, but probably less common case.  The
	 * attachment already references an on-disk file, so we can
	 * just use GIO to copy it asynchronously.
	 *
	 * We use G_FILE_COPY_OVERWRITE because the user should have
	 * already confirmed the overwrite through the save dialog. */
	if (G_IS_FILE (source))
		g_file_copy_async (
			source, destination,
			G_FILE_COPY_OVERWRITE,
			G_PRIORITY_DEFAULT, cancellable,
			progress_callback, file_activity,
			(GAsyncReadyCallback) attachment_save_file_cb,
			g_object_ref (file_activity));

	/* CamelMimePart can only be decoded to a file synchronously, so
	 * we do this in two stages.  Stage one asynchronously opens the
	 * destination file for writing.  Stage two spawns a thread that
	 * decodes the MIME part to the destination file.  This stage is
	 * not cancellable, unfortunately. */
	else if (CAMEL_IS_MIME_PART (mime_part)) {
		g_object_set_data_full (
			G_OBJECT (file_activity),
			"attachment", g_object_ref (attachment),
			(GDestroyNotify) g_object_unref);
		g_file_replace_async (
			destination, NULL, FALSE,
			G_FILE_CREATE_REPLACE_DESTINATION,
			G_PRIORITY_DEFAULT, cancellable,
			(GAsyncReadyCallback) attachment_save_part_cb,
			g_object_ref (file_activity));
	}
}

#if 0
typedef struct {
	gint io_priority;
	GCancellable *cancellable;
	GSimpleAsyncResult *simple;
	GFileInfo *file_info;
} BuildMimePartData;

static BuildMimePartData *
attachment_build_mime_part_data_new (EAttachment *attachment,
                                     gint io_priority,
                                     GCancellable *cancellable,
                                     GAsyncReadyCallback callback,
                                     gpointer user_data,
                                     gpointer source_tag)
{
	BuildMimePartData *data;
	GSimpleAsyncResult *simple;

	simple = g_simple_async_result_new (
		G_OBJECT (attachment), callback, user_data, source_tag);

	if (G_IS_CANCELLABLE (cancellable))
		g_object_ref (cancellable);

	data = g_slice_new0 (BuildMimePartData);
	data->io_priority = io_priority;
	data->cancellable = cancellable;
	data->simple = simple;
	return data;
}

static void
attachment_build_mime_part_data_free (BuildMimePartData *data)
{
	if (data->attachment != NULL)
		g_object_unref (data->attachment);

	if (data->cancellable != NULL)
		g_object_unref (data->cancellable);

	if (data->simple != NULL)
		g_object_unref (data->simple);

	if (data->file_info != NULL)
		g_object_unref (data->file_info);

	g_slice_free (BuildMimePartData, data);
}

static void
attachment_build_mime_part_splice_cb (GObject *source,
                                      GAsyncResult *result,
                                      gpointer user_data)
{
	GSimpleAsyncResult *final_result;
	GCancellable *cancellable;
	EAttachment *attachment;
	CamelDataWrapper *wrapper;
	CamelMimePart *mime_part;
	CamelStream *stream;
	const gchar *content_type;
	gchar *mime_type;
	gssize length;
	gpointer data;
	GError *error = NULL;

	final_result = G_SIMPLE_ASYNC_RESULT (user_data);

	cancellable = g_cancellable_get_current ();
	g_cancellable_pop_current (cancellable);
	g_object_unref (cancellable);

	length = g_output_stream_splice_finish (
		G_OUTPUT_STREAM (source), result, &error);
	if (error != NULL)
		goto fail;

	data = g_memory_output_stream_get_data (
		G_MEMORY_OUTPUT_STREAM (source));

	attachment = E_ATTACHMENT (
		g_async_result_get_source_object (
		G_ASYNC_RESULT (final_result)));

	if (e_attachment_is_rfc822 (attachment))
		wrapper = (CamelDataWrapper *) camel_mime_message_new ();
	else
		wrapper = camel_data_wrapper_new ();

	content_type = e_attachment_get_content_type (attachment);
	mime_type = g_content_type_get_mime_type (content_type);

	stream = camel_stream_mem_new_with_buffer (data, length);
	camel_data_wrapper_construct_from_stream (wrapper, stream);
	camel_data_wrapper_set_mime_type (wrapper, mime_type);
	camel_object_unref (stream);

	mime_part = camel_mime_part_new ();
	camel_medium_set_content_object (CAMEL_MEDIUM (mime_part), wrapper);

	g_simple_async_result_set_op_res_gpointer (
		final_result, mime_part, camel_object_unref);

	g_simple_async_result_complete (final_result);

	camel_object_unref (wrapper);
	g_free (mime_type);

	return;

fail:
	g_simple_async_result_set_from_error (final_result, error);
	g_simple_async_result_complete (final_result);
	g_error_free (error);
}

static void
attachment_build_mime_part_read_cb (GObject *source,
                                    GAsyncResult *result,
                                    BuildMimePartData *data)
{
	GFileInputStream *input_stream;
	GOutputStream *output_stream;
	GCancellable *cancellable;
	GError *error = NULL;

	input_stream = g_file_read_finish (G_FILE (source), result, &error);
	if (error != NULL)
		goto fail;

	output_stream = g_memory_output_stream_new (
		NULL, 0, g_realloc, g_free);

	g_output_stream_splice_async (
		output_stream, G_INPUT_STREAM (input_stream),
		G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE |
		G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
		G_PRIORITY_DEFAULT, cancellable,
		attachment_build_mime_part_splice_cb, result);

	g_cancellable_push_current (cancellable);

	g_object_unref (input_stream);
	g_object_unref (output_stream);

	return;

fail:
	g_simple_async_result_set_from_error (final_result, error);
	g_simple_async_result_complete (final_result);
	g_error_free (error);
}

static gboolean
attachment_build_mime_part_idle_cb (BuildMimePartData *data)
{
	GObject *source;
	GAsyncResult *result;
	GFileInfo *file_info;
	GFile *file;
	GError *error = NULL;

	if (g_cancellable_set_error_if_cancelled (data->cancellable, &error))
		goto cancelled;

	result = G_ASYNC_RESULT (data->simple);
	source = g_async_result_get_source_object (result);
	file_info = e_attachment_get_file_info (E_ATTACHMENT (source));

	/* Poll again on the next idle. */
	if (!G_IS_FILE_INFO (file_info))
		return TRUE;

	/* We have a GFileInfo, so on to step 2. */

	data->file_info = g_file_info_dup (file_info);
	file = e_attachment_get_file (E_ATTACHMENT (source));

	/* Because Camel's stream API is synchronous and not
	 * cancellable, we have to asynchronously read the file
	 * into memory and then encode it to a MIME part.  That
	 * means double buffering the file contents in memory,
	 * unfortunately. */
	g_file_read_async (
		file, data->io_priority, data->cancellable,
		attachment_build_mime_part_read_cb, data);

	return FALSE;

cancelled:
	g_simple_async_result_set_op_res_gboolean (data->simple, FALSE);
	g_simple_async_result_set_from_error (data->simple, error);
	g_simple_async_result_complete (data->simple);

	build_mime_part_data_free (data);
	g_error_free (error);

	return FALSE;
}

void
e_attachment_build_mime_part_async (EAttachment *attachment,
                                    GCancellable *cancellable,
                                    GAsyncReadyCallback callback,
                                    gpointer user_data)
{
	CamelMimePart *mime_part;
	GSimpleAsyncResult *result;
	GFile *file;

	g_return_if_fail (E_IS_ATTACHMENT (attachment));
	g_return_if_fail (callback != NULL);

	file = e_attachment_get_file (attachment);
	mime_part = e_attachment_get_mime_part (attachment);
	g_return_if_fail (file != NULL || mime_part != NULL);

	result = g_simple_async_result_new (
		G_OBJECT (attachment), callback, user_data,
		e_attachment_build_mime_part_async);

	/* First try the easy way out. */
	if (CAMEL_IS_MIME_PART (mime_part)) {
		camel_object_ref (mime_part);
		g_simple_async_result_set_op_res_gpointer (
			result, mime_part, camel_object_unref);
		g_simple_async_result_complete_in_idle (result);
		return;
	}

	/* XXX g_cancellable_push_current() documentation lies.
	 *     The function rejects NULL pointers, so create a
	 *     dummy GCancellable if necessary. */
	if (cancellable == NULL)
		cancellable = g_cancellable_new ();
	else
		g_object_ref (cancellable);

	/* Because Camel's stream API is synchronous and not
	 * cancellable, we have to asynchronously read the file
	 * into memory and then encode it to a MIME part.  That
	 * means it's double buffered, unfortunately. */
	g_file_read_async (
		file, G_PRIORITY_DEFAULT, cancellable,
		attachment_build_mime_part_read_cb, result);

	g_cancellable_push_current (cancellable);
}

CamelMimePart *
e_attachment_build_mime_part_finish (EAttachment *attachment,
                                     GAsyncResult *result,
                                     GError **error)
{
	CamelMimePart *mime_part;
	GSimpleAsyncResult *simple_result;
	gboolean async_result_is_valid;
	gpointer source_tag;

	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), NULL);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

	source_tag = e_attachment_build_mime_part_async;
	async_result_is_valid = g_simple_async_result_is_valid (
		result, G_OBJECT (attachment), source_tag);
	g_return_val_if_fail (async_result_is_valid, NULL);

	simple_result = G_SIMPLE_ASYNC_RESULT (result);
	g_simple_async_result_propagate_error (simple_result, error);
	mime_part = g_simple_async_result_get_op_res_gpointer (simple_result);
	attachment_file_info_to_mime_part (attachment, mime_part);

	if (CAMEL_IS_MIME_PART (mime_part))
		camel_object_ref (mime_part);

	g_object_unref (result);

	return mime_part;
}
#endif

void
_e_attachment_set_reference (EAttachment *attachment,
                             GtkTreeRowReference *reference)
{
	g_return_if_fail (E_IS_ATTACHMENT (attachment));
	g_return_if_fail (reference != NULL);

	gtk_tree_row_reference_free (attachment->priv->reference);
	attachment->priv->reference = gtk_tree_row_reference_copy (reference);
}
