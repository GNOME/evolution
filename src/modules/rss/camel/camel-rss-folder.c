/*
 * SPDX-FileCopyrightText: (C) 2022 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <libsoup/soup.h>

#include "camel-rss-folder-summary.h"
#include "camel-rss-settings.h"
#include "camel-rss-store.h"
#include "camel-rss-store-summary.h"

#include "camel-rss-folder.h"

struct _CamelRssFolderPrivate {
	gboolean apply_filters;
	CamelThreeState complete_articles;
	CamelThreeState feed_enclosures;
	gchar *id;
};

enum {
	PROP_0,
	PROP_APPLY_FILTERS,
	PROP_COMPLETE_ARTICLES,
	PROP_FEED_ENCLOSURES
};

G_DEFINE_TYPE_WITH_PRIVATE (CamelRssFolder, camel_rss_folder, CAMEL_TYPE_FOLDER)

static gchar *
rss_get_filename (CamelFolder *folder,
		  const gchar *uid,
		  GError **error)
{
	CamelStore *parent_store;
	CamelDataCache *rss_cache;
	CamelRssFolder *rss_folder;
	CamelRssStore *rss_store;

	parent_store = camel_folder_get_parent_store (folder);
	rss_folder = CAMEL_RSS_FOLDER (folder);
	rss_store = CAMEL_RSS_STORE (parent_store);
	rss_cache = camel_rss_store_get_cache (rss_store);

	return camel_data_cache_get_filename (rss_cache, rss_folder->priv->id, uid);
}

static gboolean
rss_folder_append_message_sync (CamelFolder *folder,
				CamelMimeMessage *message,
				CamelMessageInfo *info,
				gchar **appended_uid,
				GCancellable *cancellable,
				GError **error)
{
	g_set_error (error, CAMEL_FOLDER_ERROR, CAMEL_FOLDER_ERROR_INVALID,
		_("Cannot add message into News and Blogs folder"));

	return FALSE;
}

static gboolean
rss_folder_expunge_sync (CamelFolder *folder,
			 GCancellable *cancellable,
			 GError **error)
{
	CamelDataCache *cache;
	CamelFolderSummary *summary;
	CamelFolderChangeInfo *changes;
	CamelRssFolder *rss_folder;
	CamelStore *store;
	GPtrArray *known_uids;
	GPtrArray *to_remove = NULL;
	guint ii;

	summary = camel_folder_get_folder_summary (folder);
	store = camel_folder_get_parent_store (folder);

	if (!store)
		return TRUE;

	camel_folder_summary_prepare_fetch_all (summary, NULL);
	known_uids = camel_folder_summary_dup_uids (summary);

	if (known_uids == NULL)
		return TRUE;

	rss_folder = CAMEL_RSS_FOLDER (folder);
	cache = camel_rss_store_get_cache (CAMEL_RSS_STORE (store));
	changes = camel_folder_change_info_new ();

	for (ii = 0; ii < known_uids->len; ii++) {
		guint32 flags;
		const gchar *uid;

		uid = g_ptr_array_index (known_uids, ii);
		flags = camel_folder_summary_get_info_flags (summary, uid);

		if ((flags & CAMEL_MESSAGE_DELETED) != 0) {
			/* ignore cache removal error */
			camel_data_cache_remove (cache, rss_folder->priv->id, uid, NULL);

			camel_folder_change_info_remove_uid (changes, uid);
			if (!to_remove)
				to_remove = g_ptr_array_new ();
			g_ptr_array_add (to_remove, (gpointer) uid);
		}
	}

	if (to_remove) {
		camel_folder_summary_remove_uids (summary, to_remove);
		camel_folder_summary_save (summary, NULL);
		camel_folder_changed (folder, changes);

		g_clear_pointer (&to_remove, g_ptr_array_unref);
	}

	camel_folder_change_info_free (changes);
	g_ptr_array_unref (known_uids);

	return TRUE;
}

static CamelMimeMessage *
rss_folder_get_message_cached (CamelFolder *folder,
			       const gchar *uid,
			       GCancellable *cancellable)
{
	CamelRssFolderSummary *rss_summary;

	g_return_val_if_fail (CAMEL_IS_RSS_FOLDER (folder), NULL);
	g_return_val_if_fail (uid != NULL, NULL);

	rss_summary = CAMEL_RSS_FOLDER_SUMMARY (camel_folder_get_folder_summary (folder));

	return camel_rss_folder_summary_dup_message (rss_summary, uid, NULL, NULL, cancellable, NULL);
}

