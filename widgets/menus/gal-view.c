/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gal-view.c: A View
 *
 * Authors:
 *   Chris Lahey (clahey@helixcode.com)
 *
 * (C) 2000 Helix Code, Inc.
 */
#include <config.h>
#include "gal-view.h"

#define GV_CLASS(e) ((GalViewClass *)((GtkObject *)e)->klass)

#define PARENT_TYPE gtk_object_get_type ()

#define d(x)

d(static gint depth = 0);


static GtkObjectClass *gal_view_parent_class;

/**
 * gal_view_edit
 * @view: The view to edit
 */
void
gal_view_edit            (GalView *view)
{
	g_return_if_fail (view != NULL);
	g_return_if_fail (GAL_IS_VIEW (view));

	if (GV_CLASS (view)->edit)
		GV_CLASS (view)->edit (view);
}

/**
 * gal_view_load_from_node
 * @view: The view to load to
 * @node: The xml data to load
 */
void  
gal_view_load_from_node  (GalView *view,
			  xmlNode *node)
{
	g_return_if_fail (view != NULL);
	g_return_if_fail (GAL_IS_VIEW (view));

	if (GV_CLASS (view)->load_from_node)
		GV_CLASS (view)->load_from_node (view, node);
}

/**
 * gal_view_save_to_node
 * @view: The view to save
 * @parent: Save the data as a child of this node
 */
void
gal_view_save_to_node    (GalView *view,
			  xmlNode *parent)
{
	g_return_if_fail (view != NULL);
	g_return_if_fail (GAL_IS_VIEW (view));

	if (GV_CLASS (view)->save_to_node)
		GV_CLASS (view)->save_to_node (view, parent);
}

/**
 * gal_view_get_title
 * @view: The view to query.
 *
 * Returns: The title of the view.
 */
const char *
gal_view_get_title       (GalView *view)
{
	g_return_val_if_fail (view != NULL, NULL);
	g_return_val_if_fail (GAL_IS_VIEW (view), NULL);

	if (GV_CLASS (view)->get_title)
		return GV_CLASS (view)->get_title (view);
	else
		return NULL;
}

/**
 * gal_view_clone
 * @view: The view to clone.
 *
 * Returns: The clone.
 */
GalView *
gal_view_clone       (GalView *view)
{
	g_return_val_if_fail (view != NULL, NULL);
	g_return_val_if_fail (GAL_IS_VIEW (view), NULL);

	if (GV_CLASS (view)->clone)
		return GV_CLASS (view)->clone (view);
	else
		return NULL;
}

static void
gal_view_class_init      (GtkObjectClass *object_class)
{
	GalViewClass *klass   = GAL_VIEW_CLASS(object_class);
	gal_view_parent_class = gtk_type_class (PARENT_TYPE);
	
	klass->edit           = NULL;     
	klass->load_from_node = NULL;     
	klass->save_to_node   = NULL;     
	klass->get_title      = NULL;     
	klass->clone          = NULL;
}

GtkType
gal_view_get_type        (void)
{
	static guint type = 0;
	
	if (!type)
	{
		GtkTypeInfo info =
		{
			"GalView",
			sizeof (GalView),
			sizeof (GalViewClass),
			(GtkClassInitFunc) gal_view_class_init,
			NULL,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		type = gtk_type_unique (PARENT_TYPE, &info);
	}

	return type;
}
