/*
 * SPDX-FileCopyrightText: (C) 2022 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>
#include <libedataserver/libedataserver.h>

#include "camel-rss-folder.h"
#include "camel-rss-settings.h"
#include "camel-rss-store-summary.h"
#include "camel-rss-store.h"

struct _CamelRssStorePrivate {
	CamelDataCache *cache;
	CamelRssStoreSummary *summary;
};

enum {
	PROP_0,
	PROP_SUMMARY
};

static GInitableIface *parent_initable_interface;

/* Forward Declarations */
static void camel_rss_store_initable_init (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (CamelRssStore, camel_rss_store, CAMEL_TYPE_STORE,
	G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, camel_rss_store_initable_init)
	G_ADD_PRIVATE (CamelRssStore))

static gchar *
rss_store_get_name (CamelService *service,
		    gboolean brief)
{
	return g_strdup (_("News and Blogs"));
}

static gboolean
rss_store_can_refresh_folder (CamelStore *store,
			      CamelFolderInfo *info,
			      GError **error)
{
	/* Any RSS folder can be refreshed */
	return TRUE;
}

static CamelFolder *
rss_store_get_folder_sync (CamelStore *store,
			   const gchar *folder_name,
			   CamelStoreGetFolderFlags flags,
			   GCancellable *cancellable,
			   GError **error)
{
	CamelRssStore *self = CAMEL_RSS_STORE (store);
	CamelFolder *folder = NULL;

	camel_rss_store_summary_lock (self->priv->summary);

	/* The 'folder_name' is the folder ID */
	if (camel_rss_store_summary_contains (self->priv->summary, folder_name)) {
		folder = camel_rss_folder_new (store, folder_name, cancellable, error);
	} else {
		g_set_error (error, CAMEL_STORE_ERROR, CAMEL_STORE_ERROR_NO_FOLDER,
			_("Folder '%s' not found"), folder_name);
	}

	camel_rss_store_summary_unlock (self->priv->summary);

	return folder;
}

static CamelFolderInfo *
rss_store_get_folder_info_sync (CamelStore *store,
				const gchar *top,
				CamelStoreGetFolderInfoFlags flags,
				GCancellable *cancellable,
				GError **error)
{
	CamelRssStore *self = CAMEL_RSS_STORE (store);
	CamelFolderInfo *fi = NULL, *first = NULL, *last = NULL;

	if (!top || !*top) {
		GSList *ids, *link;

		ids = camel_rss_store_summary_dup_feeds (self->priv->summary);

		for (link = ids; link; link = g_slist_next (link)) {
			const gchar *id = link->data;

			fi = camel_rss_store_summary_dup_folder_info (self->priv->summary, id);
			if (fi) {
				if (last) {
					last->next = fi;
					last = fi;
				} else {
					first = fi;
					last = first;
				}
			}
		}

		g_slist_free_full (ids, g_free);
	} else {
		first = camel_rss_store_summary_dup_folder_info (self->priv->summary, top);
		if (!first)
			first = camel_rss_store_summary_dup_folder_info_for_display_name (self->priv->summary, top);

		if (!first) {
			g_set_error (error, CAMEL_STORE_ERROR, CAMEL_STORE_ERROR_NO_FOLDER,
				_("Folder '%s' not found"), top);
		}
	}

	return first;
}

static CamelFolderInfo *
rss_store_create_folder_sync (CamelStore *store,
			      const gchar *parent_name,
			      const gchar *folder_name,
			      GCancellable *cancellable,
			      GError **error)
{
	g_set_error (error, CAMEL_STORE_ERROR, CAMEL_STORE_ERROR_INVALID,
		_("Cannot create a folder in a News and Blogs store."));

	return NULL;
}