static CamelMimeMessage *
rss_folder_get_message_sync (CamelFolder *folder,
			     const gchar *uid,
			     GCancellable *cancellable,
			     GError **error)
{
	CamelMimeMessage *message;

	message = rss_folder_get_message_cached (folder, uid, cancellable);

	if (!message) {
		g_set_error_literal (error, CAMEL_SERVICE_ERROR, CAMEL_SERVICE_ERROR_UNAVAILABLE,
			_("Message is not available"));
	}

	return message;
}

static gboolean
rss_folder_refresh_info_sync (CamelFolder *folder,
			      GCancellable *cancellable,
			      GError **error)
{
	CamelRssFolder *self;
	CamelRssFolderSummary *rss_folder_summary;
	CamelRssStore *rss_store;
	CamelRssStoreSummary *rss_store_summary;
	CamelFolderChangeInfo *changes = NULL;
	CamelSession *session;
	gchar *href;
	gchar *last_etag;
	gchar *last_modified;
	gint64 last_updated;
	gboolean success = TRUE;

	self = CAMEL_RSS_FOLDER (folder);
	rss_store = CAMEL_RSS_STORE (camel_folder_get_parent_store (folder));
	session = camel_service_ref_session (CAMEL_SERVICE (rss_store));

	if (!session || !camel_session_get_online (session)) {
		g_clear_object (&session);
		return TRUE;
	}

	g_clear_object (&session);

	rss_store_summary = camel_rss_store_get_summary (rss_store);
	rss_folder_summary = CAMEL_RSS_FOLDER_SUMMARY (camel_folder_get_folder_summary (folder));

	camel_rss_store_summary_lock (rss_store_summary);

	href = g_strdup (camel_rss_store_summary_get_href (rss_store_summary, self->priv->id));
	last_etag = g_strdup (camel_rss_store_summary_get_last_etag (rss_store_summary, self->priv->id));
	last_modified = g_strdup (camel_rss_store_summary_get_last_modified (rss_store_summary, self->priv->id));
	last_updated = camel_rss_store_summary_get_last_updated (rss_store_summary, self->priv->id);

	camel_rss_store_summary_unlock (rss_store_summary);

	if (href && *href) {
		SoupSession *soup_session;
		SoupMessageHeaders *request_headers;
		SoupMessage *message;
		GBytes *bytes;
		GError *local_error = NULL;

		message = soup_message_new (SOUP_METHOD_GET, href);
		if (!message) {
			g_set_error (error, CAMEL_FOLDER_ERROR, CAMEL_FOLDER_ERROR_INVALID, _("Invalid Feed URL “%s”."), href);
			g_free (href);

			return FALSE;
		}

		soup_session = soup_session_new_with_options (
			"timeout", 15,
			"user-agent", "Evolution/" VERSION,
			NULL);

		if (camel_debug ("rss")) {
			SoupLogger *logger;

			logger = soup_logger_new (SOUP_LOGGER_LOG_BODY);
			soup_session_add_feature (soup_session, SOUP_SESSION_FEATURE (logger));
			g_object_unref (logger);
		}

		request_headers = soup_message_get_request_headers (message);

		soup_message_headers_append (request_headers, "Connection", "close");

		if (last_etag && *last_etag)
			soup_message_headers_append (request_headers, "If-None-Match", last_etag);
		else if (last_modified && *last_modified)
			soup_message_headers_append (request_headers, "If-Modified-Since", last_modified);

		bytes = soup_session_send_and_read (soup_session, message, cancellable, &local_error);

		if (soup_message_get_status (message) == SOUP_STATUS_NOT_MODIFIED) {
			g_clear_error (&local_error);
		} else if (bytes) {
			GSList *feeds = NULL;
			gboolean save_summary = FALSE;

			success = SOUP_STATUS_IS_SUCCESSFUL (soup_message_get_status (message));

			if (!success) {
				g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
					_("Failed to download feeds, error code %d (%s)"),
					soup_message_get_status (message),
					soup_message_get_reason_phrase (message) ? soup_message_get_reason_phrase (message) :
					soup_status_get_phrase (soup_message_get_status (message)));
			}

			if (success && e_rss_parser_parse ((const gchar *) g_bytes_get_data (bytes, NULL), g_bytes_get_size (bytes), NULL, NULL, NULL, NULL, &feeds)) {
				CamelSettings *settings;
				CamelRssSettings *rss_settings;
				gboolean download_complete_article;
				gboolean feed_enclosures, limit_feed_enclosure_size;
				guint32 max_feed_enclosure_size;
				gint64 max_last_modified = last_updated;
				GSList *link;

				settings = camel_service_ref_settings (CAMEL_SERVICE (rss_store));
				rss_settings = CAMEL_RSS_SETTINGS (settings);
				limit_feed_enclosure_size = camel_rss_settings_get_limit_feed_enclosure_size (rss_settings);
				max_feed_enclosure_size = camel_rss_settings_get_max_feed_enclosure_size (rss_settings);

				switch (self->priv->complete_articles) {
				case CAMEL_THREE_STATE_ON:
					download_complete_article = TRUE;
					break;
				case CAMEL_THREE_STATE_OFF:
					download_complete_article = FALSE;
					break;
				default:
					download_complete_article = camel_rss_settings_get_complete_articles (rss_settings);
					break;
				}

				switch (self->priv->feed_enclosures) {
				case CAMEL_THREE_STATE_ON:
					feed_enclosures = TRUE;
					break;
				case CAMEL_THREE_STATE_OFF:
					feed_enclosures = FALSE;
					break;
				default:
					feed_enclosures = camel_rss_settings_get_feed_enclosures (rss_settings);
					break;
				}

				g_clear_object (&settings);

				for (link = feeds; link && success; link = g_slist_next (link)) {
					ERssFeed *feed = link->data;

					if (feed->last_modified > last_updated) {
						GBytes *complete_article = NULL;

						if (max_last_modified < feed->last_modified)
							max_last_modified = feed->last_modified;

						if (download_complete_article && feed->link) {
							g_clear_object (&message);
							g_clear_pointer (&bytes, g_bytes_unref);

							message = soup_message_new (SOUP_METHOD_GET, feed->link);
							if (message) {
								complete_article = soup_session_send_and_read (soup_session, message, cancellable, NULL);

								if (!SOUP_STATUS_IS_SUCCESSFUL (soup_message_get_status (message)))
									g_clear_pointer (&complete_article, g_bytes_unref);
							}
						}

						if (success && feed_enclosures && feed->enclosures) {
							GSList *elink;

							for (elink = feed->enclosures; elink && success; elink = g_slist_next (elink)) {
								ERssEnclosure *enclosure = elink->data;

								if (limit_feed_enclosure_size && enclosure->size > max_feed_enclosure_size)
									continue;

								g_clear_object (&message);
								g_clear_pointer (&bytes, g_bytes_unref);

								message = soup_message_new (SOUP_METHOD_GET, enclosure->href);
								if (message) {
									enclosure->data = soup_session_send_and_read (soup_session, message, cancellable, NULL);

									if (!SOUP_STATUS_IS_SUCCESSFUL (soup_message_get_status (message)))
										g_clear_pointer (&enclosure->data, g_bytes_unref);
								}
							}
						}

						success = success && camel_rss_folder_summary_add_or_update_feed_sync (rss_folder_summary, href, feed, complete_article, &changes, cancellable, error);

						g_clear_pointer (&complete_article, g_bytes_unref);
					}
				}

				if (success && max_last_modified != last_updated) {
					camel_rss_store_summary_lock (rss_store_summary);
					camel_rss_store_summary_set_last_updated (rss_store_summary, self->priv->id, max_last_modified);
					camel_rss_store_summary_unlock (rss_store_summary);

					save_summary = TRUE;
				}
			}

			g_slist_free_full (feeds, e_rss_feed_free);

			if (success) {
				SoupMessageHeaders *response_headers;

				response_headers = soup_message_get_response_headers (message);

				if (response_headers) {
					const gchar *tmp;

					camel_rss_store_summary_lock (rss_store_summary);

					tmp = soup_message_headers_get_one (response_headers, "ETag");
					/* ignore weak ETag-s */
					if (tmp && (g_ascii_strncasecmp (tmp, "W/", 2) == 0 || g_ascii_strncasecmp (tmp, "\"W/", 3) == 0))
						tmp = NULL;
					if (tmp && !*tmp)
						tmp = NULL;

					camel_rss_store_summary_set_last_etag (rss_store_summary, self->priv->id, tmp);

					tmp = soup_message_headers_get_one (response_headers, "Last-Modified");
					if (tmp && !*tmp)
						tmp = NULL;

					camel_rss_store_summary_set_last_modified (rss_store_summary, self->priv->id, tmp);

					camel_rss_store_summary_unlock (rss_store_summary);

					save_summary = TRUE;
				}
			}

			if (success && save_summary)
				success = camel_rss_store_summary_save (rss_store_summary, error);
		} else {
			success = FALSE;
			if (local_error)
				g_propagate_error (error, local_error);
		}

		g_clear_pointer (&bytes, g_bytes_unref);
		g_clear_object (&soup_session);
		g_clear_object (&message);
	} else {
		g_set_error (error, CAMEL_FOLDER_ERROR, CAMEL_FOLDER_ERROR_INVALID, _("Invalid Feed URL."));
		success = FALSE;
	}

	g_free (last_modified);
	g_free (last_etag);
	g_free (href);

	if (changes) {
		GError *local_error = NULL;

		if (!camel_folder_summary_save (CAMEL_FOLDER_SUMMARY (rss_folder_summary), (error && !*error) ? error : &local_error)) {
			if (local_error)
				g_warning ("Failed to save RSS folder summary: %s", local_error->message);
		}

		g_clear_error (&local_error);

		camel_folder_changed (folder, changes);
		camel_folder_change_info_free (changes);
	}

	return success;
}

