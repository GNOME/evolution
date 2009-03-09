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
 * Authors:
 *			Ettore Perazzoli <ettore@ximian.com>
 *          Jeffrey Stedfast <fejj@ximian.com>
 *	     	Srinivasa Ragavan <sragavan@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-attachment.h"
#include "e-attachment-dialog.h"

#ifdef G_OS_WIN32
/* Include <windows.h> early (as the gio stuff below will
 * include it anyway, sigh) to workaround the DATADIR problem.
 * <windows.h> (and the headers it includes) stomps all over the
 * namespace like a baboon on crack, and especially the DATADIR enum
 * in objidl.h causes problems.
 */
#undef DATADIR
#define DATADIR crap_DATADIR
#include <windows.h>
#undef DATADIR
#endif

#include <sys/stat.h>
#include <string.h>
#include <errno.h>

#include <camel/camel.h>

#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <libebook/e-vcard.h>

#include "e-util/e-util.h"
#include "e-util/e-error.h"
#include "e-util/e-mktemp.h"
#include "e-util/e-util-private.h"

#define E_ATTACHMENT_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_ATTACHMENT, EAttachmentPrivate))

struct _EAttachmentPrivate {
	gchar *filename;
	gchar *description;
	gchar *disposition;
	gchar *mime_type;

	GdkPixbuf *thumbnail;
	CamelMimePart *mime_part;
};

enum {
	PROP_0,
	PROP_DESCRIPTION,
	PROP_DISPOSITION,
	PROP_FILENAME,
	PROP_THUMBNAIL
};

enum {
	CHANGED,
	UPDATE,
	LAST_SIGNAL
};

static gpointer parent_class;
static guint signals[LAST_SIGNAL];

