/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gal-view-etable.c: An ETable View
 *
 * Authors:
 *   Chris Lahey (clahey@helixcode.com)
 *
 * (C) 2000 Helix Code, Inc.
 */
#include <config.h>
#include "gal-view-etable.h"
#include <gal/e-table/e-table-config.h>

#define PARENT_TYPE gal_view_get_type ()

static GalViewClass *gal_view_etable_parent_class;

static void
gal_view_etable_edit            (GalView *view)
{
	GalViewEtable *etable_view = GAL_VIEW_ETABLE(view);
	e_table_config_new(etable_view->title,
			   etable_view->spec,
			   etable_view->state);
}

static void  
gal_view_etable_load_from_node  (GalView *view,
				 xmlNode *node)
{

}

static void
gal_view_etable_save_to_node    (GalView *view,
				 xmlNode *parent)
{

}

static const char *
gal_view_etable_get_title       (GalView *view)
{
	return GAL_VIEW_ETABLE(view)->title;
}

static GalView *
gal_view_etable_clone       (GalView *view)
{
	GalViewEtable *gve, *new;

	gve = GAL_VIEW_ETABLE(view);

	new        = gtk_type_new (gal_view_etable_get_type ());
	new->spec  = gve->spec;
	new->title = g_strdup (gve->title);
	new->state = e_table_state_duplicate(gve->state);

	gtk_object_ref(GTK_OBJECT(new->spec));

	return GAL_VIEW(new);
}

static void
gal_view_etable_destroy         (GtkObject *object)
{
	GalViewEtable *view = GAL_VIEW_ETABLE(object);
	g_free(view->title);
	if (view->spec)
		gtk_object_unref(GTK_OBJECT(view->spec));
	if (view->state)
		gtk_object_unref(GTK_OBJECT(view->state));
}

static void
gal_view_etable_class_init      (GtkObjectClass *object_class)
{
	GalViewClass *gal_view_class = GAL_VIEW_CLASS(object_class);
	gal_view_etable_parent_class = gtk_type_class (PARENT_TYPE);
	
	gal_view_class->edit           = gal_view_etable_edit          ;
	gal_view_class->load_from_node = gal_view_etable_load_from_node;
	gal_view_class->save_to_node   = gal_view_etable_save_to_node  ;
	gal_view_class->get_title      = gal_view_etable_get_title     ;
	gal_view_class->clone          = gal_view_etable_clone         ;

	object_class->destroy          = gal_view_etable_destroy       ;
}

static void
gal_view_etable_init      (GalViewEtable *gve)
{
	gve->spec  = NULL;
	gve->state = e_table_state_new();
	gve->title = NULL;
}

/**
 * gal_view_etable_new
 * @spec: The ETableSpecification that this view will be based upon.
 * @title: The name of the new view.
 *
 * Returns a new GalViewEtable.  This is primarily for use by
 * GalViewFactoryEtable.
 *
 * Returns: The new GalViewEtable.
 */
GalView *
gal_view_etable_new (ETableSpecification *spec,
		     const gchar *title)
{
	return gal_view_etable_construct (gtk_type_new (gal_view_etable_get_type ()), spec, title);
}

/**
 * gal_view_etable_construct
 * @view: The view to construct.
 * @spec: The ETableSpecification that this view will be based upon.
 * @title: The name of the new view.
 *
 * constructs the GalViewEtable.  To be used by subclasses and
 * language bindings.
 *
 * Returns: The GalViewEtable.
 */
GalView *
gal_view_etable_construct  (GalViewEtable *view,
			    ETableSpecification *spec,
			    const gchar *title)
{
	if (spec)
		gtk_object_ref(GTK_OBJECT(spec));
	view->spec = spec;
	view->title = g_strdup(title);
	return GAL_VIEW(view);
}

GtkType
gal_view_etable_get_type        (void)
{
	static guint type = 0;
	
	if (!type)
	{
		GtkTypeInfo info =
		{
			"GalViewEtable",
			sizeof (GalViewEtable),
			sizeof (GalViewEtableClass),
			(GtkClassInitFunc) gal_view_etable_class_init,
			(GtkObjectInitFunc) gal_view_etable_init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		type = gtk_type_unique (PARENT_TYPE, &info);
	}

	return type;
}
