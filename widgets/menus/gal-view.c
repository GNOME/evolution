/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gal-view-menus.c: Savable state of a table.
 *
 * Author:
 *   Chris Lahey <clahey@helixcode.com>
 *
 * (C) 2000 Helix Code, Inc.
 */
#include <config.h>
#include <stdlib.h>
#include <gtk/gtksignal.h>
#include "gal-view.h"
#include <gal/util/e-util.h>

static void gal_view_set_arg (GtkObject *o, GtkArg *arg, guint arg_id);
static void gal_view_get_arg (GtkObject *object, GtkArg *arg, guint arg_id);

#define PARENT_TYPE (gtk_object_get_type())

static GtkObjectClass *gv_parent_class;

enum {
	ARG_0,
	ARG_NAME,
};

static void
gv_destroy (GtkObject *object)
{
	GalView *gv = GAL_VIEW (object);

	g_free(gv->name);

	GTK_OBJECT_CLASS (gv_parent_class)->destroy (object);
}

static void
gal_view_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	GalView *view;

	view = GAL_VIEW (o);
	
	switch (arg_id){
	case ARG_NAME:
		g_free(view->name);
		view->name = g_strdup(GTK_VALUE_STRING (*arg));
		break;
	}
}

static void
gal_view_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	GalView *view;

	view = GAL_VIEW (object);

	switch (arg_id) {
	case ARG_NAME:
		GTK_VALUE_STRING (*arg) = g_strdup(view->name);
		break;
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}

static void
gv_init (GalView *view)
{
	view->name = NULL;
}

static void
gv_class_init (GtkObjectClass *klass)
{
	gv_parent_class = gtk_type_class (PARENT_TYPE);
	
	klass->destroy = gv_destroy;
	klass->set_arg = gal_view_set_arg;
	klass->get_arg = gal_view_get_arg;

	gtk_object_add_arg_type ("GalView::name", GTK_TYPE_STRING,
				 GTK_ARG_READWRITE, ARG_NAME);
}

E_MAKE_TYPE(gal_view, "GalView", GalView, gv_class_init, gv_init, PARENT_TYPE);

GalView *
gal_view_new (void)
{
	GalView *gv = gtk_type_new (GAL_VIEW_TYPE);

	return (GalView *) gv;
}