static void
attachment_set_property (GObject *object,
                         guint property_id,
                         const GValue *value,
                         GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_DESCRIPTION:
			e_attachment_set_description (
				E_ATTACHMENT (object),
				g_value_get_string (value));
			return;

		case PROP_DISPOSITION:
			e_attachment_set_disposition (
				E_ATTACHMENT (object),
				g_value_get_string (value));
			return;

		case PROP_FILENAME:
			e_attachment_set_filename (
				E_ATTACHMENT (object),
				g_value_get_string (value));
			return;

		case PROP_THUMBNAIL:
			e_attachment_set_thumbnail (
				E_ATTACHMENT (object),
				g_value_get_object (value));
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
		case PROP_DESCRIPTION:
			g_value_set_string (
				value, e_attachment_get_description (
				E_ATTACHMENT (object)));
			return;

		case PROP_DISPOSITION:
			g_value_set_string (
				value, e_attachment_get_disposition (
				E_ATTACHMENT (object)));
			return;

		case PROP_FILENAME:
			g_value_set_string (
				value, e_attachment_get_filename (
				E_ATTACHMENT (object)));
			return;

		case PROP_THUMBNAIL:
			g_value_set_object (
				value, e_attachment_get_thumbnail (
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

	if (priv->thumbnail != NULL) {
		g_object_unref (priv->thumbnail);
		priv->thumbnail = NULL;
	}

	if (priv->mime_part != NULL) {
		camel_object_unref (priv->mime_part);
		priv->mime_part = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
attachment_finalize (GObject *object)
{
	EAttachment *attachment = (EAttachment *) object;

	if (attachment->cancellable) {
		/* the operation is still running, so cancel it */
		g_cancellable_cancel (attachment->cancellable);
		attachment->cancellable = NULL;
	}

	g_free (attachment->store_uri);

	g_free (attachment->priv->filename);
	g_free (attachment->priv->description);
	g_free (attachment->priv->disposition);
	g_free (attachment->priv->mime_type);

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
		PROP_DESCRIPTION,
		g_param_spec_string (
			"description",
			"Description",
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_DESCRIPTION,
		g_param_spec_string (
			"disposition",
			"Disposition",
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_DESCRIPTION,
		g_param_spec_string (
			"filename",
			"Filename",
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_THUMBNAIL,
		g_param_spec_object (
			"thumbnail",
			"Thumbnail Image",
			NULL,
			GDK_TYPE_PIXBUF,
			G_PARAM_READWRITE));

	signals[CHANGED] = g_signal_new (
		"changed",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EAttachmentClass, changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[UPDATE] = g_signal_new (
		"update",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EAttachmentClass, update),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
attachment_init (EAttachment *attachment)
{
	attachment->priv = E_ATTACHMENT_GET_PRIVATE (attachment);

	attachment->index = -1;
	attachment->percentage = -1;
	attachment->sign = CAMEL_CIPHER_VALIDITY_SIGN_NONE;
	attachment->encrypt = CAMEL_CIPHER_VALIDITY_ENCRYPT_NONE;
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

/**
 * file_ext_is:
 * @param filename: path for file
 * @param ext: desired extension, with a dot
 * @return if filename has extension ext or not
 **/

static gboolean
file_ext_is (const char *filename, const char *ext)
{
	int i, dot = -1;

	if (!filename || !ext)
		return FALSE;

	for (i = 0; filename[i]; i++) {
		if (filename [i] == '.')
			dot = i;
	}

	if (dot > 0) {
		return 0 == g_ascii_strcasecmp (filename + dot, ext);
	}

	return FALSE;
}

static char *
attachment_guess_mime_type (const char *filename)
{
	char *type;
	gchar *content = NULL;

	type = e_util_guess_mime_type (filename, TRUE);

	if (type && strcmp (type, "text/directory") == 0 &&
	    file_ext_is (filename, ".vcf") &&
	    g_file_get_contents (filename, &content, NULL, NULL) &&
	    content) {
		EVCard *vc = e_vcard_new_from_string (content);

		if (vc) {
			g_free (type);
			g_object_unref (G_OBJECT (vc));

			type = g_strdup ("text/x-vcard");
		}

	}

	g_free (content);

	if (type) {
		/* Check if returned mime_type is valid */
		CamelContentType *ctype = camel_content_type_decode (type);

		if (!ctype) {
			g_free (type);
			type = NULL;
		} else
			camel_content_type_unref (ctype);
	}

	return type;
}


/**
 * e_attachment_new:
 * @filename: filename to attach
 * @disposition: Content-Disposition of the attachment
 * @ex: exception
 *
 * Return value: the new attachment, or %NULL on error
 **/
EAttachment *
e_attachment_new (const char *filename, const char *disposition, CamelException *ex)
{
	EAttachment *new;
	CamelMimePart *part;
	CamelDataWrapper *wrapper;
	CamelStream *stream;
	struct stat statbuf;
	gchar *mime_type;
	gchar *basename;
	CamelURL *url;

	g_return_val_if_fail (filename != NULL, NULL);

	if (g_stat (filename, &statbuf) < 0) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot attach file %s: %s"),
				      filename, g_strerror (errno));
		return NULL;
	}

	/* return if it's not a regular file */
	if (!S_ISREG (statbuf.st_mode)) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot attach file %s: not a regular file"),
				      filename);
		return NULL;
	}

	if (!(stream = camel_stream_fs_new_with_name (filename, O_RDONLY, 0))) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot attach file %s: %s"),
				      filename, g_strerror (errno));
		return NULL;
	}

	if ((mime_type = attachment_guess_mime_type (filename))) {
		if (!g_ascii_strcasecmp (mime_type, "message/rfc822")) {
			wrapper = (CamelDataWrapper *) camel_mime_message_new ();
		} else {
			wrapper = camel_data_wrapper_new ();
		}

		camel_data_wrapper_construct_from_stream (wrapper, stream);
		camel_data_wrapper_set_mime_type (wrapper, mime_type);
		g_free (mime_type);
	} else {
		wrapper = camel_data_wrapper_new ();
		camel_data_wrapper_construct_from_stream (wrapper, stream);
		camel_data_wrapper_set_mime_type (wrapper, "application/octet-stream");
	}

	camel_object_unref (stream);

	part = camel_mime_part_new ();
	camel_medium_set_content_object (CAMEL_MEDIUM (part), wrapper);
	camel_object_unref (wrapper);

	camel_mime_part_set_disposition (part, disposition);
	basename = g_path_get_basename (filename);
	camel_mime_part_set_filename (part, basename);

#if 0
	/* Note: Outlook 2002 is broken with respect to Content-Ids on
           non-multipart/related parts, so as an interoperability
           workaround, don't set a Content-Id on these parts. Fixes
           bug #10032 */
	/* set the Content-Id */
	content_id = camel_header_msgid_generate ();
	camel_mime_part_set_content_id (part, content_id);
	g_free (content_id);
#endif

	new = g_object_new (E_TYPE_ATTACHMENT, "filename", basename, NULL);
	new->priv->mime_part = part;
	new->size = statbuf.st_size;
	new->guessed_type = TRUE;
	new->cancellable = NULL;
	new->is_available_local = TRUE;

	url = camel_url_new ("file://", NULL);
	camel_url_set_path (url, filename);
	new->store_uri = camel_url_to_string (url, 0);
	camel_url_free (url);

	return new;
}


typedef struct {
	EAttachment *attachment;
	char *filename;
	char *uri;
	GtkWindow *parent; /* for error dialog */

	guint64 file_size; /* zero indicates unknown size */
	GInputStream *istream; /* read from here ... */
	GOutputStream *ostream; /* ...and write into this. */
	gboolean was_error;
	GCancellable *cancellable;

	void *buffer; /* read into this, not more than buffer_size bytes */
	gsize buffer_size;
} DownloadInfo;

static void
download_info_free (DownloadInfo *download_info)
{
	/* if there was an error, then free attachment too */
	if (download_info->was_error)
		g_object_unref (download_info->attachment);

	if (download_info->ostream)
		g_object_unref (download_info->ostream);

	if (download_info->istream)
		g_object_unref (download_info->istream);

	if (download_info->cancellable)
		g_object_unref (download_info->cancellable);

	g_free (download_info->filename);
	g_free (download_info->uri);
	g_free (download_info->buffer);
	g_free (download_info);
}

static void
data_ready_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
	DownloadInfo *download_info = (DownloadInfo *)user_data;
	GError *error = NULL;
	gssize read;

	g_return_if_fail (download_info != NULL);

	if (g_cancellable_is_cancelled (download_info->cancellable)) {
		/* finish the operation and close both streams */
		g_input_stream_read_finish (G_INPUT_STREAM (source_object), res, NULL);

		g_output_stream_close (download_info->ostream, NULL, NULL);
		g_input_stream_close  (download_info->istream, NULL, NULL);

		/* The only way how to get this canceled is in EAttachment's finalize method,
		   and because the download_info_free free's the attachment on error,
		   then do not consider cancellation as an error. */
		download_info_free (download_info);
		return;
	}

	read = g_input_stream_read_finish (G_INPUT_STREAM (source_object), res, &error);

	if (!error)
		g_output_stream_write_all (download_info->ostream, download_info->buffer, read, NULL, download_info->cancellable, &error);

	if (error) {
		download_info->was_error = error->domain != G_IO_ERROR || error->code != G_IO_ERROR_CANCELLED;
		if (download_info->was_error)
			e_error_run (download_info->parent, "mail-composer:no-attach", download_info->uri, error->message, NULL);

		g_error_free (error);

		download_info->attachment->cancellable = NULL;
		download_info_free (download_info);
		return;
	}

	if (read == 0) {
		CamelException ex;

		/* done with reading */
		g_output_stream_close (download_info->ostream, NULL, NULL);
		g_input_stream_close  (download_info->istream, NULL, NULL);

		download_info->attachment->cancellable = NULL;

		camel_exception_init (&ex);
		e_attachment_build_remote_file (download_info->filename, download_info->attachment, &ex);

		if (camel_exception_is_set (&ex)) {
			download_info->was_error = TRUE;
			e_error_run (download_info->parent, "mail-composer:no-attach", download_info->uri, camel_exception_get_description (&ex), NULL);
			camel_exception_clear (&ex);
		}

		download_info->attachment->percentage = -1;
		download_info->attachment->is_available_local = TRUE;
		g_signal_emit (download_info->attachment, signals[UPDATE], 0);

		download_info_free (download_info);
		return;
	} else 	if (download_info->file_size) {
		download_info->attachment->percentage = read * 100 / download_info->file_size;
		download_info->file_size -= MIN (download_info->file_size, read);
		g_signal_emit (download_info->attachment, signals[UPDATE], 0);
	} else {
		download_info->attachment->percentage = 0;
		g_signal_emit (download_info->attachment, signals[UPDATE], 0);
	}

	/* read next chunk */
	g_input_stream_read_async (download_info->istream, download_info->buffer, download_info->buffer_size, G_PRIORITY_DEFAULT, download_info->cancellable, data_ready_cb, download_info);
}

static gboolean
download_to_local_path (DownloadInfo *download_info, CamelException *ex)
{
	GError *error = NULL;
	GFile *src = g_file_new_for_uri (download_info->uri);
	GFile *des = g_file_new_for_path (download_info->filename);
	gboolean res = FALSE;

	g_return_val_if_fail (src != NULL && des != NULL, FALSE);

	download_info->ostream = G_OUTPUT_STREAM (g_file_replace (des, NULL, FALSE, G_FILE_CREATE_NONE, NULL, &error));

	if (download_info->ostream && !error) {
		GFileInfo *fi;

		fi = g_file_query_info (src, G_FILE_ATTRIBUTE_STANDARD_SIZE, G_FILE_QUERY_INFO_NONE, NULL, NULL);

		if (fi) {
			download_info->file_size = g_file_info_get_attribute_uint64 (fi, G_FILE_ATTRIBUTE_STANDARD_SIZE);
			g_object_unref (fi);
		} else {
			download_info->file_size = 0;
		}

		download_info->istream = G_INPUT_STREAM (g_file_read (src, NULL, &error));

		if (download_info->istream && !error) {
			download_info->cancellable = g_cancellable_new ();
			download_info->attachment->cancellable = download_info->cancellable;
			download_info->buffer_size = 10240; /* max 10KB chunk */
			download_info->buffer = g_malloc (sizeof (char) * download_info->buffer_size);

			g_input_stream_read_async (download_info->istream, download_info->buffer, download_info->buffer_size, G_PRIORITY_DEFAULT, download_info->cancellable, data_ready_cb, download_info);

			res = TRUE;
		}
	}

	if (error) {
		/* propagate error */
		if (ex)
			camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM, error->message);

		g_error_free (error);
		download_info->was_error = TRUE;
		download_info_free (download_info);
	}

	g_object_unref (src);
	g_object_unref (des);

	return res;
}

EAttachment *
e_attachment_new_remote_file (GtkWindow *error_dlg_parent, const char *uri, const char *disposition, const char *path, CamelException *ex)
{
	EAttachment *new;
	DownloadInfo *download_info;
	CamelURL *url;
	char *base;
	gchar *filename;

	g_return_val_if_fail (uri != NULL, NULL);

	url = camel_url_new (uri, NULL);
	base = g_path_get_basename (url->path);
	camel_url_free (url);

	filename = g_build_filename (path, base, NULL);

	new = g_object_new (E_TYPE_ATTACHMENT, "filename", filename, NULL);
	new->size = 0;
	new->guessed_type = FALSE;
	new->cancellable = NULL;
	new->is_available_local = FALSE;
	new->percentage = 0;

	g_free (base);

	download_info = g_new0 (DownloadInfo, 1);
	download_info->attachment = new;
	download_info->filename = g_strdup (filename);
	download_info->uri = g_strdup (uri);
	download_info->parent = error_dlg_parent;
	download_info->was_error = FALSE;

	g_free (filename);

	/* it frees all on the error, so do not free it twice */
	if (!download_to_local_path (download_info, ex))
		return NULL;

	return new;
}


void
e_attachment_build_remote_file (const gchar *filename,
                                EAttachment *attachment,
                                CamelException *ex)
{
	CamelMimePart *part;
	CamelDataWrapper *wrapper;
	CamelStream *stream;
	struct stat statbuf;
	const gchar *description;
	const gchar *disposition;
	gchar *mime_type;
	gchar *basename;
	CamelURL *url;

	g_return_if_fail (filename != NULL);

	if (g_stat (filename, &statbuf) == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot attach file %s: %s"),
				      filename, g_strerror (errno));
		g_message ("Cannot attach file %s: %s\n", filename, g_strerror (errno));
		return;
	}

	/* return if it's not a regular file */
	if (!S_ISREG (statbuf.st_mode)) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot attach file %s: not a regular file"),
				      filename);
		g_message ("Cannot attach file %s: not a regular file", filename);
		return;
	}

	if (!(stream = camel_stream_fs_new_with_name (filename, O_RDONLY, 0))) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot attach file %s: %s"),
				      filename, g_strerror (errno));
		return;
	}

	if ((mime_type = attachment_guess_mime_type (filename))) {
		if (!g_ascii_strcasecmp (mime_type, "message/rfc822")) {
			wrapper = (CamelDataWrapper *) camel_mime_message_new ();
		} else {
			wrapper = camel_data_wrapper_new ();
		}

		camel_data_wrapper_construct_from_stream (wrapper, stream);
		camel_data_wrapper_set_mime_type (wrapper, mime_type);
		g_free (mime_type);
	} else {
		wrapper = camel_data_wrapper_new ();
		camel_data_wrapper_construct_from_stream (wrapper, stream);
		camel_data_wrapper_set_mime_type (wrapper, "application/octet-stream");
	}

	camel_object_unref (stream);

	part = camel_mime_part_new ();
	camel_medium_set_content_object (CAMEL_MEDIUM (part), wrapper);
	camel_object_unref (wrapper);

	disposition = e_attachment_get_disposition (attachment);
	camel_mime_part_set_disposition (part, disposition);

	if (e_attachment_get_filename (attachment) == NULL)
		basename = g_path_get_basename (filename);
	else
		basename = g_path_get_basename (e_attachment_get_filename (attachment));

	camel_mime_part_set_filename (part, filename);

	description = e_attachment_get_description (attachment);
	if (description != NULL) {
		camel_mime_part_set_description (part, description);
		e_attachment_set_description (attachment, NULL);
	}

	attachment->priv->mime_part = part;
	attachment->size = statbuf.st_size;
	attachment->guessed_type = TRUE;

	e_attachment_set_filename (attachment, basename);

	url = camel_url_new ("file://", NULL);
	camel_url_set_path (url, filename);
	attachment->store_uri = camel_url_to_string (url, 0);
	camel_url_free (url);

	g_free (basename);
}