static void
rss_unset_flagged_flag (const gchar *uid,
			CamelFolderSummary *summary)
{
	CamelMessageInfo *info;

	info = camel_folder_summary_get (summary, uid);
	if (info) {
		camel_message_info_set_folder_flagged (info, FALSE);
		g_clear_object (&info);
	}
}

static gboolean
rss_folder_synchronize_sync (CamelFolder *folder,
			     gboolean expunge,
			     GCancellable *cancellable,
			     GError **error)
{
	CamelFolderSummary *summary;
	GPtrArray *changed;

	if (expunge) {
		if (!camel_folder_expunge_sync (folder, cancellable, error))
			return FALSE;
	}

	summary = camel_folder_get_folder_summary (folder);
	changed = camel_folder_summary_dup_changed (summary);

	if (changed) {
		g_ptr_array_foreach (changed, (GFunc) rss_unset_flagged_flag, summary);
		camel_folder_summary_touch (summary);
		g_ptr_array_free (changed, TRUE);
	}

	return camel_folder_summary_save (summary, error);
}

static void
rss_folder_changed (CamelFolder *folder,
		    CamelFolderChangeInfo *info)
{
	g_return_if_fail (CAMEL_IS_RSS_FOLDER (folder));

	if (info && info->uid_removed && info->uid_removed->len) {
		CamelDataCache *rss_cache;

		rss_cache = camel_rss_store_get_cache (CAMEL_RSS_STORE (camel_folder_get_parent_store (folder)));

		if (rss_cache) {
			CamelRssFolder *self = CAMEL_RSS_FOLDER (folder);
			guint ii;

			for (ii = 0; ii < info->uid_removed->len; ii++) {
				const gchar *message_uid = info->uid_removed->pdata[ii], *real_uid;

				if (!message_uid)
					continue;

				real_uid = strchr (message_uid, ',');
				if (real_uid)
					camel_data_cache_remove (rss_cache, self->priv->id, real_uid + 1, NULL);
			}
		}
	}

	/* Chain up to parent's method. */
	CAMEL_FOLDER_CLASS (camel_rss_folder_parent_class)->changed (folder, info);
}

