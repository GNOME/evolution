/*
 * SPDX-FileCopyrightText: (C) 2022 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <errno.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <libedataserver/libedataserver.h>

#include "camel-rss-store-summary.h"

struct _CamelRssStoreSummaryPrivate {
	GRecMutex mutex;
	gboolean dirty;
	gchar *filename;
	GHashTable *feeds; /* gchar *uid ~> RssFeed * */
};

enum {
	FEED_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_PRIVATE (CamelRssStoreSummary, camel_rss_store_summary, G_TYPE_OBJECT)

typedef struct _RssFeed {
	guint index; /* to preserve order of adding */
	gchar *href;
	gchar *display_name;
	gchar *icon_filename;
	gchar *last_etag;
	gchar *last_modified;
	CamelRssContentType content_type;
	guint32 total_count;
	guint32 unread_count;
	gint64 last_updated;
} RssFeed;

static void
rss_feed_free (gpointer ptr)
{
	RssFeed *feed = ptr;

	if (feed) {
		g_free (feed->href);
		g_free (feed->display_name);
		g_free (feed->icon_filename);
		g_free (feed->last_etag);
		g_free (feed->last_modified);
		g_free (feed);
	}
}

typedef struct _EmitIdleData {
	GWeakRef *weak_ref;
	gchar *id;
} EmitIdleData;

static void
emit_idle_data_free (gpointer ptr)
{
	EmitIdleData *eid = ptr;

	if (eid) {
		e_weak_ref_free (eid->weak_ref);
		g_free (eid->id);
		g_slice_free (EmitIdleData, eid);
	}
}

static gboolean
camel_rss_store_summary_emit_feed_changed_cb (gpointer user_data)
{
	EmitIdleData *eid = user_data;
	CamelRssStoreSummary *self;

	self = g_weak_ref_get (eid->weak_ref);
	if (self) {
		g_signal_emit (self, signals[FEED_CHANGED], 0, eid->id, NULL);
		g_object_unref (self);
	}

	return G_SOURCE_REMOVE;
}

static void
camel_rss_store_summary_schedule_feed_changed (CamelRssStoreSummary *self,
					       const gchar *id)
{
	EmitIdleData *eid;

	eid = g_slice_new (EmitIdleData);
	eid->weak_ref = e_weak_ref_new (self);
	eid->id = g_strdup (id);

	g_idle_add_full (G_PRIORITY_HIGH,
		camel_rss_store_summary_emit_feed_changed_cb,
		eid, emit_idle_data_free);
}

static void
rss_store_summary_finalize (GObject *object)
{
	CamelRssStoreSummary *self = CAMEL_RSS_STORE_SUMMARY (object);

	g_hash_table_destroy (self->priv->feeds);
	g_free (self->priv->filename);

	g_rec_mutex_clear (&self->priv->mutex);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (camel_rss_store_summary_parent_class)->finalize (object);
}

static void
camel_rss_store_summary_class_init (CamelRssStoreSummaryClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = rss_store_summary_finalize;

	signals[FEED_CHANGED] = g_signal_new (
		"feed-changed",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST |
		G_SIGNAL_ACTION,
		0,
		NULL, NULL, NULL,
		G_TYPE_NONE, 1,
		G_TYPE_STRING);
}

static void
camel_rss_store_summary_init (CamelRssStoreSummary *self)
{
	self->priv = camel_rss_store_summary_get_instance_private (self);

	self->priv->dirty = FALSE;
	self->priv->feeds = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, rss_feed_free);

	g_rec_mutex_init (&self->priv->mutex);
}

CamelRssStoreSummary *
camel_rss_store_summary_new (const gchar *filename)
{
	CamelRssStoreSummary *self = g_object_new (CAMEL_TYPE_RSS_STORE_SUMMARY, NULL);

	self->priv->filename = g_strdup (filename);

	return self;
}

void
camel_rss_store_summary_lock (CamelRssStoreSummary *self)
{
	g_return_if_fail (CAMEL_IS_RSS_STORE_SUMMARY (self));

	g_rec_mutex_lock (&self->priv->mutex);
}