/**
 * e_attachment_new_from_mime_part:
 * @part: a CamelMimePart
 *
 * Return value: a new EAttachment based on the mime part
 **/
EAttachment *
e_attachment_new_from_mime_part (CamelMimePart *part)
{
	EAttachment *new;
	const gchar *filename;

	g_return_val_if_fail (CAMEL_IS_MIME_PART (part), NULL);

	filename = camel_mime_part_get_filename (part);

	new = g_object_new (E_TYPE_ATTACHMENT, "filename", filename, NULL);
	camel_object_ref (part);
	new->priv->mime_part = part;
	new->guessed_type = FALSE;
	new->is_available_local = TRUE;
	new->size = camel_mime_part_get_content_size (part);

	return new;
}

void
e_attachment_edit (EAttachment *attachment,
                   GtkWindow *parent)
{
	GtkWidget *dialog;

	g_return_if_fail (E_IS_ATTACHMENT (attachment));

	dialog = e_attachment_dialog_new (parent, attachment);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

const gchar *
e_attachment_get_description (EAttachment *attachment)
{
	CamelMimePart *mime_part;
	const gchar *description;

	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), NULL);

	mime_part = e_attachment_get_mime_part (attachment);
	if (mime_part != NULL)
		description = camel_mime_part_get_description (mime_part);
	else 
		description = attachment->priv->description;

	return description;
}

