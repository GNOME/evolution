/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * gal-view-collection.c
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>

#include <util/e-i18n.h>
#include <ctype.h>
#include <string.h>
#include <gtk/gtksignal.h>
#include <parser.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-util.h>
#include <gal/util/e-util.h>
#include <gal/util/e-xml-utils.h>
#include <gal/widgets/e-unicode.h>
#include "gal-view-collection.h"

#define GVC_CLASS(e) ((GalViewCollectionClass *)((GtkObject *)e)->klass)

#define PARENT_TYPE gtk_object_get_type ()

static GtkObjectClass *gal_view_collection_parent_class;

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

	gtk_signal_emit (GTK_OBJECT (collection),
			 gal_view_collection_signals [DISPLAY_VIEW],
			 view);
}

static void
gal_view_collection_changed (GalViewCollection *collection)
{
	g_return_if_fail (collection != NULL);
	g_return_if_fail (GAL_IS_VIEW_COLLECTION (collection));

	gtk_signal_emit (GTK_OBJECT (collection),
			 gal_view_collection_signals [CHANGED]);
}

static void
gal_view_collection_item_free (GalViewCollectionItem *item)
{
	g_free(item->id);
	if (item->view)
		gtk_object_unref(GTK_OBJECT(item->view));
	g_free(item);
}

static void
gal_view_collection_destroy (GtkObject *object)
{
	GalViewCollection *collection = GAL_VIEW_COLLECTION(object);
	int i;

	for (i = 0; i < collection->view_count; i++) {
		gal_view_collection_item_free (collection->view_data[i]);
	}
	g_free(collection->view_data);
	e_free_object_list(collection->factory_list);

	for (i = 0; i < collection->removed_view_count; i++) {
		gal_view_collection_item_free (collection->removed_view_data[i]);
	}
	g_free(collection->removed_view_data);

	g_free(collection->system_dir);
	g_free(collection->local_dir);

	if (gal_view_collection_parent_class->destroy)
		(*gal_view_collection_parent_class->destroy)(object);
}