static gboolean
rss_folder_get_apply_filters (CamelRssFolder *folder)
{
	g_return_val_if_fail (CAMEL_IS_RSS_FOLDER (folder), FALSE);

	return folder->priv->apply_filters;
}

static void
rss_folder_set_apply_filters (CamelRssFolder *folder,
			      gboolean apply_filters)
{
	g_return_if_fail (CAMEL_IS_RSS_FOLDER (folder));

	if ((folder->priv->apply_filters ? 1 : 0) == (apply_filters ? 1 : 0))
		return;

	folder->priv->apply_filters = apply_filters;

	g_object_notify (G_OBJECT (folder), "apply-filters");
}

static CamelThreeState
rss_folder_get_complete_articles (CamelRssFolder *folder)
{
	g_return_val_if_fail (CAMEL_IS_RSS_FOLDER (folder), CAMEL_THREE_STATE_INCONSISTENT);

	return folder->priv->complete_articles;
}

static void
rss_folder_set_complete_articles (CamelRssFolder *folder,
				  CamelThreeState value)
{
	g_return_if_fail (CAMEL_IS_RSS_FOLDER (folder));

	if (folder->priv->complete_articles == value)
		return;

	folder->priv->complete_articles = value;

	g_object_notify (G_OBJECT (folder), "complete-articles");
}