void
camel_rss_store_summary_unlock (CamelRssStoreSummary *self)
{
	g_return_if_fail (CAMEL_IS_RSS_STORE_SUMMARY (self));

	g_rec_mutex_unlock (&self->priv->mutex);
}

static gint
compare_feeds_by_index (gconstpointer fd1,
			gconstpointer fd2)
{
	const RssFeed *feed1 = fd1, *feed2 = fd2;

	if (!feed1 || !feed2)
		return 0;

	return feed1->index - feed2->index;
}

gboolean
camel_rss_store_summary_load (CamelRssStoreSummary *self,
			      GError **error)
{
	GKeyFile *key_file;
	GError *local_error = NULL;
	gboolean success;

	g_return_val_if_fail (CAMEL_IS_RSS_STORE_SUMMARY (self), FALSE);

	camel_rss_store_summary_lock (self);

	g_hash_table_remove_all (self->priv->feeds);

	key_file = g_key_file_new ();
	success = g_key_file_load_from_file (key_file, self->priv->filename, G_KEY_FILE_NONE, &local_error);

	if (success) {
		GSList *feeds = NULL, *link;
		gchar **groups;
		guint ii;

		groups = g_key_file_get_groups (key_file, NULL);

		for (ii = 0; groups && groups[ii]; ii++) {
			const gchar *group = groups[ii];

			if (g_str_has_prefix (group, "feed:")) {
				RssFeed *feed;

				feed = g_new0 (RssFeed, 1);
				feed->href = g_key_file_get_string (key_file, group, "href", NULL);
				feed->display_name = g_key_file_get_string (key_file, group, "display-name", NULL);
				feed->icon_filename = g_key_file_get_string (key_file, group, "icon-filename", NULL);
				feed->last_etag = g_key_file_get_string (key_file, group, "last-etag", NULL);
				feed->last_modified = g_key_file_get_string (key_file, group, "last-modified", NULL);
				feed->content_type = g_key_file_get_integer (key_file, group, "content-type", NULL);
				feed->total_count = (guint32) g_key_file_get_uint64 (key_file, group, "total-count", NULL);
				feed->unread_count = (guint32) g_key_file_get_uint64 (key_file, group, "unread-count", NULL);
				feed->last_updated = g_key_file_get_int64 (key_file, group, "last-updated", NULL);
				feed->index = (gint) g_key_file_get_int64 (key_file, group, "index", NULL);

				if (feed->href && *feed->href && feed->display_name && *feed->display_name) {
					if (feed->icon_filename && !*feed->icon_filename)
						g_clear_pointer (&feed->icon_filename, g_free);

					g_hash_table_insert (self->priv->feeds, g_strdup (group + 5 /* strlen ("feed:") */), feed);

					feeds = g_slist_prepend (feeds, feed);
				} else {
					rss_feed_free (feed);
				}
			}
		}

		/* renumber indexes on load */
		feeds = g_slist_sort (feeds, compare_feeds_by_index);

		for (ii = 1, link = feeds; link; ii++, link = g_slist_next (link)) {
			RssFeed *feed = link->data;

			feed->index = ii;
		}

		g_slist_free (feeds);
		g_strfreev (groups);
	} else {
		if (g_error_matches (local_error, G_FILE_ERROR, G_FILE_ERROR_NOENT)) {
			success = TRUE;
			g_clear_error (&local_error);
		} else {
			g_propagate_error (error, local_error);
		}
	}

	g_key_file_free (key_file);

	self->priv->dirty = FALSE;

	camel_rss_store_summary_unlock (self);

	return success;
}