void
e_attachment_set_description (EAttachment *attachment,
                              const gchar *description)
{
	CamelMimePart *mime_part;

	g_return_if_fail (E_IS_ATTACHMENT (attachment));

	g_free (attachment->priv->description);
	attachment->priv->description = g_strdup (description);

	mime_part = e_attachment_get_mime_part (attachment);
	if (mime_part != NULL)
		camel_mime_part_set_description (mime_part, description);

	g_object_notify (G_OBJECT (attachment), "description");
}

const gchar *
e_attachment_get_disposition (EAttachment *attachment)
{
	CamelMimePart *mime_part;
	const gchar *disposition;

	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), NULL);

	mime_part = e_attachment_get_mime_part (attachment);
	if (mime_part != NULL)
		disposition = camel_mime_part_get_disposition (mime_part);
	else
		disposition = attachment->priv->disposition;

	return disposition;
}

void
e_attachment_set_disposition (EAttachment *attachment,
                              const gchar *disposition)
{
	CamelMimePart *mime_part;

	g_return_if_fail (E_IS_ATTACHMENT (attachment));

	g_free (attachment->priv->disposition);
	attachment->priv->disposition = g_strdup (disposition);

	mime_part = e_attachment_get_mime_part (attachment);
	if (mime_part != NULL)
		camel_mime_part_set_disposition (mime_part, disposition);

	g_object_notify (G_OBJECT (attachment), "disposition");
}

