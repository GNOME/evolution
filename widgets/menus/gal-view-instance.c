/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * gal-view-instance.c
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
#include <gnome-xml/parser.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-util.h>
#include <gal/util/e-util.h>
#include <gal/util/e-xml-utils.h>
#include <gal/widgets/e-unicode.h>
#include "gal-view-instance.h"

#define GVI_CLASS(e) ((GalViewInstanceClass *)((GtkObject *)e)->klass)

#define PARENT_TYPE gtk_object_get_type ()

static GtkObjectClass *gal_view_instance_parent_class;

enum {
	DISPLAY_VIEW,
	CHANGED,
	LAST_SIGNAL
};

static guint gal_view_instance_signals [LAST_SIGNAL] = { 0, };

static void
gal_view_instance_changed (GalViewInstance *instance)
{
	g_return_if_fail (instance != NULL);
	g_return_if_fail (GAL_IS_VIEW_INSTANCE (instance));

	gtk_signal_emit (GTK_OBJECT (instance),
			 gal_view_instance_signals [CHANGED]);
}

static void
gal_view_instance_display_view (GalViewInstance *instance, GalView *view)
{
	g_return_if_fail (instance != NULL);
	g_return_if_fail (GAL_IS_VIEW_INSTANCE (instance));

	gtk_signal_emit (GTK_OBJECT (instance),
			 gal_view_instance_signals [DISPLAY_VIEW],
			 view);
}

static void
gal_view_instance_destroy (GtkObject *object)
{
	GalViewInstance *instance = GAL_VIEW_INSTANCE(object);

	if (instance->collection)
		gtk_object_unref (GTK_OBJECT (instance->collection));

	g_free (instance->instance_id);
	g_free (instance->custom_filename);
	g_free (instance->current_view_filename);

	g_free (instance->current_title);
	g_free (instance->current_type);
	g_free (instance->current_id);
	if (instance->current_view)
		gtk_object_unref (GTK_OBJECT (instance->current_view));
	
	if (gal_view_instance_parent_class->destroy)
		(*gal_view_instance_parent_class->destroy)(object);
}

