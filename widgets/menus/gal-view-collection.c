/*
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
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>

#include <ctype.h>
#include <string.h>
#include <errno.h>

#include <glib.h>
#include <libxml/parser.h>
#include <libedataserver/e-xml-utils.h>

#include <glib/gi18n.h>
#include "e-util/e-util.h"
#include "e-util/e-xml-utils.h"
#include "misc/e-unicode.h"

#include "gal-view-collection.h"

G_DEFINE_TYPE (GalViewCollection, gal_view_collection, G_TYPE_OBJECT)

#define d(x)

enum {
	DISPLAY_VIEW,
	CHANGED,
	LAST_SIGNAL
};

static guint gal_view_collection_signals [LAST_SIGNAL] = { 0, };

/**
 * gal_view_collection_display_view:
 * @collection: The GalViewCollection to send the signal on.
 * @view: The view to display.
 *
 */
void
gal_view_collection_display_view (GalViewCollection *collection,
				  GalView *view)
{
	g_return_if_fail (collection != NULL);
	g_return_if_fail (GAL_IS_VIEW_COLLECTION (collection));
	g_return_if_fail (view != NULL);
	g_return_if_fail (GAL_IS_VIEW (view));

	g_signal_emit (collection,
		       gal_view_collection_signals [DISPLAY_VIEW], 0,
		       view);
}

static void
gal_view_collection_changed (GalViewCollection *collection)
{
	g_return_if_fail (collection != NULL);
	g_return_if_fail (GAL_IS_VIEW_COLLECTION (collection));

	g_signal_emit (collection,
		       gal_view_collection_signals [CHANGED], 0);
}

static void
gal_view_collection_item_free (GalViewCollectionItem *item)
{
	g_free(item->id);
	if (item->view) {
		if (item->view_changed_id)
			g_signal_handler_disconnect (item->view,
						     item->view_changed_id);
		g_object_unref(item->view);
	}
	g_free(item);
}