const gchar *
e_attachment_get_filename (EAttachment *attachment)
{
	CamelMimePart *mime_part;
	const gchar *filename;

	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), NULL);

	mime_part = e_attachment_get_mime_part (attachment);
	if (mime_part != NULL)
		filename = camel_mime_part_get_filename (mime_part);
	else
		filename = attachment->priv->filename;

	return filename;
}

void
e_attachment_set_filename (EAttachment *attachment,
                           const gchar *filename)
{
	CamelMimePart *mime_part;

	g_return_if_fail (E_IS_ATTACHMENT (attachment));

	g_free (attachment->priv->filename);
	attachment->priv->filename = g_strdup (filename);

	mime_part = e_attachment_get_mime_part (attachment);
	if (mime_part != NULL)
		camel_mime_part_set_filename (mime_part, filename);

	g_object_notify (G_OBJECT (attachment), "filename");
}

CamelMimePart *
e_attachment_get_mime_part (EAttachment *attachment)
{
	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), NULL);

	return attachment->priv->mime_part;
}

const gchar *
e_attachment_get_mime_type (EAttachment *attachment)
{
	CamelContentType *content_type;
	CamelMimePart *mime_part;
	const gchar *filename;
	gchar *mime_type;

	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), NULL);

	if (attachment->priv->mime_type != NULL)
		goto exit;

	mime_part = e_attachment_get_mime_part (attachment);
	filename = e_attachment_get_filename (attachment);
	content_type = camel_mime_part_get_content_type (mime_part);

	if (mime_part == NULL)
		mime_type = attachment_guess_mime_type (filename);
	else {
		content_type = camel_mime_part_get_content_type (mime_part);
		mime_type = camel_content_type_simple (content_type);
	}

	attachment->priv->mime_type = mime_type;