static void
gal_view_collection_class_init (GtkObjectClass *object_class)
{
	GalViewCollectionClass *klass = GAL_VIEW_COLLECTION_CLASS(object_class);
	gal_view_collection_parent_class = gtk_type_class (PARENT_TYPE);
	
	object_class->destroy = gal_view_collection_destroy;

	gal_view_collection_signals [DISPLAY_VIEW] =
		gtk_signal_new ("display_view",
				GTK_RUN_LAST,
				E_OBJECT_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (GalViewCollectionClass, display_view),
				gtk_marshal_NONE__OBJECT,
				GTK_TYPE_NONE, 1, GAL_VIEW_TYPE);

	gal_view_collection_signals [CHANGED] =
		gtk_signal_new ("changed",
				GTK_RUN_LAST,
				E_OBJECT_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (GalViewCollectionClass, changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	E_OBJECT_CLASS_ADD_SIGNALS (object_class, gal_view_collection_signals, LAST_SIGNAL);

	klass->display_view = NULL;
	klass->changed      = NULL;
}

static void
gal_view_collection_init (GalViewCollection *collection)
{
	collection->view_data          = NULL;
	collection->view_count         = 0;
	collection->factory_list       = NULL;

	collection->removed_view_data  = NULL;
	collection->removed_view_count = 0;

	collection->system_dir         = NULL;
	collection->local_dir          = NULL;
}

/**
 * gal_view_collection_get_type:
 *
 */
guint
gal_view_collection_get_type (void)
{
	static guint type = 0;
	
	if (!type)
	{
		GtkTypeInfo info =
		{
			"GalViewCollection",
			sizeof (GalViewCollection),
			sizeof (GalViewCollectionClass),
			(GtkClassInitFunc) gal_view_collection_class_init,
			(GtkObjectInitFunc) gal_view_collection_init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		type = gtk_type_unique (PARENT_TYPE, &info);
	}

  return type;
}

/**
 * gal_view_collection_new:
 *
 * A collection of views and view factories.
 */
GalViewCollection *
gal_view_collection_new                      (void)
{
	return gtk_type_new(gal_view_collection_get_type());
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
					      const char        *system_dir,
					      const char        *local_dir)
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

	gtk_object_ref (GTK_OBJECT (factory));
	collection->factory_list = g_list_prepend (collection->factory_list, factory);
}

static void
view_changed (GalView *view,
	      GalViewCollectionItem *item)
{
	item->changed = TRUE;
	item->ever_changed = TRUE;

	gal_view_collection_changed(item->collection);
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
	item->id = e_xml_get_string_prop_by_name(node, "id");
	item->filename = e_xml_get_string_prop_by_name(node, "filename");
	item->title = e_xml_get_translated_utf8_string_prop_by_name(node, "title");
	item->type = e_xml_get_string_prop_by_name(node, "type");
	item->collection = collection;

	if (item->filename) {
		GalViewFactory *factory;
		GList *factories;

		factory = NULL;
		for (factories = collection->factory_list; factories; factories = factories->next) {
			if (item->type && !strcmp(gal_view_factory_get_type_code(factories->data), item->type)) {
				factory = factories->data;
				break;
			}
		}
		if (factory) {
			char *filename;

			filename = g_concat_dir_and_file(dir, item->filename);
			item->view = gal_view_factory_new_view (factory, item->title);
			gal_view_load(item->view, filename);
			gal_view_set_title (item->view, item->title);
			gtk_signal_connect(GTK_OBJECT(item->view), "changed",
					   GTK_SIGNAL_FUNC(view_changed), item);
			g_free(filename);
		}

	}
	return item;
}

static void
load_single_dir (GalViewCollection *collection,
		 char *dir,
		 gboolean local)
{
	xmlDoc *doc;
	xmlNode *root;
	xmlNode *child;
	char *filename = g_concat_dir_and_file(dir, "galview.xml");

	doc = xmlParseFile(filename);
	if (!doc) {
		g_free (filename);
		return;
	}
	root = xmlDocGetRootElement(doc);
	for (child = root->xmlChildrenNode; child; child = child->next) {
		gchar *id = e_xml_get_string_prop_by_name(child, "id");
		gboolean found = FALSE;
		int i;

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

	load_single_dir(collection, collection->local_dir, TRUE);
	load_single_dir(collection, collection->system_dir, FALSE);
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
	int i;
	xmlDoc *doc;
	xmlNode *root;
	char *filename;

	g_return_if_fail (collection != NULL);
	g_return_if_fail (GAL_IS_VIEW_COLLECTION (collection));
	g_return_if_fail (collection->local_dir != NULL);

	e_create_directory(collection->local_dir);

	doc = xmlNewDoc("1.0");
	root = xmlNewNode(NULL, "GalViewCollection");
	xmlDocSetRootElement(doc, root);
	for (i = 0; i < collection->view_count; i++) {
		xmlNode *child;
		GalViewCollectionItem *item;

		item = collection->view_data[i];
		if (item->ever_changed) {
			child = xmlNewChild(root, NULL, "GalView", NULL);
			e_xml_set_string_prop_by_name(child, "id", item->id);
			e_xml_set_string_prop_by_name(child, "title", item->title);
			e_xml_set_string_prop_by_name(child, "filename", item->filename);
			e_xml_set_string_prop_by_name(child, "type", item->type);

			if (item->changed) {
				filename = g_concat_dir_and_file(collection->local_dir, item->filename);
				gal_view_save(item->view, filename);
				g_free(filename);
			}
		}
	}
	for (i = 0; i < collection->removed_view_count; i++) {
		xmlNode *child;
		GalViewCollectionItem *item;

		item = collection->removed_view_data[i];

		child = xmlNewChild(root, NULL, "GalView", NULL);
		e_xml_set_string_prop_by_name(child, "id", item->id);
		e_xml_set_string_prop_by_name(child, "title", item->title);
		e_xml_set_string_prop_by_name(child, "type", item->type);
	}
	filename = g_concat_dir_and_file(collection->local_dir, "galview.xml");
	xmlSaveFile(filename, doc);
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
			      int n)
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
				   int n)
{
	g_return_val_if_fail (collection != NULL, NULL);
	g_return_val_if_fail (GAL_IS_VIEW_COLLECTION (collection), NULL);
	g_return_val_if_fail(n < collection->view_count, NULL);
	g_return_val_if_fail(n >= 0, NULL);

	return collection->view_data[n];
}

static char *
gal_view_generate_string (GalViewCollection *collection,
			  GalView           *view,
			  int which)
{
	char *ret_val;
	char *pointer;

	if (which == 1)
		ret_val = g_strdup(gal_view_get_title(view));
	else
		ret_val = g_strdup_printf("%s_%d", gal_view_get_title(view), which);
	for (pointer = ret_val; *pointer; pointer++) {
		if (!isalnum((guint) *pointer)) {
			*pointer = '_';
		}
	}
	return ret_val;
}

static gint
gal_view_check_string (GalViewCollection *collection,
		       char *string)
{
	int i;

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

static char *
gal_view_generate_id (GalViewCollection *collection,
		      GalView           *view)
{
	int i;
	for (i = 1; TRUE; i++) {
		char *try;

		try = gal_view_generate_string(collection, view, i);
		if (gal_view_check_string(collection, try))
			return try;
		g_free(try);
	}
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
	gtk_object_ref(GTK_OBJECT(view));

	gtk_signal_connect(GTK_OBJECT(item->view), "changed",
			   GTK_SIGNAL_FUNC(view_changed), item);

	collection->view_data = g_renew(GalViewCollectionItem *, collection->view_data, collection->view_count + 1);
	collection->view_data[collection->view_count] = item;
	collection->view_count ++;

	gal_view_collection_changed(collection);
}

void
gal_view_collection_delete_view              (GalViewCollection *collection,
					      int                i)
{
	GalViewCollectionItem *item;

	g_return_if_fail (collection != NULL);
	g_return_if_fail (GAL_IS_VIEW_COLLECTION (collection));
	g_return_if_fail (i >= 0 && i < collection->view_count);

	item = collection->view_data[i];
	memmove(collection->view_data + i, collection->view_data + i + 1, (collection->view_count - i - 1) * sizeof(GalViewCollectionItem *));
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
					      int                i)
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

	gtk_signal_connect(GTK_OBJECT(item->view), "changed",
			   GTK_SIGNAL_FUNC(view_changed), item);

	collection->view_data = g_renew(GalViewCollectionItem *, collection->view_data, collection->view_count + 1);
	collection->view_data[collection->view_count] = item;
	collection->view_count ++;

	gal_view_collection_changed(collection);
}
