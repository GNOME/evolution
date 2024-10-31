/*
 * SPDX-FileCopyrightText: (C) 2022 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <glib-object.h>

#include "mail/em-folder-tree-model.h"
#include "../camel-rss-store-summary.h"

#include "module-rss.h"

#define E_TYPE_RSS_FOLDER_TREE_MODEL_EXTENSION (e_rss_folder_tree_model_extension_get_type ())

GType e_rss_folder_tree_model_extension_get_type (void);

typedef struct _ERssFolderTreeModelExtension {
	EExtension parent;
	gboolean listens_feed_changed;
} ERssFolderTreeModelExtension;

typedef struct _ERssFolderTreeModelExtensionClass {
	EExtensionClass parent_class;
} ERssFolderTreeModelExtensionClass;

G_DEFINE_DYNAMIC_TYPE (ERssFolderTreeModelExtension, e_rss_folder_tree_model_extension, E_TYPE_EXTENSION)

static void
e_rss_update_custom_icon (CamelRssStoreSummary *store_summary,
			  const gchar *full_name,
			  EMFolderTreeModel *model,
			  GtkTreeIter *iter)
{
	const gchar *icon_filename;

	icon_filename = camel_rss_store_summary_get_icon_filename (store_summary, full_name);

	if (icon_filename && g_file_test (icon_filename, G_FILE_TEST_IS_REGULAR | G_FILE_TEST_EXISTS))
		icon_filename = full_name;
	else
		icon_filename = "rss";

	gtk_tree_store_set (GTK_TREE_STORE (model), iter,
		COL_STRING_ICON_NAME, icon_filename,
		-1);
}

static void
e_rss_folder_custom_icon_feed_changed_cb (CamelRssStoreSummary *store_summary,
					  const gchar *feed_id,
					  EMFolderTreeModel *model)
{
	EMailSession *session;
	CamelService *service;

	if (!feed_id || !camel_rss_store_summary_contains (store_summary, feed_id))
		return;

	session = em_folder_tree_model_get_session (model);

	if (!session)
		return;

	service = camel_session_ref_service (CAMEL_SESSION (session), "rss");

	if (service) {
		GtkTreeRowReference *row;

		row = em_folder_tree_model_get_row_reference (model, CAMEL_STORE (service), feed_id);
		if (row) {
			GtkTreePath *path;
			GtkTreeIter iter;

			path = gtk_tree_row_reference_get_path (row);
			gtk_tree_model_get_iter (GTK_TREE_MODEL (model), &iter, path);
			gtk_tree_path_free (path);

			e_rss_update_custom_icon (store_summary, feed_id, model, &iter);
		}
	}

	g_clear_object (&service);
}

static void
e_rss_folder_custom_icon_cb (EMFolderTreeModel *model,
			     GtkTreeIter *iter,
			     CamelStore *store,
			     const gchar *full_name,
			     ERssFolderTreeModelExtension *extension)
{
	CamelRssStoreSummary *store_summary = NULL;
	const gchar *uid = camel_service_get_uid (CAMEL_SERVICE (store));

	g_return_if_fail (extension != NULL);

	if (g_strcmp0 (uid, "rss") != 0 || !full_name)
		return;

	if (g_strcmp0 (full_name, CAMEL_VJUNK_NAME) == 0 ||
	    g_strcmp0 (full_name, CAMEL_VTRASH_NAME) == 0)
		return;

	g_object_get (store, "summary", &store_summary, NULL);

	if (!store_summary)
		return;

	if (!extension->listens_feed_changed) {
		extension->listens_feed_changed = TRUE;

		g_signal_connect_object (store_summary, "feed-changed",
			G_CALLBACK (e_rss_folder_custom_icon_feed_changed_cb), model, 0);
	}

	e_rss_update_custom_icon (store_summary, full_name, model, iter);

	g_clear_object (&store_summary);
}

static gint
e_rss_compare_folders_cb (EMFolderTreeModel *model,
			  const gchar *store_uid,
			  GtkTreeIter *iter1,
			  GtkTreeIter *iter2)
{
	gint rv = -2;

	/* Junk/Trash as the last, to not mix them with feed folders */
	if (g_strcmp0 (store_uid, "rss") == 0) {
		gboolean a_is_vfolder, b_is_vfolder;
		guint32 flags_a, flags_b;

		gtk_tree_model_get (
			GTK_TREE_MODEL (model), iter1,
			COL_UINT_FLAGS, &flags_a,
			-1);

		gtk_tree_model_get (
			GTK_TREE_MODEL (model), iter2,
			COL_UINT_FLAGS, &flags_b,
			-1);

		a_is_vfolder = (flags_a & CAMEL_FOLDER_TYPE_MASK) == CAMEL_FOLDER_TYPE_JUNK ||
			       (flags_a & CAMEL_FOLDER_TYPE_MASK) == CAMEL_FOLDER_TYPE_TRASH;
		b_is_vfolder = (flags_b & CAMEL_FOLDER_TYPE_MASK) == CAMEL_FOLDER_TYPE_JUNK ||
			       (flags_b & CAMEL_FOLDER_TYPE_MASK) == CAMEL_FOLDER_TYPE_TRASH;

		if ((a_is_vfolder || b_is_vfolder) && (!a_is_vfolder || !b_is_vfolder)) {
			if (a_is_vfolder)
				rv = 1;
			else
				rv = -1;
		}
	}

	return rv;
}

static void
e_rss_folder_tree_model_extension_constructed (GObject *object)
{
	static gboolean icon_dir_registered = FALSE;

	/* Chain up to parent's method */
	G_OBJECT_CLASS (e_rss_folder_tree_model_extension_parent_class)->constructed (object);

	g_signal_connect_object (e_extension_get_extensible (E_EXTENSION (object)), "folder-custom-icon",
		G_CALLBACK (e_rss_folder_custom_icon_cb), object, 0);

	g_signal_connect_object (e_extension_get_extensible (E_EXTENSION (object)), "compare-folders",
		G_CALLBACK (e_rss_compare_folders_cb), NULL, 0);

	if (!icon_dir_registered) {
		gchar *icon_dir;

		icon_dir_registered = TRUE;

		icon_dir = g_build_filename (e_get_user_data_dir (), "mail", "rss", NULL);

		gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (), icon_dir);

		g_free (icon_dir);
	}
}

static void
e_rss_folder_tree_model_extension_class_init (ERssFolderTreeModelExtensionClass *klass)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->constructed = e_rss_folder_tree_model_extension_constructed;

	extension_class = E_EXTENSION_CLASS (klass);
	extension_class->extensible_type = EM_TYPE_FOLDER_TREE_MODEL;
}

static void
e_rss_folder_tree_model_extension_class_finalize (ERssFolderTreeModelExtensionClass *klass)
{
}

static void
e_rss_folder_tree_model_extension_init (ERssFolderTreeModelExtension *extension)
{
}

void
e_rss_folder_tree_model_extension_type_register (GTypeModule *type_module)
{
	e_rss_folder_tree_model_extension_register_type (type_module);
}
