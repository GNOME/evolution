/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gal-view-collection.c: a View Collection
 *
 * Authors:
 *   Chris Lahey (clahey@helixcode.com)
 *
 * (C) 1999, 2000 Helix Code, Inc.
 */
#include <config.h>
#include <gtk/gtksignal.h>
#include <gal/util/e-util.h>
#include "gal-view-collection.h"

#define GVC_CLASS(e) ((GalViewCollectionClass *)((GtkObject *)e)->klass)

#define PARENT_TYPE gtk_object_get_type ()

static GtkObjectClass *gal_view_collection_parent_class;

enum {
	DISPLAY_VIEW,
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

	gtk_signal_emit (GTK_OBJECT (collection),
			 gal_view_collection_signals [DISPLAY_VIEW],
			 view);
}

static void
gal_view_collection_destroy (GtkObject *object)
{
	GalViewCollection *collection = GAL_VIEW_COLLECTION(object);

	e_free_object_list(collection->view_list);
	e_free_object_list(collection->factory_list);

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
				object_class->type,
				GTK_SIGNAL_OFFSET (GalViewCollectionClass, display_view),
				gtk_marshal_NONE__OBJECT,
				GTK_TYPE_NONE, 1, GTK_TYPE_OBJECT);

	gtk_object_class_add_signals (object_class, gal_view_collection_signals, LAST_SIGNAL);

	klass->display_view = NULL;     
}

static void
gal_view_collection_init (GalViewCollection *collection)
{
	collection->view_list    = NULL;
	collection->factory_list = NULL;
}

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

GalViewCollection *gal_view_collection_new                      (void)
{
	return gtk_type_new(gal_view_collection_get_type());
}

/* Set up the view collection */
void               gal_view_collection_set_storage_directories  (GalViewCollection *collection,
								 char              *system_dir,
								 char              *local_dir)
{
}

void               gal_view_collection_add_factory              (GalViewCollection *collection,
								 GalViewFactory    *factory)
{
	gtk_object_ref(GTK_OBJECT(factory));
	collection->factory_list = g_list_prepend(collection->factory_list, factory);
}