static CamelThreeState
rss_folder_get_feed_enclosures (CamelRssFolder *folder)
{
	g_return_val_if_fail (CAMEL_IS_RSS_FOLDER (folder), CAMEL_THREE_STATE_INCONSISTENT);

	return folder->priv->feed_enclosures;
}

static void
rss_folder_set_feed_enclosures (CamelRssFolder *folder,
				CamelThreeState value)
{
	g_return_if_fail (CAMEL_IS_RSS_FOLDER (folder));

	if (folder->priv->feed_enclosures == value)
		return;

	folder->priv->feed_enclosures = value;

	g_object_notify (G_OBJECT (folder), "feed-enclosures");
}

static void
rss_folder_set_property (GObject *object,
			 guint property_id,
			 const GValue *value,
			 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_APPLY_FILTERS:
			rss_folder_set_apply_filters (CAMEL_RSS_FOLDER (object), g_value_get_boolean (value));
			return;
		case PROP_COMPLETE_ARTICLES:
			rss_folder_set_complete_articles (CAMEL_RSS_FOLDER (object), g_value_get_enum (value));
			return;
		case PROP_FEED_ENCLOSURES:
			rss_folder_set_feed_enclosures (CAMEL_RSS_FOLDER (object), g_value_get_enum (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
rss_folder_get_property (GObject *object,
			 guint property_id,
			 GValue *value,
			 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_APPLY_FILTERS:
			g_value_set_boolean (value, rss_folder_get_apply_filters (CAMEL_RSS_FOLDER (object)));
			return;
		case PROP_COMPLETE_ARTICLES:
			g_value_set_enum (value, rss_folder_get_complete_articles (CAMEL_RSS_FOLDER (object)));
			return;
		case PROP_FEED_ENCLOSURES:
			g_value_set_enum (value, rss_folder_get_feed_enclosures (CAMEL_RSS_FOLDER (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
rss_folder_dispose (GObject *object)
{
	camel_folder_summary_save (camel_folder_get_folder_summary (CAMEL_FOLDER (object)), NULL);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (camel_rss_folder_parent_class)->dispose (object);
}

static void
rss_folder_finalize (GObject *object)
{
	CamelRssFolder *self = CAMEL_RSS_FOLDER (object);

	g_free (self->priv->id);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (camel_rss_folder_parent_class)->finalize (object);
}

static void
camel_rss_folder_class_init (CamelRssFolderClass *class)
{
	GObjectClass *object_class;
	CamelFolderClass *folder_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = rss_folder_set_property;
	object_class->get_property = rss_folder_get_property;
	object_class->dispose = rss_folder_dispose;
	object_class->finalize = rss_folder_finalize;

	folder_class = CAMEL_FOLDER_CLASS (class);
	folder_class->get_filename = rss_get_filename;
	folder_class->append_message_sync = rss_folder_append_message_sync;
	folder_class->expunge_sync = rss_folder_expunge_sync;
	folder_class->get_message_cached = rss_folder_get_message_cached;
	folder_class->get_message_sync = rss_folder_get_message_sync;
	folder_class->refresh_info_sync = rss_folder_refresh_info_sync;
	folder_class->synchronize_sync = rss_folder_synchronize_sync;
	folder_class->changed = rss_folder_changed;

	camel_folder_class_map_legacy_property (folder_class, "apply-filters", 0x2501);
	camel_folder_class_map_legacy_property (folder_class, "complete-articles", 0x2502);
	camel_folder_class_map_legacy_property (folder_class, "feed-enclosures", 0x2503);

	g_object_class_install_property (
		object_class,
		PROP_APPLY_FILTERS,
		g_param_spec_boolean (
			"apply-filters",
			"Apply Filters",
			_("Apply message _filters to this folder"),
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_EXPLICIT_NOTIFY |
			G_PARAM_STATIC_STRINGS |
			CAMEL_FOLDER_PARAM_PERSISTENT));

	g_object_class_install_property (
		object_class,
		PROP_COMPLETE_ARTICLES,
		g_param_spec_enum (
			"complete-articles",
			"Complete Articles",
			_("_Download complete articles"),
			CAMEL_TYPE_THREE_STATE,
			CAMEL_THREE_STATE_INCONSISTENT,
			G_PARAM_READWRITE |
			G_PARAM_EXPLICIT_NOTIFY |
			CAMEL_FOLDER_PARAM_PERSISTENT));

	g_object_class_install_property (
		object_class,
		PROP_FEED_ENCLOSURES,
		g_param_spec_enum (
			"feed-enclosures",
			"Feed Enclosures",
			_("Download feed _enclosures"),
			CAMEL_TYPE_THREE_STATE,
			CAMEL_THREE_STATE_INCONSISTENT,
			G_PARAM_READWRITE |
			G_PARAM_EXPLICIT_NOTIFY |
			CAMEL_FOLDER_PARAM_PERSISTENT));
}

static void
camel_rss_folder_init (CamelRssFolder *self)
{
	self->priv = camel_rss_folder_get_instance_private (self);
	self->priv->complete_articles = CAMEL_THREE_STATE_INCONSISTENT;
	self->priv->feed_enclosures = CAMEL_THREE_STATE_INCONSISTENT;
}

CamelFolder *
camel_rss_folder_new (CamelStore *parent,
		      const gchar *id,
		      GCancellable *cancellable,
		      GError **error)
{
	CamelFolder *folder;
	CamelRssFolder *self;
	CamelFolderSummary *folder_summary;
	CamelRssStoreSummary *store_summary;
	gchar *storage_path, *root;
	CamelService *service;
	CamelSettings *settings;
	const gchar *user_data_dir;
	gboolean filter_all = FALSE;

	g_return_val_if_fail (id != NULL, NULL);

	store_summary = camel_rss_store_get_summary (CAMEL_RSS_STORE (parent));
	g_return_val_if_fail (store_summary != NULL, NULL);

	service = CAMEL_SERVICE (parent);
	user_data_dir = camel_service_get_user_data_dir (service);

	settings = camel_service_ref_settings (service);

	g_object_get (
		settings,
		"filter-all", &filter_all,
		NULL);

	g_object_unref (settings);

	camel_rss_store_summary_lock (store_summary);

	folder = g_object_new (
		CAMEL_TYPE_RSS_FOLDER,
		"display-name", camel_rss_store_summary_get_display_name (store_summary, id),
		"full-name", id,
		"parent-store", parent, NULL);

	camel_rss_store_summary_unlock (store_summary);

	self = CAMEL_RSS_FOLDER (folder);
	self->priv->id = g_strdup (id);

	camel_folder_set_flags (folder, camel_folder_get_flags (folder) | CAMEL_FOLDER_HAS_SUMMARY_CAPABILITY);

	storage_path = g_build_filename (user_data_dir, id, NULL);
	root = g_strdup_printf ("%s.cmeta", storage_path);
	camel_folder_take_state_filename (folder, g_steal_pointer (&root));
	camel_folder_load_state (folder);
	g_free (storage_path);

	folder_summary = camel_rss_folder_summary_new (folder);

	camel_folder_take_folder_summary (folder, folder_summary);

	if (filter_all || rss_folder_get_apply_filters (self))
		camel_folder_set_flags (folder, camel_folder_get_flags (folder) | CAMEL_FOLDER_FILTER_RECENT);

	camel_folder_summary_load (folder_summary, NULL);

	return folder;
}

const gchar *
camel_rss_folder_get_id (CamelRssFolder *self)
{
	g_return_val_if_fail (CAMEL_IS_RSS_FOLDER (self), NULL);

	return self->priv->id;
}