static void
gal_view_instance_class_init (GtkObjectClass *object_class)
{
	GalViewInstanceClass *klass = GAL_VIEW_INSTANCE_CLASS(object_class);
	gal_view_instance_parent_class = gtk_type_class (PARENT_TYPE);
	
	object_class->destroy = gal_view_instance_destroy;

	gal_view_instance_signals [DISPLAY_VIEW] =
		gtk_signal_new ("display_view",
				GTK_RUN_LAST,
				E_OBJECT_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (GalViewInstanceClass, display_view),
				gtk_marshal_NONE__OBJECT,
				GTK_TYPE_NONE, 1, GAL_VIEW_TYPE);

	gal_view_instance_signals [CHANGED] =
		gtk_signal_new ("changed",
				GTK_RUN_LAST,
				E_OBJECT_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (GalViewInstanceClass, changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	E_OBJECT_CLASS_ADD_SIGNALS (object_class, gal_view_instance_signals, LAST_SIGNAL);

	klass->display_view = NULL;
	klass->changed      = NULL;
}

static void
gal_view_instance_init (GalViewInstance *instance)
{
	instance->collection = NULL;

	instance->instance_id = NULL;
	instance->custom_filename = NULL;
	instance->current_view_filename = NULL;
	
	instance->current_title = NULL;
	instance->current_type = NULL;
	instance->current_id = NULL;
	instance->current_view = NULL;

}

/**
 * gal_view_instance_get_type:
 *
 */
guint
gal_view_instance_get_type (void)
{
	static guint type = 0;
	
	if (!type)
	{
		GtkTypeInfo info =
		{
			"GalViewInstance",
			sizeof (GalViewInstance),
			sizeof (GalViewInstanceClass),
			(GtkClassInitFunc) gal_view_instance_class_init,
			(GtkObjectInitFunc) gal_view_instance_init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		type = gtk_type_unique (PARENT_TYPE, &info);
	}

  return type;
}

static void
save_current_view (GalViewInstance *instance)
{
	xmlDoc *doc;
	xmlNode *root;

	doc = xmlNewDoc("1.0");
	root = xmlNewNode (NULL, "GalViewCurrentView");
	xmlDocSetRootElement(doc, root);

	if (instance->current_id)
		e_xml_set_string_prop_by_name (root, "current_view", instance->current_id);
	if (instance->current_type)
		e_xml_set_string_prop_by_name (root, "current_view_type", instance->current_type);

	xmlSaveFile(instance->current_view_filename, doc);
	xmlFreeDoc(doc);
}

static void
view_changed (GalView *view, GalViewInstance *instance)
{
	if (instance->current_id != NULL) {
		instance->current_view = NULL;
		save_current_view (instance);
		gal_view_instance_changed(instance);
	}

	gal_view_save (view, instance->custom_filename);
}

static void
load_current_view (GalViewInstance *instance)
{
	xmlDoc *doc;
	xmlNode *root;

	doc = xmlParseFile(instance->current_view_filename);

	if (!doc)
		return;

	root = xmlDocGetRootElement(doc);
	instance->current_id = e_xml_get_string_prop_by_name_with_default (root, "current_view", NULL);
	instance->current_type = e_xml_get_string_prop_by_name_with_default (root, "current_view_type", NULL);
	xmlFreeDoc(doc);

	if (instance->current_id == NULL) {
		instance->current_view =
			gal_view_collection_load_view_from_file (instance->collection,
								 instance->current_type,
								 instance->custom_filename);

	} else {
		int index = gal_view_collection_get_view_index_by_id (instance->collection,
								      instance->current_id);
		GalView *view = gal_view_collection_get_view (instance->collection,
							      index);
		instance->current_view = gal_view_clone(view);

	}

	instance->current_title = g_strdup (gal_view_get_title(instance->current_view));
	gtk_signal_connect(GTK_OBJECT(instance->current_view), "changed",
			   GTK_SIGNAL_FUNC(view_changed), instance);

	gal_view_instance_display_view (instance, instance->current_view);
}

/**
 * gal_view_instance_new:
 * @collection: This %GalViewCollection should be loaded before being passed to this function.
 * @instance_id: Which instance of this type of object is this (for most of evo, this is the folder id.)
 * 
 * Create a new %GalViewInstance.
 * 
 * Return value: The new %GalViewInstance.
 **/
GalViewInstance *
gal_view_instance_new (GalViewCollection *collection, const char *instance_id)
{
	GalViewInstance *instance = gtk_type_new(gal_view_instance_get_type());
	if (gal_view_instance_construct (instance, collection, instance_id))
		return instance;
	else {
		gtk_object_unref (GTK_OBJECT (instance));
		return NULL;
	}
}

GalViewInstance *
gal_view_instance_construct (GalViewInstance *instance, GalViewCollection *collection, const char *instance_id)
{
	char *filename;
	char *safe_id;

	g_return_val_if_fail (gal_view_collection_loaded (collection), NULL);

	instance->collection = collection;
	if (collection)
		gtk_object_ref (GTK_OBJECT (collection));
	instance->instance_id = g_strdup (instance_id);

	safe_id = g_strdup (instance->instance_id);
	e_filename_make_safe (safe_id);

	filename = g_strdup_printf ("custom_view-%s.xml", safe_id);
	instance->custom_filename = g_concat_dir_and_file (instance->collection->local_dir, filename);
	g_free (filename);

	filename = g_strdup_printf ("current_view-%s.xml", safe_id);
	instance->current_view_filename = g_concat_dir_and_file (instance->collection->local_dir, filename);
	g_free (filename);

	g_free (safe_id);

	load_current_view (instance);

	return instance;
}

/* Manipulate the current view. */
char *
gal_view_instance_get_current_view_id (GalViewInstance *instance)
{
	return g_strdup (instance->current_id);
}

void
gal_view_instance_set_current_view_id (GalViewInstance *instance, char *view_id)
{
	GalView *view;
	int index;

	g_return_if_fail (instance != NULL);
	g_return_if_fail (GAL_IS_VIEW_INSTANCE (instance));

	if (instance->current_view && !strcmp (instance->current_id, view_id))
		return;

	if (instance->current_view) {
		gtk_object_unref (GTK_OBJECT (instance->current_view));
	}

	g_free (instance->current_type);
	g_free (instance->current_title);
	g_free (instance->current_id);

	index = gal_view_collection_get_view_index_by_id (instance->collection, view_id);
	view = gal_view_collection_get_view (instance->collection, index);

	instance->current_title = g_strdup (gal_view_get_title(view));
	instance->current_type = g_strdup (gal_view_get_type_code(view));
	instance->current_id = g_strdup (view_id);
	instance->current_view = gal_view_clone(view);

	gtk_signal_connect(GTK_OBJECT(instance->current_view), "changed",
			   GTK_SIGNAL_FUNC(view_changed), instance);

	save_current_view (instance);
	gal_view_instance_changed(instance);
	gal_view_instance_display_view (instance, view);
}

GalView *
gal_view_instance_get_current_view (GalViewInstance *instance)
{
	return instance->current_view;
}
