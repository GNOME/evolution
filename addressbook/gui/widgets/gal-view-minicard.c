/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gal-view-minicard.c: An Minicard View
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *
 * (C) 2000, 2001 Ximian, Inc.
 */
#include <config.h>
#include "gal-view-minicard.h"

#define PARENT_TYPE gal_view_get_type ()

static GalViewClass *gal_view_minicard_parent_class;

static void
gal_view_minicard_edit            (GalView *view)
{
	/* GalViewMinicard *minicard_view = GAL_VIEW_MINICARD(view); */
}

static void  
gal_view_minicard_load  (GalView *view,
			 const char *filename)
{
}

static void
gal_view_minicard_save    (GalView *view,
			 const char *filename)
{
}

static const char *
gal_view_minicard_get_title       (GalView *view)
{
	return GAL_VIEW_MINICARD(view)->title;
}

static void
gal_view_minicard_set_title       (GalView *view,
				 const char *title)
{
	g_free(GAL_VIEW_MINICARD(view)->title);
	GAL_VIEW_MINICARD(view)->title = g_strdup(title);
}

static const char *
gal_view_minicard_get_type_code (GalView *view)
{
	return "minicard";
}

static GalView *
gal_view_minicard_clone       (GalView *view)
{
	GalViewMinicard *gve, *new;

	gve = GAL_VIEW_MINICARD(view);

	new        = gtk_type_new (gal_view_minicard_get_type ());
	new->title = g_strdup (gve->title);

	return GAL_VIEW(new);
}

static void
gal_view_minicard_destroy         (GtkObject *object)
{
	GalViewMinicard *view = GAL_VIEW_MINICARD(object);
	g_free(view->title);
}

static void
gal_view_minicard_class_init      (GtkObjectClass *object_class)
{
	GalViewClass *gal_view_class  = GAL_VIEW_CLASS(object_class);
	gal_view_minicard_parent_class  = gtk_type_class (PARENT_TYPE);

	gal_view_class->edit          = gal_view_minicard_edit         ;
	gal_view_class->load          = gal_view_minicard_load         ;
	gal_view_class->save          = gal_view_minicard_save         ;
	gal_view_class->get_title     = gal_view_minicard_get_title    ;
	gal_view_class->set_title     = gal_view_minicard_set_title    ;
	gal_view_class->get_type_code = gal_view_minicard_get_type_code;
	gal_view_class->clone         = gal_view_minicard_clone        ;

	object_class->destroy         = gal_view_minicard_destroy      ;
}

static void
gal_view_minicard_init      (GalViewMinicard *gve)
{
	gve->title = NULL;
}

/**
 * gal_view_minicard_new
 * @title: The name of the new view.
 *
 * Returns a new GalViewMinicard.  This is primarily for use by
 * GalViewFactoryMinicard.
 *
 * Returns: The new GalViewMinicard.
 */
GalView *
gal_view_minicard_new (const gchar *title)
{
	return gal_view_minicard_construct (gtk_type_new (gal_view_minicard_get_type ()), title);
}

/**
 * gal_view_minicard_construct
 * @view: The view to construct.
 * @title: The name of the new view.
 *
 * constructs the GalViewMinicard.  To be used by subclasses and
 * language bindings.
 *
 * Returns: The GalViewMinicard.
 */
GalView *
gal_view_minicard_construct  (GalViewMinicard *view,
			      const gchar *title)
{
	view->title = g_strdup(title);
	return GAL_VIEW(view);
}

GtkType
gal_view_minicard_get_type        (void)
{
	static guint type = 0;
	
	if (!type)
	{
		GtkTypeInfo info =
		{
			"GalViewMinicard",
			sizeof (GalViewMinicard),
			sizeof (GalViewMinicardClass),
			(GtkClassInitFunc) gal_view_minicard_class_init,
			(GtkObjectInitFunc) gal_view_minicard_init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		type = gtk_type_unique (PARENT_TYPE, &info);
	}

	return type;
}