gboolean
camel_rss_store_summary_save (CamelRssStoreSummary *self,
			      GError **error)
{
	gboolean success = TRUE;

	g_return_val_if_fail (CAMEL_IS_RSS_STORE_SUMMARY (self), FALSE);

	camel_rss_store_summary_lock (self);

	if (self->priv->dirty) {
		GKeyFile *key_file;
		GHashTableIter iter;
		gpointer key, value;

		key_file = g_key_file_new ();

		g_hash_table_iter_init (&iter, self->priv->feeds);

		while (g_hash_table_iter_next (&iter, &key, &value)) {
			const gchar *id = key;
			const RssFeed *feed = value;
			gchar *group = g_strconcat ("feed:", id, NULL);

			g_key_file_set_string (key_file, group, "href", feed->href);
			g_key_file_set_string (key_file, group, "display-name", feed->display_name);
			g_key_file_set_string (key_file, group, "icon-filename", feed->icon_filename ? feed->icon_filename : "");
			g_key_file_set_string (key_file, group, "last-etag", feed->last_etag ? feed->last_etag : "");
			g_key_file_set_string (key_file, group, "last-modified", feed->last_modified ? feed->last_modified : "");
			g_key_file_set_integer (key_file, group, "content-type", feed->content_type);
			g_key_file_set_uint64 (key_file, group, "total-count", feed->total_count);
			g_key_file_set_uint64 (key_file, group, "unread-count", feed->unread_count);
			g_key_file_set_int64 (key_file, group, "last-updated", feed->last_updated);
			g_key_file_set_int64 (key_file, group, "index", feed->index);

			g_free (group);
		}

		success = g_key_file_save_to_file (key_file, self->priv->filename, error);

		g_key_file_free (key_file);

		self->priv->dirty = !success;
	}

	camel_rss_store_summary_unlock (self);

	return success;
}

const gchar *
camel_rss_store_summary_add (CamelRssStoreSummary *self,
			     const gchar *href,
			     const gchar *display_name,
			     const gchar *icon_filename,
			     CamelRssContentType content_type)
{
	RssFeed *feed;
	gchar *id;
	guint index = 1;

	g_return_val_if_fail (CAMEL_IS_RSS_STORE_SUMMARY (self), NULL);
	g_return_val_if_fail (href != NULL, NULL);
	g_return_val_if_fail (display_name != NULL, NULL);

	camel_rss_store_summary_lock (self);

	self->priv->dirty = TRUE;

	id = g_compute_checksum_for_string (G_CHECKSUM_SHA1, href, -1);

	while (g_hash_table_contains (self->priv->feeds, id) && index != 0) {
		gchar *tmp;

		tmp = g_strdup_printf ("%s::%u", href, index);
		g_free (id);
		id = g_compute_checksum_for_string (G_CHECKSUM_SHA1, tmp, -1);
		g_free (tmp);
		index++;
	}

	feed = g_new0 (RssFeed, 1);
	feed->href = g_strdup (href);
	feed->display_name = g_strdup (display_name);
	feed->icon_filename = g_strdup (icon_filename);
	feed->content_type = content_type;
	feed->index = g_hash_table_size (self->priv->feeds) + 1;

	g_hash_table_insert (self->priv->feeds, id, feed);

	camel_rss_store_summary_unlock (self);
	camel_rss_store_summary_schedule_feed_changed (self, id);

	return id;
}

static void
camel_rss_store_summary_maybe_remove_filename (CamelRssStoreSummary *self,
					       const gchar *filename)
{
	if (filename && *filename) {
		gchar *prefix, *dirsep;

		prefix = g_strdup (self->priv->filename);
		dirsep = strrchr (prefix, G_DIR_SEPARATOR);

		if (dirsep) {
			dirsep[1] = '\0';

			if (g_str_has_prefix (filename, prefix) &&
			    g_unlink (filename) == -1) {
				gint errn = errno;

				if (errn != ENOENT && camel_debug ("rss"))
					g_printerr ("%s: Failed to delete '%s': %s", G_STRFUNC, filename, g_strerror (errn));
			}
		}

		g_free (prefix);
	}
}

