/*
 * e-picture-gallery.c
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

#include "e-picture-gallery.h"

#include "e-icon-factory.h"
#include "e-misc-utils.h"

struct _EPictureGalleryPrivate {
	gboolean initialized;
	gchar *path;
	GFileMonitor *monitor;
};

enum {
	PROP_0,
	PROP_PATH
};

enum {
	COL_PIXBUF = 0,
	COL_URI,
	COL_FILENAME_TEXT
};

G_DEFINE_TYPE_WITH_PRIVATE (EPictureGallery, e_picture_gallery, GTK_TYPE_ICON_VIEW)

static gboolean
update_file_iter (GtkListStore *list_store,
                  GtkTreeIter *iter,
                  GFile *file,
                  gboolean force_thumbnail_update)
{
	GFileInfo *file_info;
	gchar *uri;
	gboolean res = FALSE;

	g_return_val_if_fail (list_store != NULL, FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);
	g_return_val_if_fail (file != NULL, FALSE);

	uri = g_file_get_uri (file);

	file_info = g_file_query_info (
		file,
		G_FILE_ATTRIBUTE_THUMBNAIL_PATH ","
		G_FILE_ATTRIBUTE_THUMBNAILING_FAILED ","
		G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME ","
		G_FILE_ATTRIBUTE_STANDARD_SIZE,
		G_FILE_QUERY_INFO_NONE,
		NULL,
		NULL);

	if (file_info != NULL) {
		const gchar *existing_thumb = g_file_info_get_attribute_byte_string (file_info, G_FILE_ATTRIBUTE_THUMBNAIL_PATH);
		gchar *new_thumb = NULL;

		if (!existing_thumb || force_thumbnail_update) {
			gchar *filename;

			filename = g_file_get_path (file);
			if (filename) {
				new_thumb = e_icon_factory_create_thumbnail (filename);
				if (new_thumb)
					existing_thumb = new_thumb;
				g_free (filename);
			}
		}

		if (existing_thumb && !g_file_info_get_attribute_boolean (file_info, G_FILE_ATTRIBUTE_THUMBNAILING_FAILED)) {
			GdkPixbuf * pixbuf;

			pixbuf = gdk_pixbuf_new_from_file (existing_thumb, NULL);

			if (pixbuf) {
				const gchar *filename;
				gchar *filename_text = NULL;
				guint64 filesize;

				filename = g_file_info_get_attribute_string (file_info, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME);
				if (filename) {
					filesize = g_file_info_get_attribute_uint64 (file_info, G_FILE_ATTRIBUTE_STANDARD_SIZE);
					if (filesize) {
						gchar *tmp = g_format_size ((goffset) filesize);
						filename_text = g_strdup_printf ("%s (%s)", filename, tmp);
						g_free (tmp);
					}

					res = TRUE;
					gtk_list_store_set (
						list_store, iter,
						COL_PIXBUF, pixbuf,
						COL_URI, uri,
						COL_FILENAME_TEXT, filename_text ? filename_text : filename,
						-1);
				}

				g_object_unref (pixbuf);
				g_free (filename_text);
			}
		}

		g_free (new_thumb);
	}

	g_free (uri);

	return res;
}

static void
add_file (GtkListStore *list_store,
          GFile *file)
{
	GtkTreeIter iter;

	g_return_if_fail (list_store != NULL);
	g_return_if_fail (file != NULL);

	gtk_list_store_append (list_store, &iter);
	if (!update_file_iter (list_store, &iter, file, FALSE))
		gtk_list_store_remove (list_store, &iter);
}

static gboolean
find_file_uri (GtkListStore *list_store,
               const gchar *uri,
               GtkTreeIter *iter)
{
	GtkTreeModel *model;

	g_return_val_if_fail (list_store != NULL, FALSE);
	g_return_val_if_fail (uri != NULL, FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);

	model = GTK_TREE_MODEL (list_store);
	g_return_val_if_fail (model != NULL, FALSE);

	if (!gtk_tree_model_get_iter_first (model, iter))
		return FALSE;

	do {
		gchar *iter_uri = NULL;

		gtk_tree_model_get (
			model, iter,
			COL_URI, &iter_uri,
			-1);

		if (iter_uri && g_ascii_strcasecmp (uri, iter_uri) == 0) {
			g_free (iter_uri);
			return TRUE;
		}

		g_free (iter_uri);
	} while (gtk_tree_model_iter_next (model, iter));

	return FALSE;
}

static void
picture_gallery_dir_changed_cb (GFileMonitor *monitor,
                                GFile *file,
                                GFile *other_file,
                                GFileMonitorEvent event_type,
                                EPictureGallery *gallery)
{
	gchar *uri;
	GtkListStore *list_store;
	GtkTreeIter iter;

	g_return_if_fail (file != NULL);

	list_store = GTK_LIST_STORE (gtk_icon_view_get_model (GTK_ICON_VIEW (gallery)));
	g_return_if_fail (list_store != NULL);

	uri = g_file_get_uri (file);
	if (!uri)
		return;

	switch (event_type) {
	case G_FILE_MONITOR_EVENT_CREATED:
		if (find_file_uri (list_store, uri, &iter)) {
			if (!update_file_iter (list_store, &iter, file, TRUE))
				gtk_list_store_remove (list_store, &iter);
		} else {
			add_file (list_store, file);
		}
		break;
	case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
		if (find_file_uri (list_store, uri, &iter)) {
			if (!update_file_iter (list_store, &iter, file, TRUE))
				gtk_list_store_remove (list_store, &iter);
		}
		break;
	case G_FILE_MONITOR_EVENT_DELETED:
		if (find_file_uri (list_store, uri, &iter))
			gtk_list_store_remove (list_store, &iter);
		break;
	default:
		break;
	}

	g_free (uri);
}

static gboolean
picture_gallery_start_loading_cb (EPictureGallery *gallery)
{
	GtkIconView *icon_view;
	GtkListStore *list_store;
	GDir *dir;
	const gchar *dirname;

	icon_view = GTK_ICON_VIEW (gallery);
	list_store = GTK_LIST_STORE (gtk_icon_view_get_model (icon_view));
	g_return_val_if_fail (list_store != NULL, FALSE);

	dirname = e_picture_gallery_get_path (gallery);
	if (!dirname)
		return FALSE;

	dir = g_dir_open (dirname, 0, NULL);
	if (dir) {
		GFile *file;
		const gchar *basename;

		while ((basename = g_dir_read_name (dir)) != NULL) {
			gchar *filename;

			filename = g_build_filename (dirname, basename, NULL);
			file = g_file_new_for_path (filename);

			add_file (list_store, file);

			g_free (filename);
			g_object_unref (file);
		}

		g_dir_close (dir);

		file = g_file_new_for_path (dirname);
		gallery->priv->monitor = g_file_monitor_directory (file, G_FILE_MONITOR_NONE, NULL, NULL);
		g_object_unref (file);

		if (gallery->priv->monitor)
			g_signal_connect (
				gallery->priv->monitor, "changed",
				G_CALLBACK (picture_gallery_dir_changed_cb),
				gallery);
	}

	g_object_unref (icon_view);

	return FALSE;
}

const gchar *
e_picture_gallery_get_path (EPictureGallery *gallery)
{
	g_return_val_if_fail (gallery != NULL, NULL);
	g_return_val_if_fail (E_IS_PICTURE_GALLERY (gallery), NULL);
	g_return_val_if_fail (gallery->priv != NULL, NULL);

	return gallery->priv->path;
}

static void
picture_gallery_set_path (EPictureGallery *gallery,
                          const gchar *path)
{
	g_return_if_fail (E_IS_PICTURE_GALLERY (gallery));
	g_return_if_fail (gallery->priv != NULL);

	g_free (gallery->priv->path);

	if (!path || !*path || !g_file_test (path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))
		gallery->priv->path = g_strdup (g_get_user_special_dir (G_USER_DIRECTORY_PICTURES));
	else
		gallery->priv->path = g_strdup (path);
}

static void
picture_gallery_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
	case PROP_PATH:
		g_value_set_string (value, e_picture_gallery_get_path (E_PICTURE_GALLERY (object)));
		return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
picture_gallery_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
	case PROP_PATH:
		picture_gallery_set_path (E_PICTURE_GALLERY (object), g_value_get_string (value));
		return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
visible_cb (EPictureGallery *gallery)
{
	if (!gallery->priv->initialized && gtk_widget_get_visible (GTK_WIDGET (gallery))) {
		gallery->priv->initialized = TRUE;

		g_idle_add ((GSourceFunc) picture_gallery_start_loading_cb, gallery);
	}
}

static void
picture_gallery_constructed (GObject *object)
{
	GtkIconView *icon_view;
	GtkCellLayout *cell_layout;
	GtkCellRenderer *renderer;
	GtkListStore *list_store;
	GtkTargetEntry *targets;
	GtkTargetList *list;
	gint n_targets;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_picture_gallery_parent_class)->constructed (object);

	icon_view = GTK_ICON_VIEW (object);

	list_store = gtk_list_store_new (3, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (list_store), COL_FILENAME_TEXT, GTK_SORT_ASCENDING);
	gtk_icon_view_set_model (icon_view, GTK_TREE_MODEL (list_store));
	g_object_unref (list_store);

	gtk_icon_view_set_item_width (icon_view, 96);

	cell_layout = GTK_CELL_LAYOUT (icon_view);

	/* This needs to happen after constructor properties are set
	 * so that GtkCellLayout.get_area() returns something valid. */

	renderer = gtk_cell_renderer_pixbuf_new ();
	g_object_set (renderer,
		"stock-size", GTK_ICON_SIZE_DIALOG,
		"xalign", 0.5,
		"yalign", 0.5,
		NULL);
	gtk_cell_layout_pack_start (cell_layout, renderer, FALSE);

	gtk_cell_layout_add_attribute (cell_layout, renderer, "pixbuf", COL_PIXBUF);

	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer,
		"alignment", PANGO_ALIGN_LEFT,
		"ellipsize", PANGO_ELLIPSIZE_END,
		"wrap-mode", PANGO_WRAP_WORD,
		"wrap-width", 96,
		"scale", 0.8,
		"xpad", 0,
		"xalign", 0.5,
		"yalign", 0.0,
		NULL);
	gtk_cell_layout_pack_start (cell_layout, renderer, FALSE);

	gtk_cell_layout_add_attribute (cell_layout, renderer, "text", COL_FILENAME_TEXT);

	list = gtk_target_list_new (NULL, 0);
	gtk_target_list_add_uri_targets (list, 0);
	targets = gtk_target_table_new_from_list (list, &n_targets);

	gtk_icon_view_enable_model_drag_source (
		icon_view, GDK_BUTTON1_MASK,
		targets, n_targets, GDK_ACTION_COPY);

	gtk_target_table_free (targets, n_targets);
	gtk_target_list_unref (list);

	e_signal_connect_notify (object, "notify::visible", G_CALLBACK (visible_cb), NULL);
}

static void
picture_gallery_dispose (GObject *object)
{
	EPictureGallery *gallery;

	gallery = E_PICTURE_GALLERY (object);
	g_clear_object (&gallery->priv->monitor);

	g_free (gallery->priv->path);
	gallery->priv->path = NULL;

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_picture_gallery_parent_class)->dispose (object);
}

static void
e_picture_gallery_class_init (EPictureGalleryClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = picture_gallery_get_property;
	object_class->set_property = picture_gallery_set_property;
	object_class->constructed = picture_gallery_constructed;
	object_class->dispose = picture_gallery_dispose;

	g_object_class_install_property (
		object_class,
		PROP_PATH,
		g_param_spec_string (
			"path",
			"Gallery path",
			NULL,
			NULL,
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
e_picture_gallery_init (EPictureGallery *gallery)
{
	gallery->priv = e_picture_gallery_get_instance_private (gallery);
	gallery->priv->initialized = FALSE;
	gallery->priv->monitor = NULL;
	picture_gallery_set_path (gallery, NULL);
}

GtkWidget *
e_picture_gallery_new (const gchar *path)
{
	return g_object_new (E_TYPE_PICTURE_GALLERY, "path", path, NULL);
}
