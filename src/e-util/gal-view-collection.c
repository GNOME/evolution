/*
 * gal-view-collection.c
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
 */

#include "gal-view-collection.h"

#include <ctype.h>
#include <string.h>
#include <errno.h>

#include <libxml/parser.h>
#include <libedataserver/libedataserver.h>

#include <libebackend/libebackend.h>

#include "e-unicode.h"
#include "e-xml-utils.h"

struct _GalViewCollectionPrivate {
	GalViewCollectionItem **view_data;
	gint view_count;

	GalViewCollectionItem **removed_view_data;
	gint removed_view_count;

	gboolean default_view_built_in;

	gchar *system_directory;
	gchar *user_directory;

	gchar *default_view;
};

enum {
	PROP_0,
	PROP_SYSTEM_DIRECTORY,
	PROP_USER_DIRECTORY
};

enum {
	CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_PRIVATE (GalViewCollection, gal_view_collection, G_TYPE_OBJECT)

static void
gal_view_collection_changed (GalViewCollection *collection)
{
	g_return_if_fail (GAL_IS_VIEW_COLLECTION (collection));

	g_signal_emit (collection, signals[CHANGED], 0);
}

static void
gal_view_collection_item_free (GalViewCollectionItem *item)
{
	g_free (item->id);
	if (item->view) {
		if (item->view_changed_id)
			g_signal_handler_disconnect (
				item->view,
				item->view_changed_id);
		g_object_unref (item->view);
	}
	g_free (item->accelerator);
	g_free (item);
}

static gchar *
gal_view_generate_string (GalViewCollection *collection,
                          GalView *view,
                          gint which)
{
	gchar *ret_val;
	gchar *pointer;

	if (which == 1)
		ret_val = g_strdup (gal_view_get_title (view));
	else
		ret_val = g_strdup_printf ("%s_%d", gal_view_get_title (view), which);
	for (pointer = ret_val; *pointer; pointer = g_utf8_next_char (pointer)) {
		if (!g_unichar_isalnum (g_utf8_get_char (pointer))) {
			gchar *ptr = pointer;
			for (; ptr < g_utf8_next_char (pointer); *ptr = '_', ptr++)
				;
		}
	}
	return ret_val;
}

static gint
gal_view_check_string (GalViewCollection *collection,
                       gchar *string)
{
	gint i;

	if (!strcmp (string, "current_view"))
		return FALSE;

	for (i = 0; i < collection->priv->view_count; i++) {
		if (!strcmp (string, collection->priv->view_data[i]->id))
			return FALSE;
	}
	for (i = 0; i < collection->priv->removed_view_count; i++) {
		if (!strcmp (string, collection->priv->removed_view_data[i]->id))
			return FALSE;
	}
	return TRUE;
}

static gchar *
gal_view_generate_id (GalViewCollection *collection,
                      GalView *view)
{
	gint i;
	for (i = 1; TRUE; i++) {
		gchar *try;

		try = gal_view_generate_string (collection, view, i);
		if (gal_view_check_string (collection, try))
			return try;
		g_free (try);
	}
}

static void
view_collection_check_type (GType type,
                            gpointer user_data)
{
	GalViewClass *class;

	struct {
		const gchar *type_code;
		GType type;
	} *closure = user_data;

	class = g_type_class_ref (type);
	g_return_if_fail (class != NULL);

	if (g_strcmp0 (class->type_code, closure->type_code) == 0)
		closure->type = type;

	g_type_class_unref (class);
}

/* Use factory list to load a GalView file. */
static GalView *
gal_view_collection_real_load_view_from_file (GalViewCollection *collection,
                                              const gchar *type,
                                              const gchar *title,
                                              const gchar *dir,
                                              const gchar *filename)
{
	GalView *view = NULL;

	struct {
		const gchar *type_code;
		GType type;
	} closure;

	closure.type_code = type;
	closure.type = G_TYPE_INVALID;

	/* Find the appropriate GalView subtype for the "type_code" string. */
	e_type_traverse (GAL_TYPE_VIEW, view_collection_check_type, &closure);

	if (g_type_is_a (closure.type, GAL_TYPE_VIEW)) {
		view = g_object_new (closure.type, "title", title, NULL);
		gal_view_load (view, filename);
	}

	return view;
}

static void
view_changed (GalView *view,
              GalViewCollectionItem *item)
{
	item->changed = TRUE;
	item->ever_changed = TRUE;

	g_signal_handler_block (item->view, item->view_changed_id);
	gal_view_collection_changed (item->collection);
	g_signal_handler_unblock (item->view, item->view_changed_id);
}

static GalViewCollectionItem *
load_single_file (GalViewCollection *collection,
                  const gchar *dir,
                  gboolean local,
                  xmlNode *node)
{
	GalViewCollectionItem *item;
	item = g_new (GalViewCollectionItem, 1);
	item->ever_changed = local;
	item->changed = FALSE;
	item->built_in = !local;
	item->id = e_xml_get_string_prop_by_name (node, (const guchar *)"id");
	item->filename = e_xml_get_string_prop_by_name (node, (const guchar *)"filename");
	item->title = e_xml_get_translated_utf8_string_prop_by_name (node, (const guchar *)"title");
	item->type = e_xml_get_string_prop_by_name (node, (const guchar *)"type");
	item->collection = collection;
	item->view_changed_id = 0;
	item->accelerator = e_xml_get_string_prop_by_name (node, (const guchar *)"accelerator");

	if (item->filename) {
		gchar *fullpath;
		fullpath = g_build_filename (dir, item->filename, NULL);
		item->view = gal_view_collection_real_load_view_from_file (collection, item->type, item->title, dir, fullpath);
		g_free (fullpath);
		if (item->view) {
			item->view_changed_id = g_signal_connect (
				item->view, "changed",
				G_CALLBACK (view_changed), item);
		}
	}
	return item;
}

static void
load_single_dir (GalViewCollection *collection,
                 const gchar *dir,
                 gboolean local)
{
	xmlDoc *doc = NULL;
	xmlNode *root;
	xmlNode *child;
	gchar *filename = g_build_filename (dir, "galview.xml", NULL);
	gchar *default_view;

	if (g_file_test (filename, G_FILE_TEST_IS_REGULAR)) {
#ifdef G_OS_WIN32
		gchar *locale_filename = g_win32_locale_filename_from_utf8 (filename);
		if (locale_filename != NULL)
			doc = xmlParseFile (locale_filename);
		g_free (locale_filename);
#else
		doc = xmlParseFile (filename);
#endif
	}

	if (!doc) {
		g_free (filename);
		return;
	}
	root = xmlDocGetRootElement (doc);
	for (child = root->xmlChildrenNode; child; child = child->next) {
		gchar *id;
		gboolean found = FALSE;
		gint i;

		if (!strcmp ((gchar *) child->name, "text"))
			continue;

		id = e_xml_get_string_prop_by_name (child, (const guchar *)"id");
		for (i = 0; i < collection->priv->view_count; i++) {
			if (!strcmp (id, collection->priv->view_data[i]->id)) {
				if (!local)
					collection->priv->view_data[i]->built_in = TRUE;
				found = TRUE;
				break;
			}
		}
		if (!found) {
			for (i = 0; i < collection->priv->removed_view_count; i++) {
				if (!strcmp (id, collection->priv->removed_view_data[i]->id)) {
					if (!local)
						collection->priv->removed_view_data[i]->built_in = TRUE;
					found = TRUE;
					break;
				}
			}
		}

		if (!found) {
			GalViewCollectionItem *item = load_single_file (collection, dir, local, child);
			if (item->filename && *item->filename) {
				collection->priv->view_data = g_renew (GalViewCollectionItem *, collection->priv->view_data, collection->priv->view_count + 1);
				collection->priv->view_data[collection->priv->view_count] = item;
				collection->priv->view_count++;
			} else {
				collection->priv->removed_view_data = g_renew (GalViewCollectionItem *, collection->priv->removed_view_data, collection->priv->removed_view_count + 1);
				collection->priv->removed_view_data[collection->priv->removed_view_count] = item;
				collection->priv->removed_view_count++;
			}
		}
		g_free (id);
	}

	default_view = e_xml_get_string_prop_by_name (root, (const guchar *)"default-view");
	if (default_view) {
		if (local)
			collection->priv->default_view_built_in = FALSE;
		else
			collection->priv->default_view_built_in = TRUE;
		g_free (collection->priv->default_view);
		collection->priv->default_view = default_view;
	}

	g_free (filename);
	xmlFreeDoc (doc);
}

static void
gal_view_collection_set_system_directory (GalViewCollection *collection,
                                          const gchar *system_directory)
{
	g_return_if_fail (system_directory != NULL);
	g_return_if_fail (collection->priv->system_directory == NULL);

	collection->priv->system_directory = g_strdup (system_directory);
}

static void
gal_view_collection_set_user_directory (GalViewCollection *collection,
                                        const gchar *user_directory)
{
	g_return_if_fail (user_directory != NULL);
	g_return_if_fail (collection->priv->user_directory == NULL);

	collection->priv->user_directory = g_strdup (user_directory);
}

static void
gal_view_collection_set_property (GObject *object,
                                  guint property_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SYSTEM_DIRECTORY:
			gal_view_collection_set_system_directory (
				GAL_VIEW_COLLECTION (object),
				g_value_get_string (value));
			return;

		case PROP_USER_DIRECTORY:
			gal_view_collection_set_user_directory (
				GAL_VIEW_COLLECTION (object),
				g_value_get_string (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
gal_view_collection_get_property (GObject *object,
                                  guint property_id,
                                  GValue *value,
                                  GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SYSTEM_DIRECTORY:
			g_value_set_string (
				value,
				gal_view_collection_get_system_directory (
				GAL_VIEW_COLLECTION (object)));
			return;

		case PROP_USER_DIRECTORY:
			g_value_set_string (
				value,
				gal_view_collection_get_user_directory (
				GAL_VIEW_COLLECTION (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
gal_view_collection_dispose (GObject *object)
{
	GalViewCollection *self = GAL_VIEW_COLLECTION (object);
	gint ii;

	for (ii = 0; ii < self->priv->view_count; ii++) {
		gal_view_collection_item_free (self->priv->view_data[ii]);
	}

	g_clear_pointer (&self->priv->view_data, g_free);
	self->priv->view_count = 0;

	for (ii = 0; ii < self->priv->removed_view_count; ii++) {
		gal_view_collection_item_free (self->priv->removed_view_data[ii]);
	}

	g_clear_pointer (&self->priv->removed_view_data, g_free);
	self->priv->removed_view_count = 0;

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (gal_view_collection_parent_class)->dispose (object);
}

static void
gal_view_collection_finalize (GObject *object)
{
	GalViewCollection *self = GAL_VIEW_COLLECTION (object);

	g_free (self->priv->system_directory);
	g_free (self->priv->user_directory);
	g_free (self->priv->default_view);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (gal_view_collection_parent_class)->finalize (object);
}

static void
gal_view_collection_constructed (GObject *object)
{
	GalViewCollection *collection;
	const gchar *directory;

	collection = GAL_VIEW_COLLECTION (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (gal_view_collection_parent_class)->constructed (object);

	/* XXX Maybe this should implement GInitable, since creating
	 *     directories and reading files can fail.  Although, we
	 *     would probably just abort Evolution on error anyway. */

	directory = gal_view_collection_get_user_directory (collection);
	g_mkdir_with_parents (directory, 0700);
	load_single_dir (collection, directory, TRUE);

	directory = gal_view_collection_get_system_directory (collection);
	load_single_dir (collection, directory, FALSE);
}

static void
gal_view_collection_class_init (GalViewCollectionClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = gal_view_collection_set_property;
	object_class->get_property = gal_view_collection_get_property;
	object_class->dispose = gal_view_collection_dispose;
	object_class->finalize = gal_view_collection_finalize;
	object_class->constructed = gal_view_collection_constructed;

	g_object_class_install_property (
		object_class,
		PROP_SYSTEM_DIRECTORY,
		g_param_spec_string (
			"system-directory",
			"System Directory",
			"Directory from which to load built-in views",
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_USER_DIRECTORY,
		g_param_spec_string (
			"user-directory",
			"User Directory",
			"Directory from which to load user-created views",
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	signals[CHANGED] = g_signal_new (
		"changed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (GalViewCollectionClass, changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
gal_view_collection_init (GalViewCollection *collection)
{
	collection->priv = gal_view_collection_get_instance_private (collection);

	collection->priv->default_view_built_in = TRUE;
}

/**
 * gal_view_collection_new:
 * @system_directory: directory from which to load built-in views
 * @user_directory: directory from which to load user-created views
 *
 * Creates a #GalViewCollection and loads ".galview" files from
 * @system_directory and @user_directory.
 */
GalViewCollection *
gal_view_collection_new (const gchar *system_directory,
                         const gchar *user_directory)
{
	g_return_val_if_fail (system_directory != NULL, NULL);
	g_return_val_if_fail (user_directory != NULL, NULL);

	return g_object_new (
		GAL_TYPE_VIEW_COLLECTION,
		"system-directory", system_directory,
		"user-directory", user_directory, NULL);
}

/**
 * gal_view_collection_get_system_directory:
 * @collection: a #GalViewCollection
 *
 * Returns the directory from which built-in views were loaded.
 *
 * Returns: the system directory for @collection
 **/
const gchar *
gal_view_collection_get_system_directory (GalViewCollection *collection)
{
	g_return_val_if_fail (GAL_IS_VIEW_COLLECTION (collection), NULL);

	return collection->priv->system_directory;
}

/**
 * gal_view_collection_get_user_directory:
 * @collection: a #GalViewCollection
 *
 * Returns the directory from which user-created views were loaded.
 *
 * Returns: the user directory for @collection
 **/
const gchar *
gal_view_collection_get_user_directory (GalViewCollection *collection)
{
	g_return_val_if_fail (GAL_IS_VIEW_COLLECTION (collection), NULL);

	return collection->priv->user_directory;
}

GalView *
gal_view_collection_load_view_from_file (GalViewCollection *collection,
                                         const gchar *type,
                                         const gchar *filename)
{
	return gal_view_collection_real_load_view_from_file (
		collection, type, "",
		collection->priv->user_directory, filename);
}

/**
 * gal_view_collection_save
 * @collection: The view collection to save information for
 *
 * Saves the data to the user directory specified in set storage
 * directories.  This is primarily for internal use by other parts of
 * gal_view.
 */
void
gal_view_collection_save (GalViewCollection *collection)
{
	gint i;
	xmlDoc *doc;
	xmlNode *root;
	gchar *filename;
	const gchar *user_directory;

	g_return_if_fail (GAL_IS_VIEW_COLLECTION (collection));

	user_directory = gal_view_collection_get_user_directory (collection);
	g_return_if_fail (user_directory != NULL);

	doc = xmlNewDoc ((const guchar *)"1.0");
	root = xmlNewNode (NULL, (const guchar *)"GalViewCollection");
	xmlDocSetRootElement (doc, root);

	if (collection->priv->default_view && !collection->priv->default_view_built_in) {
		e_xml_set_string_prop_by_name (root, (const guchar *)"default-view", collection->priv->default_view);
	}

	for (i = 0; i < collection->priv->view_count; i++) {
		xmlNode *child;
		GalViewCollectionItem *item;

		item = collection->priv->view_data[i];
		if (item->ever_changed) {
			child = xmlNewChild (root, NULL, (const guchar *)"GalView", NULL);
			e_xml_set_string_prop_by_name (child, (const guchar *)"id", item->id);
			e_xml_set_string_prop_by_name (child, (const guchar *)"title", item->title);
			e_xml_set_string_prop_by_name (child, (const guchar *)"filename", item->filename);
			e_xml_set_string_prop_by_name (child, (const guchar *)"type", item->type);

			if (item->changed) {
				filename = g_build_filename (user_directory, item->filename, NULL);
				gal_view_save (item->view, filename);
				g_free (filename);
			}
		}
	}
	for (i = 0; i < collection->priv->removed_view_count; i++) {
		xmlNode *child;
		GalViewCollectionItem *item;

		item = collection->priv->removed_view_data[i];

		child = xmlNewChild (root, NULL, (const guchar *)"GalView", NULL);
		e_xml_set_string_prop_by_name (child, (const guchar *)"id", item->id);
		e_xml_set_string_prop_by_name (child, (const guchar *)"title", item->title);
		e_xml_set_string_prop_by_name (child, (const guchar *)"type", item->type);
	}
	filename = g_build_filename (user_directory, "galview.xml", NULL);
	if (e_xml_save_file (filename, doc) == -1)
		g_warning ("Unable to save view to %s - %s", filename, g_strerror (errno));
	xmlFreeDoc (doc);
	g_free (filename);
}

/**
 * gal_view_collection_get_count
 * @collection: The view collection to count
 *
 * Calculates the number of views in the given collection.
 *
 * Returns: The number of views in the collection.
 */
gint
gal_view_collection_get_count (GalViewCollection *collection)
{
	g_return_val_if_fail (GAL_IS_VIEW_COLLECTION (collection), -1);

	return collection->priv->view_count;
}

/**
 * gal_view_collection_get_view
 * @collection: The view collection to query
 * @n: The view to get.
 *
 * Returns: The nth view in the collection
 */
GalView *
gal_view_collection_get_view (GalViewCollection *collection,
                              gint n)
{
	g_return_val_if_fail (GAL_IS_VIEW_COLLECTION (collection), NULL);
	g_return_val_if_fail (n < collection->priv->view_count, NULL);
	g_return_val_if_fail (n >= 0, NULL);

	return collection->priv->view_data[n]->view;
}

/**
 * gal_view_collection_get_view_item
 * @collection: The view collection to query
 * @n: The view item to get.
 *
 * Returns: The nth view item in the collection
 */
GalViewCollectionItem *
gal_view_collection_get_view_item (GalViewCollection *collection,
                                   gint n)
{
	g_return_val_if_fail (GAL_IS_VIEW_COLLECTION (collection), NULL);
	g_return_val_if_fail (n < collection->priv->view_count, NULL);
	g_return_val_if_fail (n >= 0, NULL);

	return collection->priv->view_data[n];
}

gint
gal_view_collection_get_view_index_by_id (GalViewCollection *collection,
                                          const gchar *view_id)
{
	gint ii;

	g_return_val_if_fail (GAL_IS_VIEW_COLLECTION (collection), -1);
	g_return_val_if_fail (view_id != NULL, -1);

	for (ii = 0; ii < collection->priv->view_count; ii++) {
		if (!strcmp (collection->priv->view_data[ii]->id, view_id))
			return ii;
	}

	return -1;
}

void
gal_view_collection_delete_view (GalViewCollection *collection,
                                 gint i)
{
	GalViewCollectionItem *item;

	g_return_if_fail (GAL_IS_VIEW_COLLECTION (collection));
	g_return_if_fail (i >= 0 && i < collection->priv->view_count);

	item = collection->priv->view_data[i];
	memmove (collection->priv->view_data + i, collection->priv->view_data + i + 1, (collection->priv->view_count - i - 1) * sizeof (GalViewCollectionItem *));
	collection->priv->view_count--;
	if (item->built_in) {
		g_free (item->filename);
		item->filename = NULL;

		collection->priv->removed_view_data = g_renew (GalViewCollectionItem *, collection->priv->removed_view_data, collection->priv->removed_view_count + 1);
		collection->priv->removed_view_data[collection->priv->removed_view_count] = item;
		collection->priv->removed_view_count++;
	} else {
		gal_view_collection_item_free (item);
	}

	gal_view_collection_changed (collection);
}

const gchar *
gal_view_collection_append_with_title (GalViewCollection *collection,
                                       const gchar *title,
                                       GalView *view)
{
	GalViewCollectionItem *item;
	GalViewClass *view_class;

	g_return_val_if_fail (GAL_IS_VIEW_COLLECTION (collection), NULL);
	g_return_val_if_fail (GAL_IS_VIEW (view), NULL);

	view_class = GAL_VIEW_GET_CLASS (view);
	g_return_val_if_fail (view_class != NULL, NULL);

	gal_view_set_title (view, title);

	item = g_new (GalViewCollectionItem, 1);
	item->ever_changed = TRUE;
	item->changed = TRUE;
	item->built_in = FALSE;
	item->title = g_strdup (gal_view_get_title (view));
	item->type = g_strdup (view_class->type_code);
	item->id = gal_view_generate_id (collection, view);
	item->filename = g_strdup_printf ("%s.galview", item->id);
	item->view = view;
	item->collection = collection;
	item->accelerator = NULL;
	g_object_ref (view);

	item->view_changed_id = g_signal_connect (
		item->view, "changed",
		G_CALLBACK (view_changed), item);

	collection->priv->view_data = g_renew (GalViewCollectionItem *, collection->priv->view_data, collection->priv->view_count + 1);
	collection->priv->view_data[collection->priv->view_count] = item;
	collection->priv->view_count++;

	gal_view_collection_changed (collection);
	return item->id;
}

const gchar *
gal_view_collection_set_nth_view (GalViewCollection *collection,
                                  gint i,
                                  GalView *view)
{
	GalViewCollectionItem *item;
	GalViewClass *view_class;

	g_return_val_if_fail (GAL_IS_VIEW_COLLECTION (collection), NULL);
	g_return_val_if_fail (GAL_IS_VIEW (view), NULL);
	g_return_val_if_fail (i >= 0, NULL);
	g_return_val_if_fail (i < collection->priv->view_count, NULL);

	view_class = GAL_VIEW_GET_CLASS (view);
	g_return_val_if_fail (view_class != NULL, NULL);

	item = collection->priv->view_data[i];

	gal_view_set_title (view, item->title);
	g_object_ref (view);
	if (item->view) {
		g_signal_handler_disconnect (
			item->view,
			item->view_changed_id);
		g_object_unref (item->view);
	}
	item->view = view;

	item->ever_changed = TRUE;
	item->changed = TRUE;
	item->type = g_strdup (view_class->type_code);

	item->view_changed_id = g_signal_connect (
		item->view, "changed",
		G_CALLBACK (view_changed), item);

	gal_view_collection_changed (collection);
	return item->id;
}

const gchar *
gal_view_collection_get_default_view (GalViewCollection *collection)
{
	g_return_val_if_fail (GAL_IS_VIEW_COLLECTION (collection), NULL);

	return collection->priv->default_view;
}