gboolean
camel_rss_store_summary_remove (CamelRssStoreSummary *self,
				const gchar *id)
{
	RssFeed *feed;
	gboolean result = FALSE;

	g_return_val_if_fail (CAMEL_IS_RSS_STORE_SUMMARY (self), FALSE);
	g_return_val_if_fail (id != NULL, FALSE);

	camel_rss_store_summary_lock (self);

	feed = g_hash_table_lookup (self->priv->feeds, id);

	if (feed) {
		guint removed_index = feed->index;

		camel_rss_store_summary_maybe_remove_filename (self, feed->icon_filename);

		result = g_hash_table_remove (self->priv->feeds, id);

		/* Correct indexes of the left feeds */
		if (result) {
			GHashTableIter iter;
			gpointer value;

			g_hash_table_iter_init (&iter, self->priv->feeds);
			while (g_hash_table_iter_next (&iter, NULL, &value)) {
				RssFeed *feed2 = value;

				if (feed2 && feed2->index > removed_index)
					feed2->index--;
			}
		}
	}

	if (result)
		self->priv->dirty = TRUE;

	camel_rss_store_summary_unlock (self);

	if (result)
		camel_rss_store_summary_schedule_feed_changed (self, id);

	return result;
}

gboolean
camel_rss_store_summary_contains (CamelRssStoreSummary *self,
				  const gchar *id)
{
	gboolean result = FALSE;

	g_return_val_if_fail (CAMEL_IS_RSS_STORE_SUMMARY (self), FALSE);
	g_return_val_if_fail (id != NULL, FALSE);

	camel_rss_store_summary_lock (self);

	result = g_hash_table_contains (self->priv->feeds, id);

	camel_rss_store_summary_unlock (self);

	return result;
}

static gint
compare_ids_by_index (gconstpointer id1,
		      gconstpointer id2,
		      gpointer user_data)
{
	GHashTable *feeds = user_data;
	RssFeed *feed1, *feed2;

	feed1 = g_hash_table_lookup (feeds, id1);
	feed2 = g_hash_table_lookup (feeds, id2);

	if (!feed1 || !feed2)
		return 0;

	return feed1->index - feed2->index;
}

GSList * /* gchar *id */
camel_rss_store_summary_dup_feeds (CamelRssStoreSummary *self)
{
	GSList *ids = NULL;
	GHashTableIter iter;
	gpointer key;

	g_return_val_if_fail (CAMEL_IS_RSS_STORE_SUMMARY (self), NULL);

	camel_rss_store_summary_lock (self);

	g_hash_table_iter_init (&iter, self->priv->feeds);

	while (g_hash_table_iter_next (&iter, &key, NULL)) {
		ids = g_slist_prepend (ids, g_strdup (key));
	}

	ids = g_slist_sort_with_data (ids, compare_ids_by_index, self->priv->feeds);

	camel_rss_store_summary_unlock (self);

	return ids;
}

CamelFolderInfo *
camel_rss_store_summary_dup_folder_info	(CamelRssStoreSummary *self,
					 const gchar *id)
{
	RssFeed *feed;
	CamelFolderInfo *fi = NULL;

	g_return_val_if_fail (CAMEL_IS_RSS_STORE_SUMMARY (self), NULL);
	g_return_val_if_fail (id != NULL, NULL);

	camel_rss_store_summary_lock (self);

	feed = g_hash_table_lookup (self->priv->feeds, id);
	if (feed) {
		fi = camel_folder_info_new ();
		fi->full_name = g_strdup (id);
		fi->display_name = g_strdup (feed->display_name);
		fi->flags = CAMEL_FOLDER_NOCHILDREN;
		fi->unread = feed->unread_count;
		fi->total = feed->total_count;
	}

	camel_rss_store_summary_unlock (self);

	return fi;
}