static gchar *
gal_view_generate_string (GalViewCollection *collection,
			  GalView           *view,
			  gint which)
{
	gchar *ret_val;
	gchar *pointer;

	if (which == 1)
		ret_val = g_strdup(gal_view_get_title(view));
	else
		ret_val = g_strdup_printf("%s_%d", gal_view_get_title(view), which);
	for (pointer = ret_val; *pointer; pointer = g_utf8_next_char(pointer)) {
		if (!g_unichar_isalnum(g_utf8_get_char(pointer))) {
			gchar *ptr = pointer;
			for (; ptr < g_utf8_next_char(pointer); *ptr = '_', ptr++)
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

	for (i = 0; i < collection->view_count; i++) {
		if (!strcmp(string, collection->view_data[i]->id))
			return FALSE;
	}
	for (i = 0; i < collection->removed_view_count; i++) {
		if (!strcmp(string, collection->removed_view_data[i]->id))
			return FALSE;
	}
	return TRUE;
}

static gchar *
gal_view_generate_id (GalViewCollection *collection,
		      GalView           *view)
{
	gint i;
	for (i = 1; TRUE; i++) {
		gchar *try;

		try = gal_view_generate_string(collection, view, i);
		if (gal_view_check_string(collection, try))
			return try;
		g_free(try);
	}
}

static void
gal_view_collection_dispose (GObject *object)
{
	GalViewCollection *collection = GAL_VIEW_COLLECTION(object);
	gint i;

	for (i = 0; i < collection->view_count; i++) {
		gal_view_collection_item_free (collection->view_data[i]);
	}
	g_free (collection->view_data);
	collection->view_data = NULL;
	collection->view_count = 0;

	g_list_foreach (
		collection->factory_list,
		(GFunc) g_object_unref, NULL);
	g_list_free (collection->factory_list);
	collection->factory_list = NULL;

	for (i = 0; i < collection->removed_view_count; i++) {
		gal_view_collection_item_free (collection->removed_view_data[i]);
	}
	g_free(collection->removed_view_data);
	collection->removed_view_data  = NULL;
	collection->removed_view_count = 0;

	g_free(collection->system_dir);
	collection->system_dir = NULL;

	g_free(collection->local_dir);
	collection->local_dir = NULL;

	g_free (collection->default_view);
	collection->default_view = NULL;

	g_free (collection->title);
	collection->title = NULL;

	if (G_OBJECT_CLASS (gal_view_collection_parent_class)->dispose)
		(* G_OBJECT_CLASS (gal_view_collection_parent_class)->dispose)(object);
}

static void
gal_view_collection_class_init (GalViewCollectionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = gal_view_collection_dispose;

	gal_view_collection_signals [DISPLAY_VIEW] =
		g_signal_new ("display_view",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GalViewCollectionClass, display_view),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, GAL_VIEW_TYPE);

	gal_view_collection_signals [CHANGED] =
		g_signal_new ("changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GalViewCollectionClass, changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	klass->display_view = NULL;
	klass->changed      = NULL;
}

static void
gal_view_collection_init (GalViewCollection *collection)
{
	collection->view_data             = NULL;
	collection->view_count            = 0;
	collection->factory_list          = NULL;

	collection->removed_view_data     = NULL;
	collection->removed_view_count    = 0;

	collection->system_dir            = NULL;
	collection->local_dir             = NULL;

	collection->loaded                = FALSE;
	collection->default_view          = NULL;
	collection->default_view_built_in = TRUE;

	collection->title                 = NULL;
}

/**
 * gal_view_collection_new:
 *
 * A collection of views and view factories.
 */
GalViewCollection *
gal_view_collection_new                      (void)
{
	return g_object_new (GAL_VIEW_COLLECTION_TYPE, NULL);
}

void
gal_view_collection_set_title (GalViewCollection *collection,
			       const gchar *title)
{
	g_free (collection->title);
	collection->title = g_strdup (title);
}

/**
 * gal_view_collection_set_storage_directories
 * @collection: The view collection to initialize
 * @system_dir: The location of the system built in views
 * @local_dir: The location to store the users set up views
 *
 * Sets up the GalViewCollection.
 */
void
gal_view_collection_set_storage_directories  (GalViewCollection *collection,
					      const gchar        *system_dir,
					      const gchar        *local_dir)
{
	g_return_if_fail (collection != NULL);
	g_return_if_fail (GAL_IS_VIEW_COLLECTION (collection));
	g_return_if_fail (system_dir != NULL);
	g_return_if_fail (local_dir != NULL);

	g_free(collection->system_dir);
	g_free(collection->local_dir);

	collection->system_dir = g_strdup(system_dir);
	collection->local_dir = g_strdup(local_dir);
}

/**
 * gal_view_collection_add_factory
 * @collection: The view collection to add a factory to
 * @factory: The factory to add.  The @collection will add a reference
 * to the factory object, so you should unref it after calling this
 * function if you no longer need it.
 *
 * Adds the given factory to this collection.  This list is used both
 * when loading views from their xml description as well as when the
 * user tries to create a new view.
 */
void
gal_view_collection_add_factory              (GalViewCollection *collection,
					      GalViewFactory    *factory)
{
	g_return_if_fail (collection != NULL);
	g_return_if_fail (GAL_IS_VIEW_COLLECTION (collection));
	g_return_if_fail (factory != NULL);
	g_return_if_fail (GAL_IS_VIEW_FACTORY (factory));

	g_object_ref (factory);
	collection->factory_list = g_list_prepend (collection->factory_list, factory);
}

static void
view_changed (GalView *view,
	      GalViewCollectionItem *item)
{
	item->changed = TRUE;
	item->ever_changed = TRUE;

	g_signal_handler_block(G_OBJECT(item->view), item->view_changed_id);
	gal_view_collection_changed(item->collection);
	g_signal_handler_unblock(G_OBJECT(item->view), item->view_changed_id);
}

/* Use factory list to load a GalView file. */
static GalView *
gal_view_collection_real_load_view_from_file (GalViewCollection *collection, const gchar *type, const gchar *title, const gchar *dir, const gchar *filename)
{
	GalViewFactory *factory;
	GList *factories;

	factory = NULL;
	for (factories = collection->factory_list; factories; factories = factories->next) {
		if (type && !strcmp(gal_view_factory_get_type_code(factories->data), type)) {
			factory = factories->data;
			break;
		}
	}
	if (factory) {
		GalView *view;

		view = gal_view_factory_new_view (factory, title);
		gal_view_set_title (view, title);
		gal_view_load(view, filename);
		return view;
	}
	return NULL;
}

GalView *
gal_view_collection_load_view_from_file (GalViewCollection *collection, const gchar *type, const gchar *filename)
{
	return gal_view_collection_real_load_view_from_file (collection, type, "", collection->local_dir, filename);
}

static GalViewCollectionItem *
load_single_file (GalViewCollection *collection,
		  gchar *dir,
		  gboolean local,
		  xmlNode *node)
{
	GalViewCollectionItem *item;
	item = g_new(GalViewCollectionItem, 1);
	item->ever_changed = local;
	item->changed = FALSE;
	item->built_in = !local;
	item->id = e_xml_get_string_prop_by_name(node, (const guchar *)"id");
	item->filename = e_xml_get_string_prop_by_name(node, (const guchar *)"filename");
	item->title = e_xml_get_translated_utf8_string_prop_by_name(node, (const guchar *)"title");
	item->type = e_xml_get_string_prop_by_name(node, (const guchar *)"type");
	item->collection = collection;
	item->view_changed_id = 0;

	if (item->filename) {
		gchar *fullpath;
		fullpath = g_build_filename(dir, item->filename, NULL);
		item->view = gal_view_collection_real_load_view_from_file (collection, item->type, item->title, dir, fullpath);
		g_free(fullpath);
		if (item->view) {
			item->view_changed_id =
				g_signal_connect(item->view, "changed",
						 G_CALLBACK(view_changed), item);
		}
	}
	return item;
}

static void
load_single_dir (GalViewCollection *collection,
		 gchar *dir,
		 gboolean local)
{
	xmlDoc *doc = NULL;
	xmlNode *root;
	xmlNode *child;
	gchar *filename = g_build_filename(dir, "galview.xml", NULL);
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
	root = xmlDocGetRootElement(doc);
	for (child = root->xmlChildrenNode; child; child = child->next) {
		gchar *id;
		gboolean found = FALSE;
		gint i;

		if (!strcmp ((gchar *)child->name, "text"))
			continue;

		id = e_xml_get_string_prop_by_name(child, (const guchar *)"id");
		for (i = 0; i < collection->view_count; i++) {
			if (!strcmp(id, collection->view_data[i]->id)) {
				if (!local)
					collection->view_data[i]->built_in = TRUE;
				found = TRUE;
				break;
			}
		}
		if (!found) {
			for (i = 0; i < collection->removed_view_count; i++) {
				if (!strcmp(id, collection->removed_view_data[i]->id)) {
					if (!local)
						collection->removed_view_data[i]->built_in = TRUE;
					found = TRUE;
					break;
				}
			}
		}

		if (!found) {
			GalViewCollectionItem *item = load_single_file (collection, dir, local, child);
			if (item->filename && *item->filename) {
				collection->view_data = g_renew(GalViewCollectionItem *, collection->view_data, collection->view_count + 1);
				collection->view_data[collection->view_count] = item;
				collection->view_count ++;
			} else {
				collection->removed_view_data = g_renew(GalViewCollectionItem *, collection->removed_view_data, collection->removed_view_count + 1);
				collection->removed_view_data[collection->removed_view_count] = item;
				collection->removed_view_count ++;
			}
		}
		g_free(id);
	}

	default_view = e_xml_get_string_prop_by_name (root, (const guchar *)"default-view");
	if (default_view) {
		if (local)
			collection->default_view_built_in = FALSE;
		else
			collection->default_view_built_in = TRUE;
		g_free (collection->default_view);
		collection->default_view = default_view;
	}

	g_free(filename);
	xmlFreeDoc(doc);
}

/**
 * gal_view_collection_load
 * @collection: The view collection to load information for
 *
 * Loads the data from the system and user directories specified in
 * set storage directories.  This is primarily for internal use by
 * other parts of gal_view.
 */
void
gal_view_collection_load              (GalViewCollection *collection)
{
	g_return_if_fail (collection != NULL);
	g_return_if_fail (GAL_IS_VIEW_COLLECTION (collection));
	g_return_if_fail (collection->local_dir != NULL);
	g_return_if_fail (collection->system_dir != NULL);
	g_return_if_fail (!collection->loaded);

	if ((g_mkdir_with_parents (collection->local_dir, 0777) == -1) && (errno != EEXIST))
		g_warning ("Unable to create dir %s: %s", collection->local_dir, g_strerror(errno));

	load_single_dir(collection, collection->local_dir, TRUE);
	load_single_dir(collection, collection->system_dir, FALSE);
	gal_view_collection_changed(collection);

	collection->loaded = TRUE;
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
gal_view_collection_save              (GalViewCollection *collection)
{
	gint i;
	xmlDoc *doc;
	xmlNode *root;
	gchar *filename;

	g_return_if_fail (collection != NULL);
	g_return_if_fail (GAL_IS_VIEW_COLLECTION (collection));
	g_return_if_fail (collection->local_dir != NULL);

	doc = xmlNewDoc((const guchar *)"1.0");
	root = xmlNewNode(NULL, (const guchar *)"GalViewCollection");
	xmlDocSetRootElement(doc, root);

	if (collection->default_view && !collection->default_view_built_in) {
		e_xml_set_string_prop_by_name(root, (const guchar *)"default-view", collection->default_view);
	}

	for (i = 0; i < collection->view_count; i++) {
		xmlNode *child;
		GalViewCollectionItem *item;

		item = collection->view_data[i];
		if (item->ever_changed) {
			child = xmlNewChild(root, NULL, (const guchar *)"GalView", NULL);
			e_xml_set_string_prop_by_name(child, (const guchar *)"id", item->id);
			e_xml_set_string_prop_by_name(child, (const guchar *)"title", item->title);
			e_xml_set_string_prop_by_name(child, (const guchar *)"filename", item->filename);
			e_xml_set_string_prop_by_name(child, (const guchar *)"type", item->type);

			if (item->changed) {
				filename = g_build_filename(collection->local_dir, item->filename, NULL);
				gal_view_save(item->view, filename);
				g_free(filename);
			}
		}
	}
	for (i = 0; i < collection->removed_view_count; i++) {
		xmlNode *child;
		GalViewCollectionItem *item;

		item = collection->removed_view_data[i];

		child = xmlNewChild(root, NULL, (const guchar *)"GalView", NULL);
		e_xml_set_string_prop_by_name(child, (const guchar *)"id", item->id);
		e_xml_set_string_prop_by_name(child, (const guchar *)"title", item->title);
		e_xml_set_string_prop_by_name(child, (const guchar *)"type", item->type);
	}
	filename = g_build_filename(collection->local_dir, "galview.xml", NULL);
	if (e_xml_save_file (filename, doc) == -1)
		g_warning ("Unable to save view to %s - %s", filename, g_strerror(errno));
	xmlFreeDoc(doc);
	g_free(filename);
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
	g_return_val_if_fail (collection != NULL, -1);
	g_return_val_if_fail (GAL_IS_VIEW_COLLECTION (collection), -1);

	return collection->view_count;
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
	g_return_val_if_fail (collection != NULL, NULL);
	g_return_val_if_fail (GAL_IS_VIEW_COLLECTION (collection), NULL);
	g_return_val_if_fail (n < collection->view_count, NULL);
	g_return_val_if_fail (n >= 0, NULL);

	return collection->view_data[n]->view;
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
	g_return_val_if_fail (collection != NULL, NULL);
	g_return_val_if_fail (GAL_IS_VIEW_COLLECTION (collection), NULL);
	g_return_val_if_fail(n < collection->view_count, NULL);
	g_return_val_if_fail(n >= 0, NULL);

	return collection->view_data[n];
}

gint
gal_view_collection_get_view_index_by_id     (GalViewCollection *collection, const gchar *view_id)
{
	gint i;
	for (i = 0; i < collection->view_count; i++) {
		if (!strcmp (collection->view_data[i]->id, view_id))
			return i;
	}
	return -1;
}

gchar *
gal_view_collection_get_view_id_by_index (GalViewCollection *collection, gint n)
{
	g_return_val_if_fail (collection != NULL, NULL);
	g_return_val_if_fail (GAL_IS_VIEW_COLLECTION (collection), NULL);
	g_return_val_if_fail(n < collection->view_count, NULL);
	g_return_val_if_fail(n >= 0, NULL);

	return g_strdup (collection->view_data[n]->id);
}

void
gal_view_collection_append                   (GalViewCollection *collection,
					      GalView           *view)
{
	GalViewCollectionItem *item;

	g_return_if_fail (collection != NULL);
	g_return_if_fail (GAL_IS_VIEW_COLLECTION (collection));
	g_return_if_fail (view != NULL);
	g_return_if_fail (GAL_IS_VIEW (view));

	item = g_new(GalViewCollectionItem, 1);
	item->ever_changed = TRUE;
	item->changed = TRUE;
	item->built_in = FALSE;
	item->title = g_strdup(gal_view_get_title(view));
	item->type = g_strdup(gal_view_get_type_code(view));
	item->id = gal_view_generate_id(collection, view);
	item->filename = g_strdup_printf("%s.galview", item->id);
	item->view = view;
	item->collection = collection;
	g_object_ref(view);

	item->view_changed_id =
		g_signal_connect(item->view, "changed",
				 G_CALLBACK (view_changed), item);

	collection->view_data = g_renew(GalViewCollectionItem *, collection->view_data, collection->view_count + 1);
	collection->view_data[collection->view_count] = item;
	collection->view_count ++;

	gal_view_collection_changed(collection);
}

void
gal_view_collection_delete_view              (GalViewCollection *collection,
					      gint                i)
{
	GalViewCollectionItem *item;

	g_return_if_fail (collection != NULL);
	g_return_if_fail (GAL_IS_VIEW_COLLECTION (collection));
	g_return_if_fail (i >= 0 && i < collection->view_count);

	item = collection->view_data[i];
	memmove(collection->view_data + i, collection->view_data + i + 1, (collection->view_count - i - 1) * sizeof(GalViewCollectionItem *));
	collection->view_count --;
	if (item->built_in) {
		g_free(item->filename);
		item->filename = NULL;

		collection->removed_view_data = g_renew(GalViewCollectionItem *, collection->removed_view_data, collection->removed_view_count + 1);
		collection->removed_view_data[collection->removed_view_count] = item;
		collection->removed_view_count ++;
	} else {
		gal_view_collection_item_free (item);
	}

	gal_view_collection_changed(collection);
}

void
gal_view_collection_copy_view                (GalViewCollection *collection,
					      gint                i)
{
	GalViewCollectionItem *item;
	GalView *view;

	g_return_if_fail (collection != NULL);
	g_return_if_fail (GAL_IS_VIEW_COLLECTION (collection));
	g_return_if_fail (i >= 0 && i < collection->view_count);

	view = collection->view_data[i]->view;

	item = g_new(GalViewCollectionItem, 1);
	item->ever_changed = TRUE;
	item->changed = FALSE;
	item->built_in = FALSE;
	item->title = g_strdup(gal_view_get_title(view));
	item->type = g_strdup(gal_view_get_type_code(view));
	item->id = gal_view_generate_id(collection, view);
	item->filename = g_strdup_printf("%s.galview", item->id);
	item->view = gal_view_clone(view);
	item->collection = collection;

	item->view_changed_id =
		g_signal_connect(item->view, "changed",
				 G_CALLBACK (view_changed), item);

	collection->view_data = g_renew(GalViewCollectionItem *, collection->view_data, collection->view_count + 1);
	collection->view_data[collection->view_count] = item;
	collection->view_count ++;

	gal_view_collection_changed(collection);
}

gboolean
gal_view_collection_loaded (GalViewCollection *collection)
{
	return collection->loaded;
}

const gchar *
gal_view_collection_append_with_title (GalViewCollection *collection, const gchar *title, GalView *view)
{
	GalViewCollectionItem *item;

	g_return_val_if_fail (collection != NULL, NULL);
	g_return_val_if_fail (GAL_IS_VIEW_COLLECTION (collection), NULL);
	g_return_val_if_fail (view != NULL, NULL);
	g_return_val_if_fail (GAL_IS_VIEW (view), NULL);

	gal_view_set_title (view, title);

	d(g_print("%s: %p\n", G_STRFUNC, view));

	item = g_new(GalViewCollectionItem, 1);
	item->ever_changed = TRUE;
	item->changed = TRUE;
	item->built_in = FALSE;
	item->title = g_strdup(gal_view_get_title(view));
	item->type = g_strdup(gal_view_get_type_code(view));
	item->id = gal_view_generate_id(collection, view);
	item->filename = g_strdup_printf("%s.galview", item->id);
	item->view = view;
	item->collection = collection;
	g_object_ref(view);

	item->view_changed_id =
		g_signal_connect(item->view, "changed",
				 G_CALLBACK (view_changed), item);

	collection->view_data = g_renew(GalViewCollectionItem *, collection->view_data, collection->view_count + 1);
	collection->view_data[collection->view_count] = item;
	collection->view_count ++;

	gal_view_collection_changed(collection);
	return item->id;
}

const gchar *
gal_view_collection_set_nth_view (GalViewCollection *collection, gint i, GalView *view)
{
	GalViewCollectionItem *item;

	g_return_val_if_fail (collection != NULL, NULL);
	g_return_val_if_fail (GAL_IS_VIEW_COLLECTION (collection), NULL);
	g_return_val_if_fail (view != NULL, NULL);
	g_return_val_if_fail (GAL_IS_VIEW (view), NULL);
	g_return_val_if_fail (i >= 0, NULL);
	g_return_val_if_fail (i < collection->view_count, NULL);

	d(g_print("%s: %p\n", G_STRFUNC, view));

	item = collection->view_data[i];

	gal_view_set_title (view, item->title);
	g_object_ref (view);
	if (item->view) {
		g_signal_handler_disconnect (item->view,
					     item->view_changed_id);
		g_object_unref (item->view);
	}
	item->view = view;

	item->ever_changed = TRUE;
	item->changed = TRUE;
	item->type = g_strdup(gal_view_get_type_code(view));

	item->view_changed_id =
		g_signal_connect(item->view, "changed",
				 G_CALLBACK (view_changed), item);

	gal_view_collection_changed (collection);
	return item->id;
}

const gchar *
gal_view_collection_get_default_view (GalViewCollection *collection)
{
	return collection->default_view;
}

void
gal_view_collection_set_default_view (GalViewCollection *collection, const gchar *id)
{
	g_free (collection->default_view);
	collection->default_view = g_strdup (id);
	gal_view_collection_changed (collection);
	collection->default_view_built_in = FALSE;
}