static gboolean
rss_store_rename_folder_sync (CamelStore *store,
			      const gchar *old_name,
			      const gchar *new_name_in,
			      GCancellable *cancellable,
			      GError **error)
{
	CamelRssStore *self = CAMEL_RSS_STORE (store);
	gboolean success = FALSE;

	camel_rss_store_summary_lock (self->priv->summary);

	if (camel_rss_store_summary_contains (self->priv->summary, old_name)) {
		const gchar *display_name;

		success = TRUE;

		display_name = camel_rss_store_summary_get_display_name (self->priv->summary, old_name);
		if (g_strcmp0 (display_name, new_name_in) != 0) {
			camel_rss_store_summary_set_display_name (self->priv->summary, old_name, new_name_in);

			success = camel_rss_store_summary_save (self->priv->summary, error);

			if (success) {
				CamelFolderInfo *fi;

				fi = camel_rss_store_summary_dup_folder_info (self->priv->summary, old_name);
				camel_store_folder_renamed (store, old_name, fi);
				camel_folder_info_free (fi);
			}
		}
	} else {
		g_set_error (error, CAMEL_STORE_ERROR, CAMEL_STORE_ERROR_NO_FOLDER,
			_("Folder '%s' not found"), old_name);
	}

	camel_rss_store_summary_unlock (self->priv->summary);

	return success;
}

static gboolean
rss_store_delete_folder_sync (CamelStore *store,
			      const gchar *folder_name,
			      GCancellable *cancellable,
			      GError **error)
{
	CamelRssStore *self = CAMEL_RSS_STORE (store);
	CamelFolderInfo *fi;
	gboolean success = FALSE;

	camel_rss_store_summary_lock (self->priv->summary);

	/* The 'folder_name' is the folder ID */
	fi = camel_rss_store_summary_dup_folder_info (self->priv->summary, folder_name);

	if (camel_rss_store_summary_remove (self->priv->summary, folder_name)) {
		GFile *file;
		gchar *cmeta_filename;
		GError *local_error = NULL;

		file = g_file_new_build_filename (camel_data_cache_get_path (self->priv->cache), folder_name, NULL);

		/* Ignore errors */
		if (!e_file_recursive_delete_sync (file, cancellable, &local_error)) {
			if (camel_debug ("rss") &&
			    !g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND) &&
			    !g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
				g_printerr ("%s: Failed to delete cache directory '%s': %s", G_STRFUNC, g_file_peek_path (file), local_error ? local_error->message : "Unknown error");

			g_clear_error (&local_error);
		}

		g_clear_object (&file);

		cmeta_filename = g_strdup_printf ("%s%c%s.cmeta", camel_data_cache_get_path (self->priv->cache), G_DIR_SEPARATOR, folder_name);
		if (g_unlink (cmeta_filename)) {
			gint errn = errno;

			if (errn != ENOENT && camel_debug ("rss"))
				g_printerr ("%s: Failed to delete '%s': %s", G_STRFUNC, cmeta_filename, g_strerror (errn));
		}

		g_free (cmeta_filename);

		camel_store_folder_deleted (store, fi);
		success = camel_rss_store_summary_save (self->priv->summary, error);
	} else {
		g_set_error (error, CAMEL_STORE_ERROR, CAMEL_STORE_ERROR_NO_FOLDER,
			_("Folder '%s' not found"), folder_name);
	}

	camel_rss_store_summary_unlock (self->priv->summary);

	if (fi)
		camel_folder_info_free (fi);

	return success;
}