CamelFolderInfo *
camel_rss_store_summary_dup_folder_info_for_display_name (CamelRssStoreSummary *self,
							  const gchar *display_name)
{
	CamelFolderInfo *fi = NULL;
	GHashTableIter iter;
	gpointer key, value;

	g_return_val_if_fail (CAMEL_IS_RSS_STORE_SUMMARY (self), NULL);
	g_return_val_if_fail (display_name != NULL, NULL);

	camel_rss_store_summary_lock (self);

	g_hash_table_iter_init (&iter, self->priv->feeds);

	while (g_hash_table_iter_next (&iter, &key, &value)) {
		const gchar *id = key;
		RssFeed *feed = value;

		if (g_strcmp0 (display_name, feed->display_name) == 0) {
			fi = camel_rss_store_summary_dup_folder_info (self, id);
			break;
		}
	}

	camel_rss_store_summary_unlock (self);

	return fi;
}

const gchar *
camel_rss_store_summary_get_href (CamelRssStoreSummary *self,
				  const gchar *id)
{
	RssFeed *feed;
	const gchar *result = NULL;

	g_return_val_if_fail (CAMEL_IS_RSS_STORE_SUMMARY (self), NULL);
	g_return_val_if_fail (id != NULL, NULL);

	camel_rss_store_summary_lock (self);

	feed = g_hash_table_lookup (self->priv->feeds, id);
	if (feed)
		result = feed->href;

	camel_rss_store_summary_unlock (self);

	return result;
}

const gchar *
camel_rss_store_summary_get_display_name (CamelRssStoreSummary *self,
					  const gchar *id)
{
	RssFeed *feed;
	const gchar *result = NULL;

	g_return_val_if_fail (CAMEL_IS_RSS_STORE_SUMMARY (self), NULL);
	g_return_val_if_fail (id != NULL, NULL);

	camel_rss_store_summary_lock (self);

	feed = g_hash_table_lookup (self->priv->feeds, id);
	if (feed)
		result = feed->display_name;

	camel_rss_store_summary_unlock (self);

	return result;
}

void
camel_rss_store_summary_set_display_name (CamelRssStoreSummary *self,
					  const gchar *id,
					  const gchar *display_name)
{
	RssFeed *feed;
	gboolean changed = FALSE;

	g_return_if_fail (CAMEL_IS_RSS_STORE_SUMMARY (self));
	g_return_if_fail (id != NULL);
	g_return_if_fail (display_name && *display_name);

	camel_rss_store_summary_lock (self);

	feed = g_hash_table_lookup (self->priv->feeds, id);
	if (feed) {
		if (g_strcmp0 (feed->display_name, display_name) != 0) {
			g_free (feed->display_name);
			feed->display_name = g_strdup (display_name);
			self->priv->dirty = TRUE;
			changed = TRUE;
		}
	}

	camel_rss_store_summary_unlock (self);

	if (changed)
		camel_rss_store_summary_schedule_feed_changed (self, id);
}

const gchar *
camel_rss_store_summary_get_icon_filename (CamelRssStoreSummary *self,
					   const gchar *id)
{
	RssFeed *feed;
	const gchar *result = NULL;

	g_return_val_if_fail (CAMEL_IS_RSS_STORE_SUMMARY (self), NULL);
	g_return_val_if_fail (id != NULL, NULL);

	camel_rss_store_summary_lock (self);

	feed = g_hash_table_lookup (self->priv->feeds, id);
	if (feed)
		result = feed->icon_filename;

	camel_rss_store_summary_unlock (self);

	return result;
}

void
camel_rss_store_summary_set_icon_filename (CamelRssStoreSummary *self,
					   const gchar *id,
					   const gchar *icon_filename)
{
	RssFeed *feed;
	gboolean changed = FALSE;

	g_return_if_fail (CAMEL_IS_RSS_STORE_SUMMARY (self));
	g_return_if_fail (id != NULL);

	camel_rss_store_summary_lock (self);

	feed = g_hash_table_lookup (self->priv->feeds, id);
	if (feed) {
		if (g_strcmp0 (feed->icon_filename, icon_filename) != 0) {
			camel_rss_store_summary_maybe_remove_filename (self, feed->icon_filename);
			g_free (feed->icon_filename);
			feed->icon_filename = g_strdup (icon_filename);
			self->priv->dirty = TRUE;
			changed = TRUE;
		}
	}

	camel_rss_store_summary_unlock (self);

	if (changed)
		camel_rss_store_summary_schedule_feed_changed (self, id);
}

