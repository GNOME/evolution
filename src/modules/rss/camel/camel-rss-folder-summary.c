/*
 * SPDX-FileCopyrightText: (C) 2022 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include <camel/camel.h>
#include <libedataserver/libedataserver.h>

#include "camel-rss-folder.h"
#include "camel-rss-store.h"
#include "camel-rss-folder-summary.h"

struct _CamelRssFolderSummaryPrivate {
	gulong saved_count_id;
	gulong unread_count_id;
};

G_DEFINE_TYPE_WITH_PRIVATE (CamelRssFolderSummary, camel_rss_folder_summary, CAMEL_TYPE_FOLDER_SUMMARY)

static void
rss_folder_summary_sync_counts_cb (GObject *object,
				   GParamSpec *param,
				   gpointer user_data)
{
	CamelRssFolderSummary *self = CAMEL_RSS_FOLDER_SUMMARY (object);
	CamelFolder *folder;
	CamelStore *parent_store;
	CamelRssStoreSummary *rss_store_summary;
	const gchar *id;

	folder = camel_folder_summary_get_folder (CAMEL_FOLDER_SUMMARY (self));
	parent_store = camel_folder_get_parent_store (folder);

	if (!parent_store)
		return;

	rss_store_summary = camel_rss_store_get_summary (CAMEL_RSS_STORE (parent_store));

	if (!rss_store_summary)
		return;

	id = camel_rss_folder_get_id (CAMEL_RSS_FOLDER (folder));

	if (g_strcmp0 (g_param_spec_get_name (param), "saved-count") == 0)
		camel_rss_store_summary_set_total_count (rss_store_summary, id, camel_folder_summary_get_saved_count (CAMEL_FOLDER_SUMMARY (self)));
	else if (g_strcmp0 (g_param_spec_get_name (param), "unread-count") == 0)
		camel_rss_store_summary_set_unread_count (rss_store_summary, id, camel_folder_summary_get_unread_count (CAMEL_FOLDER_SUMMARY (self)));
}

static void
rss_folder_summary_constructed (GObject *object)
{
	CamelRssFolderSummary *self = CAMEL_RSS_FOLDER_SUMMARY (object);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (camel_rss_folder_summary_parent_class)->constructed (object);

	self->priv->saved_count_id = g_signal_connect (self, "notify::saved-count",
		G_CALLBACK (rss_folder_summary_sync_counts_cb), NULL);

	self->priv->unread_count_id = g_signal_connect (self, "notify::unread-count",
		G_CALLBACK (rss_folder_summary_sync_counts_cb), NULL);
}

static void
rss_folder_summary_dispose (GObject *object)
{
	CamelRssFolderSummary *self = CAMEL_RSS_FOLDER_SUMMARY (object);

	if (self->priv->saved_count_id) {
		g_signal_handler_disconnect (self, self->priv->saved_count_id);
		self->priv->saved_count_id = 0;
	}

	if (self->priv->unread_count_id) {
		g_signal_handler_disconnect (self, self->priv->unread_count_id);
		self->priv->unread_count_id = 0;
	}

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (camel_rss_folder_summary_parent_class)->dispose (object);
}

static void
camel_rss_folder_summary_class_init (CamelRssFolderSummaryClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = rss_folder_summary_constructed;
	object_class->dispose = rss_folder_summary_dispose;
}

static void
camel_rss_folder_summary_init (CamelRssFolderSummary *rss_folder_summary)
{
	rss_folder_summary->priv = camel_rss_folder_summary_get_instance_private (rss_folder_summary);
}

CamelFolderSummary *
camel_rss_folder_summary_new (CamelFolder *folder)
{
	return g_object_new (CAMEL_TYPE_RSS_FOLDER_SUMMARY, "folder", folder, NULL);
}

CamelMimeMessage *
camel_rss_folder_summary_dup_message (CamelRssFolderSummary *self,
				      const gchar *uid,
				      CamelDataCache **out_rss_cache,
				      CamelRssContentType *out_content_type,
				      GCancellable *cancellable,
				      GError **error)
{
	CamelFolder *folder;
	CamelDataCache *rss_cache;
	CamelMimeMessage *message = NULL;
	CamelRssStore *rss_store;
	GIOStream *base_stream;

	g_return_val_if_fail (CAMEL_IS_RSS_FOLDER_SUMMARY (self), NULL);
	g_return_val_if_fail (uid != NULL, NULL);

	folder = camel_folder_summary_get_folder (CAMEL_FOLDER_SUMMARY (self));
	rss_store = CAMEL_RSS_STORE (camel_folder_get_parent_store (folder));

	if (out_content_type) {
		*out_content_type = camel_rss_store_summary_get_content_type (
			camel_rss_store_get_summary (rss_store),
			camel_rss_folder_get_id (CAMEL_RSS_FOLDER (folder)));
	}

	rss_cache = camel_rss_store_get_cache (rss_store);
	base_stream = camel_data_cache_get (rss_cache, camel_rss_folder_get_id (CAMEL_RSS_FOLDER (folder)), uid, error);

	if (base_stream) {
		CamelStream *stream;

		stream = camel_stream_new (base_stream);
		g_object_unref (base_stream);

		message = camel_mime_message_new ();
		if (!camel_data_wrapper_construct_from_stream_sync (CAMEL_DATA_WRAPPER (message), stream, cancellable, error)) {
			g_object_unref (message);
			message = NULL;
		}

		g_object_unref (stream);
	}

	if (out_rss_cache)
		*out_rss_cache = g_object_ref (rss_cache);

	return message;
}

gboolean
camel_rss_folder_summary_add_or_update_feed_sync (CamelRssFolderSummary *self,
						  const gchar *href,
						  ERssFeed *feed,
						  GBytes *complete_article,
						  CamelFolderChangeInfo **inout_changes,
						  GCancellable *cancellable,
						  GError **error)
{
	CamelDataCache *rss_cache = NULL;
	CamelDataWrapper *body_wrapper;
	CamelMimeMessage *message;
	CamelRssContentType content_type = CAMEL_RSS_CONTENT_TYPE_HTML;
	gchar *uid, *received, *received_tm;
	GSList *link;
	gboolean has_downloaded_eclosure = FALSE;
	gboolean existing_message;
	gboolean success = TRUE;

	g_return_val_if_fail (CAMEL_IS_RSS_FOLDER_SUMMARY (self), FALSE);
	g_return_val_if_fail (href != NULL, FALSE);
	g_return_val_if_fail (feed != NULL, FALSE);
	g_return_val_if_fail (inout_changes != NULL, FALSE);

	uid = g_compute_checksum_for_string (G_CHECKSUM_SHA1, feed->id ? feed->id : (feed->link ? feed->link : feed->title), -1);
	g_return_val_if_fail (uid != NULL, FALSE);

	message = camel_rss_folder_summary_dup_message (self, uid, &rss_cache, &content_type, cancellable, NULL);
	existing_message = camel_folder_summary_get_info_flags (CAMEL_FOLDER_SUMMARY (self), uid) != (~0);
	if (!existing_message)
		g_clear_object (&message);
	if (!message) {
		gchar *msg_id;

		msg_id = g_strconcat (uid, "@localhost", NULL);

		message = camel_mime_message_new ();

		camel_mime_message_set_message_id (message, msg_id);
		camel_mime_message_set_date (message, feed->last_modified, 0);
		camel_medium_set_header (CAMEL_MEDIUM (message), "From", feed->author);
		camel_medium_set_header (CAMEL_MEDIUM (message), "X-RSS-Feed", href);

		g_free (msg_id);
	}

	camel_mime_message_set_subject (message, feed->title);

	received_tm = camel_header_format_date (time (NULL), 0);
	received = g_strconcat ("from ", href, " by localhost; ", received_tm, NULL);

	camel_medium_add_header (CAMEL_MEDIUM (message), "Received", received);

	for (link = feed->enclosures; link && !has_downloaded_eclosure; link = g_slist_next (link)) {
		ERssEnclosure *enclosure = link->data;

		if (enclosure->data && g_bytes_get_size (enclosure->data) > 0)
			has_downloaded_eclosure = TRUE;
	}

	body_wrapper = camel_data_wrapper_new ();

	if (complete_article && g_bytes_get_size (complete_article) > 0) {
		camel_data_wrapper_set_encoding (body_wrapper, CAMEL_TRANSFER_ENCODING_8BIT);
		camel_data_wrapper_set_mime_type (body_wrapper, "text/html; charset=utf-8");
		if (feed->link) {
			GByteArray *bytes;
			gchar *tmp;

			/* in case the complete article uses relative paths, not full URI-s */
			tmp = g_markup_printf_escaped ("<base href=\"%s\">", feed->link);
			bytes = g_byte_array_sized_new (strlen (tmp) + g_bytes_get_size (complete_article) + 1);
			g_byte_array_append (bytes, (const guint8 *) tmp, strlen (tmp));
			g_byte_array_append (bytes, g_bytes_get_data (complete_article, NULL), g_bytes_get_size (complete_article));

			success = camel_data_wrapper_construct_from_data_sync (body_wrapper, bytes->data, bytes->len, cancellable, error);

			g_free (tmp);
			g_byte_array_unref (bytes);
		} else {
			success = camel_data_wrapper_construct_from_data_sync (body_wrapper, g_bytes_get_data (complete_article, NULL), g_bytes_get_size (complete_article), cancellable, error);
		}
	} else {
		GString *body;
		const gchar *ct;
		gboolean first_enclosure = TRUE;

		body = g_string_new (NULL);

		if (content_type == CAMEL_RSS_CONTENT_TYPE_PLAIN_TEXT) {
			ct = "text/plain; charset=utf-8";

			if (feed->link)
				g_string_append (body, feed->link);
			else
				g_string_append (body, feed->title);
			g_string_append_c (body, '\n');
			g_string_append_c (body, '\n');
		} else if (content_type == CAMEL_RSS_CONTENT_TYPE_MARKDOWN) {
			ct = "text/markdown; charset=utf-8";
		} else {
			ct = "text/html; charset=utf-8";
		}

		if (content_type != CAMEL_RSS_CONTENT_TYPE_PLAIN_TEXT) {
			gchar *tmp;

			if (feed->link) {
				tmp = g_markup_printf_escaped ("<base href=\"%s\"><h4><a href=\"%s\">%s</a></h4><div><br></div>", feed->link, feed->link, feed->title);
			} else {
				tmp = g_markup_printf_escaped ("<h4>%s</h4><div><br></div>", feed->title);
			}
			g_string_append (body, tmp);
			g_free (tmp);
		}

		if (feed->body)
			g_string_append (body, feed->body);

		for (link = feed->enclosures; link; link = g_slist_next (link)) {
			ERssEnclosure *enclosure = link->data;
			gchar *tmp;

			if (enclosure->data && g_bytes_get_size (enclosure->data) > 0)
				continue;

			if (first_enclosure) {
				first_enclosure = FALSE;

				g_string_append (body, "<br><hr><br>\n");
				g_string_append (body, _("Enclosures:"));
				g_string_append (body, "<br>\n");
			}

			tmp = g_markup_printf_escaped ("<div><a href=\"%s\">%s</a></div>\n",
				enclosure->href, enclosure->title ? enclosure->title : enclosure->href);

			g_string_append (body, tmp);

			g_free (tmp);
		}

		camel_data_wrapper_set_encoding (body_wrapper, CAMEL_TRANSFER_ENCODING_8BIT);
		camel_data_wrapper_set_mime_type (body_wrapper, ct);
		success = camel_data_wrapper_construct_from_data_sync (body_wrapper, body->str, body->len, cancellable, error);

		g_string_free (body, TRUE);
	}

	if (success && has_downloaded_eclosure) {
		CamelMultipart *mixed;
		CamelMimePart *subpart;

		mixed = camel_multipart_new ();
		camel_data_wrapper_set_mime_type (CAMEL_DATA_WRAPPER (mixed), "multipart/mixed");
		camel_multipart_set_boundary (mixed, NULL);

		subpart = camel_mime_part_new ();
		camel_medium_set_content (CAMEL_MEDIUM (subpart), body_wrapper);
		camel_multipart_add_part (mixed, subpart);
		g_object_unref (subpart);

		for (link = feed->enclosures; link; link = g_slist_next (link)) {
			ERssEnclosure *enclosure = link->data;
			GUri *link_uri;

			if (!enclosure->data || !g_bytes_get_size (enclosure->data))
				continue;

			subpart = camel_mime_part_new ();
			camel_mime_part_set_content (subpart, (const gchar *) g_bytes_get_data (enclosure->data, NULL), g_bytes_get_size (enclosure->data),
				enclosure->content_type ? enclosure->content_type : "application/octet-stream");

			camel_mime_part_set_disposition (subpart, "inline");

			link_uri = g_uri_parse (enclosure->href, G_URI_FLAGS_PARSE_RELAXED, NULL);
			if (link_uri) {
				const gchar *path = g_uri_get_path (link_uri);
				const gchar *slash = path ? strrchr (path, '/') : NULL;

				if (slash && *slash && slash[1])
					camel_mime_part_set_filename (subpart, slash + 1);

				g_uri_unref (link_uri);
			}

			camel_mime_part_set_encoding (subpart, CAMEL_TRANSFER_ENCODING_BASE64);

			camel_multipart_add_part (mixed, subpart);

			g_object_unref (subpart);
		}

		g_object_unref (body_wrapper);
		body_wrapper = CAMEL_DATA_WRAPPER (mixed);
	}

	if (CAMEL_IS_MIME_PART (body_wrapper)) {
		CamelDataWrapper *content;
		CamelMedium *imedium, *omedium;
		const CamelNameValueArray *headers;

		imedium = CAMEL_MEDIUM (body_wrapper);
		omedium = CAMEL_MEDIUM (message);

		content = camel_medium_get_content (imedium);
		camel_medium_set_content (omedium, content);
		camel_data_wrapper_set_encoding (CAMEL_DATA_WRAPPER (omedium), camel_data_wrapper_get_encoding (CAMEL_DATA_WRAPPER (imedium)));

		headers = camel_medium_get_headers (imedium);
		if (headers) {
			gint ii, length;
			length = camel_name_value_array_get_length (headers);

			for (ii = 0; ii < length; ii++) {
				const gchar *header_name = NULL;
				const gchar *header_value = NULL;

				if (camel_name_value_array_get (headers, ii, &header_name, &header_value))
					camel_medium_set_header (omedium, header_name, header_value);
			}
		}
	} else {
		camel_medium_set_content (CAMEL_MEDIUM (message), body_wrapper);
	}

	if (success) {
		CamelRssFolder *rss_folder;
		GIOStream *io_stream;

		rss_folder = CAMEL_RSS_FOLDER (camel_folder_summary_get_folder (CAMEL_FOLDER_SUMMARY (self)));
		io_stream = camel_data_cache_add (rss_cache, camel_rss_folder_get_id (rss_folder), uid, error);
		success = io_stream != NULL;

		if (io_stream) {
			success = camel_data_wrapper_write_to_output_stream_sync (CAMEL_DATA_WRAPPER (message),
				g_io_stream_get_output_stream (io_stream), cancellable, error);
		}

		g_clear_object (&io_stream);
	}

	if (success) {
		if (!*inout_changes)
			*inout_changes = camel_folder_change_info_new ();

		if (existing_message) {
			camel_folder_change_info_change_uid (*inout_changes, uid);
		} else {
			CamelFolderSummary *folder_summary = CAMEL_FOLDER_SUMMARY (self);
			CamelMessageInfo *info;

			info = camel_folder_summary_info_new_from_message (folder_summary, message);
			g_warn_if_fail (info != NULL);

			camel_message_info_set_uid (info, uid);
			camel_folder_summary_add (folder_summary, info, TRUE);

			g_clear_object (&info);

			camel_folder_change_info_add_uid (*inout_changes, uid);
			camel_folder_change_info_recent_uid (*inout_changes, uid);
		}
	}

	g_clear_object (&rss_cache);
	g_clear_object (&body_wrapper);
	g_clear_object (&message);
	g_free (received_tm);
	g_free (received);
	g_free (uid);

	return success;
}