static void
rss_store_get_property (GObject *object,
			guint property_id,
			GValue *value,
			GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SUMMARY:
			g_value_set_object (value,
				camel_rss_store_get_summary (
				CAMEL_RSS_STORE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
rss_store_dispose (GObject *object)
{
	CamelRssStore *self = CAMEL_RSS_STORE (object);

	if (self->priv->summary) {
		GError *local_error = NULL;

		if (!camel_rss_store_summary_save (self->priv->summary, &local_error)) {
			g_warning ("%s: Failed to save RSS store summary: %s", G_STRFUNC, local_error ? local_error->message : "Unknown error");
			g_clear_error (&local_error);
		}
	}

	g_clear_object (&self->priv->cache);
	g_clear_object (&self->priv->summary);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (camel_rss_store_parent_class)->dispose (object);
}

static void
camel_rss_store_class_init (CamelRssStoreClass *klass)
{
	GObjectClass *object_class;
	CamelServiceClass *service_class;
	CamelStoreClass *store_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->get_property = rss_store_get_property;
	object_class->dispose = rss_store_dispose;

	service_class = CAMEL_SERVICE_CLASS (klass);
	service_class->settings_type = CAMEL_TYPE_RSS_SETTINGS;
	service_class->get_name = rss_store_get_name;

	store_class = CAMEL_STORE_CLASS (klass);
	store_class->can_refresh_folder = rss_store_can_refresh_folder;
	store_class->get_folder_sync = rss_store_get_folder_sync;
	store_class->get_folder_info_sync = rss_store_get_folder_info_sync;
	store_class->create_folder_sync = rss_store_create_folder_sync;
	store_class->delete_folder_sync = rss_store_delete_folder_sync;
	store_class->rename_folder_sync = rss_store_rename_folder_sync;

	g_object_class_install_property (
		object_class,
		PROP_SUMMARY,
		g_param_spec_object (
			"summary", NULL, NULL,
			CAMEL_TYPE_RSS_STORE_SUMMARY,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));
}

static gboolean
rss_store_initable_init (GInitable *initable,
			 GCancellable *cancellable,
			 GError **error)
{
	CamelDataCache *rss_cache;
	CamelRssStore *self;
	CamelStore *store;
	CamelService *service;
	const gchar *user_data_dir;
	gchar *filename;

	self = CAMEL_RSS_STORE (initable);
	store = CAMEL_STORE (initable);

	camel_store_set_flags (store, camel_store_get_flags (store) | CAMEL_STORE_VTRASH | CAMEL_STORE_VJUNK | CAMEL_STORE_IS_BUILTIN);

	/* Chain up to parent method. */
	if (!parent_initable_interface->init (initable, cancellable, error))
		return FALSE;

	service = CAMEL_SERVICE (initable);
	user_data_dir = camel_service_get_user_data_dir (service);

	if (g_mkdir_with_parents (user_data_dir, S_IRWXU) == -1) {
		g_set_error_literal (
			error, G_FILE_ERROR,
			g_file_error_from_errno (errno),
			g_strerror (errno));
		return FALSE;
	}

	filename = g_build_filename (user_data_dir, "rss.ini", NULL);
	self->priv->summary = camel_rss_store_summary_new (filename);
	g_free (filename);

	if (!camel_rss_store_summary_load (self->priv->summary, error))
		return FALSE;

	/* setup store-wide cache */
	rss_cache = camel_data_cache_new (user_data_dir, error);
	if (rss_cache == NULL)
		return FALSE;

	/* Do not expire the cache */
	camel_data_cache_set_expire_enabled (rss_cache, FALSE);

	self->priv->cache = rss_cache;  /* takes ownership */

	return TRUE;
}

static void
camel_rss_store_initable_init (GInitableIface *iface)
{
	parent_initable_interface = g_type_interface_peek_parent (iface);

	iface->init = rss_store_initable_init;
}

static void
camel_rss_store_init (CamelRssStore *self)
{
	self->priv = camel_rss_store_get_instance_private (self);

	camel_store_set_flags (CAMEL_STORE (self), 0);
}

CamelDataCache *
camel_rss_store_get_cache (CamelRssStore *self)
{
	g_return_val_if_fail (CAMEL_IS_RSS_STORE (self), NULL);

	return self->priv->cache;
}

CamelRssStoreSummary *
camel_rss_store_get_summary (CamelRssStore *self)
{
	g_return_val_if_fail (CAMEL_IS_RSS_STORE (self), NULL);

	return self->priv->summary;
}