CamelRssContentType
camel_rss_store_summary_get_content_type (CamelRssStoreSummary *self,
					  const gchar *id)
{
	RssFeed *feed;
	CamelRssContentType result = CAMEL_RSS_CONTENT_TYPE_HTML;

	g_return_val_if_fail (CAMEL_IS_RSS_STORE_SUMMARY (self), result);
	g_return_val_if_fail (id != NULL, result);

	camel_rss_store_summary_lock (self);

	feed = g_hash_table_lookup (self->priv->feeds, id);
	if (feed)
		result = feed->content_type;

	camel_rss_store_summary_unlock (self);

	return result;
}

void
camel_rss_store_summary_set_content_type (CamelRssStoreSummary *self,
					  const gchar *id,
					  CamelRssContentType content_type)
{
	RssFeed *feed;
	gboolean changed = FALSE;

	g_return_if_fail (CAMEL_IS_RSS_STORE_SUMMARY (self));
	g_return_if_fail (id != NULL);

	camel_rss_store_summary_lock (self);

	feed = g_hash_table_lookup (self->priv->feeds, id);
	if (feed) {
		if (feed->content_type != content_type) {
			feed->content_type = content_type;
			self->priv->dirty = TRUE;
			changed = TRUE;
		}
	}

	camel_rss_store_summary_unlock (self);

	if (changed)
		camel_rss_store_summary_schedule_feed_changed (self, id);
}

guint32
camel_rss_store_summary_get_total_count (CamelRssStoreSummary *self,
					 const gchar *id)
{
	RssFeed *feed;
	guint32 result = 0;

	g_return_val_if_fail (CAMEL_IS_RSS_STORE_SUMMARY (self), 0);
	g_return_val_if_fail (id != NULL, 0);

	camel_rss_store_summary_lock (self);

	feed = g_hash_table_lookup (self->priv->feeds, id);
	if (feed)
		result = feed->total_count;

	camel_rss_store_summary_unlock (self);

	return result;
}

void
camel_rss_store_summary_set_total_count (CamelRssStoreSummary *self,
					 const gchar *id,
					 guint32 total_count)
{
	RssFeed *feed;

	g_return_if_fail (CAMEL_IS_RSS_STORE_SUMMARY (self));
	g_return_if_fail (id != NULL);

	camel_rss_store_summary_lock (self);

	feed = g_hash_table_lookup (self->priv->feeds, id);
	if (feed) {
		if (feed->total_count != total_count) {
			feed->total_count = total_count;
			self->priv->dirty = TRUE;
		}
	}

	camel_rss_store_summary_unlock (self);
}

guint32
camel_rss_store_summary_get_unread_count (CamelRssStoreSummary *self,
					  const gchar *id)
{
	RssFeed *feed;
	guint32 result = 0;

	g_return_val_if_fail (CAMEL_IS_RSS_STORE_SUMMARY (self), 0);
	g_return_val_if_fail (id != NULL, 0);

	camel_rss_store_summary_lock (self);

	feed = g_hash_table_lookup (self->priv->feeds, id);
	if (feed)
		result = feed->unread_count;

	camel_rss_store_summary_unlock (self);

	return result;
}

void
camel_rss_store_summary_set_unread_count (CamelRssStoreSummary *self,
					  const gchar *id,
					  guint32 unread_count)
{
	RssFeed *feed;

	g_return_if_fail (CAMEL_IS_RSS_STORE_SUMMARY (self));
	g_return_if_fail (id != NULL);

	camel_rss_store_summary_lock (self);

	feed = g_hash_table_lookup (self->priv->feeds, id);
	if (feed) {
		if (feed->unread_count != unread_count) {
			feed->unread_count = unread_count;
			self->priv->dirty = TRUE;
		}
	}

	camel_rss_store_summary_unlock (self);
}