exit:
	return attachment->priv->mime_type;
}

GdkPixbuf *
e_attachment_get_thumbnail (EAttachment *attachment)
{
	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), NULL);

	return attachment->priv->thumbnail;
}

void
e_attachment_set_thumbnail (EAttachment *attachment,
                            GdkPixbuf *thumbnail)
{
	g_return_if_fail (E_IS_ATTACHMENT (attachment));

	if (thumbnail != NULL) {
		g_return_if_fail (GDK_IS_PIXBUF (thumbnail));
		g_object_ref (thumbnail);
	}

	if (attachment->priv->thumbnail != NULL)
		g_object_unref (attachment->priv->thumbnail);

	attachment->priv->thumbnail = thumbnail;

	g_object_notify (G_OBJECT (attachment), "thumbnail");
}

gboolean
e_attachment_is_image (EAttachment *attachment)
{
	CamelContentType *content_type;
	CamelMimePart *mime_part;

	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), FALSE);

	mime_part = e_attachment_get_mime_part (attachment);
	if (mime_part == NULL)
		return FALSE;

	content_type = camel_mime_part_get_content_type (mime_part);

	return camel_content_type_is (content_type, "image", "*");
}

gboolean
e_attachment_is_inline (EAttachment *attachment)
{
	const gchar *disposition;

	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), FALSE);

	disposition = e_attachment_get_disposition (attachment);
	g_return_val_if_fail (disposition != NULL, FALSE);

	return (g_ascii_strcasecmp (disposition, "inline") == 0);
}