gint64
camel_rss_store_summary_get_last_updated (CamelRssStoreSummary *self,
					  const gchar *id)
{
	RssFeed *feed;
	gint64 result = 0;

	g_return_val_if_fail (CAMEL_IS_RSS_STORE_SUMMARY (self), 0);
	g_return_val_if_fail (id != NULL, 0);

	camel_rss_store_summary_lock (self);

	feed = g_hash_table_lookup (self->priv->feeds, id);
	if (feed)
		result = feed->last_updated;

	camel_rss_store_summary_unlock (self);

	return result;
}

void
camel_rss_store_summary_set_last_updated (CamelRssStoreSummary *self,
					  const gchar *id,
					  gint64 last_updated)
{
	RssFeed *feed;

	g_return_if_fail (CAMEL_IS_RSS_STORE_SUMMARY (self));
	g_return_if_fail (id != NULL);

	camel_rss_store_summary_lock (self);

	feed = g_hash_table_lookup (self->priv->feeds, id);
	if (feed) {
		if (feed->last_updated != last_updated) {
			feed->last_updated = last_updated;
			self->priv->dirty = TRUE;
		}
	}

	camel_rss_store_summary_unlock (self);
}

const gchar *
camel_rss_store_summary_get_last_etag (CamelRssStoreSummary *self,
				       const gchar *id)
{
	RssFeed *feed;
	const gchar *result = NULL;

	g_return_val_if_fail (CAMEL_IS_RSS_STORE_SUMMARY (self), NULL);
	g_return_val_if_fail (id != NULL, NULL);

	camel_rss_store_summary_lock (self);

	feed = g_hash_table_lookup (self->priv->feeds, id);
	if (feed)
		result = feed->last_etag;

	camel_rss_store_summary_unlock (self);

	return result;
}

void
camel_rss_store_summary_set_last_etag (CamelRssStoreSummary *self,
				       const gchar *id,
				       const gchar *last_etag)
{
	RssFeed *feed;
	gboolean changed = FALSE;

	g_return_if_fail (CAMEL_IS_RSS_STORE_SUMMARY (self));
	g_return_if_fail (id != NULL);

	camel_rss_store_summary_lock (self);

	feed = g_hash_table_lookup (self->priv->feeds, id);
	if (feed) {
		if (g_strcmp0 (feed->last_etag, last_etag) != 0) {
			g_free (feed->last_etag);
			feed->last_etag = g_strdup (last_etag);
			self->priv->dirty = TRUE;
			changed = TRUE;
		}
	}

	camel_rss_store_summary_unlock (self);

	if (changed)
		camel_rss_store_summary_schedule_feed_changed (self, id);
}

const gchar *
camel_rss_store_summary_get_last_modified (CamelRssStoreSummary *self,
					   const gchar *id)
{
	RssFeed *feed;
	const gchar *result = NULL;

	g_return_val_if_fail (CAMEL_IS_RSS_STORE_SUMMARY (self), NULL);
	g_return_val_if_fail (id != NULL, NULL);

	camel_rss_store_summary_lock (self);

	feed = g_hash_table_lookup (self->priv->feeds, id);
	if (feed)
		result = feed->last_modified;

	camel_rss_store_summary_unlock (self);

	return result;
}

void
camel_rss_store_summary_set_last_modified (CamelRssStoreSummary *self,
					   const gchar *id,
					   const gchar *last_modified)
{
	RssFeed *feed;
	gboolean changed = FALSE;

	g_return_if_fail (CAMEL_IS_RSS_STORE_SUMMARY (self));
	g_return_if_fail (id != NULL);

	camel_rss_store_summary_lock (self);

	feed = g_hash_table_lookup (self->priv->feeds, id);
	if (feed) {
		if (g_strcmp0 (feed->last_modified, last_modified) != 0) {
			g_free (feed->last_modified);
			feed->last_modified = g_strdup (last_modified);
			self->priv->dirty = TRUE;
			changed = TRUE;
		}
	}

	camel_rss_store_summary_unlock (self);

	if (changed)
		camel_rss_store_summary_schedule_feed_changed (self, id);
}
