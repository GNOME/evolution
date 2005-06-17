/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gal-view-treeview.c: An TreeView View
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *
 * (C) 2000, 2001 Ximian, Inc.
 */
#include <config.h>
#include "gal-view-treeview.h"
#include <libxml/parser.h>
#include <e-util/e-xml-utils.h>

#define PARENT_TYPE gal_view_get_type ()
#define d(x) x

static GalViewClass *gal_view_treeview_parent_class;

static void
gal_view_treeview_edit            (GalView *view, GtkWindow *parent_window)
{
	/* GalViewTreeView *treeview_view = GAL_VIEW_TREEVIEW(view); */
}

static void  
gal_view_treeview_load (GalView *view,
			const char *filename)
{
#if 0
	xmlDoc *doc;
	doc = xmlParseFile (filename);
	if (doc) {
		xmlNode *root = xmlDocGetRootElement(doc);
		GAL_VIEW_TREEVIEW (view)->column_width = e_xml_get_double_prop_by_name_with_default (root, "column_width", 150);
		xmlFreeDoc(doc);
	}
#endif
}

static void
gal_view_treeview_save (GalView *view,
			const char *filename)
{
#if 0
	xmlDoc *doc;
	xmlNode *root;

	doc = xmlNewDoc("1.0");
	root = xmlNewNode (NULL, "ETreeViewViewState");
	e_xml_set_double_prop_by_name (root, "column_width", GAL_VIEW_TREEVIEW (view)->column_width);
	xmlDocSetRootElement(doc, root);
	xmlSaveFile(filename, doc);
	xmlFreeDoc(doc);
#endif
}

static const char *
gal_view_treeview_get_title       (GalView *view)
{
	return GAL_VIEW_TREEVIEW(view)->title;
}

static void
gal_view_treeview_set_title       (GalView *view,
				 const char *title)
{
	g_free(GAL_VIEW_TREEVIEW(view)->title);
	GAL_VIEW_TREEVIEW(view)->title = g_strdup(title);
}

static const char *
gal_view_treeview_get_type_code (GalView *view)
{
	return "treeview";
}

static GalView *
gal_view_treeview_clone       (GalView *view)
{
	GalViewTreeView *gvm, *new;

	gvm = GAL_VIEW_TREEVIEW(view);

	new               = g_object_new (GAL_TYPE_VIEW_TREEVIEW, NULL);
	new->title        = g_strdup (gvm->title);

	return GAL_VIEW(new);
}

static void
gal_view_treeview_dispose         (GObject *object)
{
	GalViewTreeView *view = GAL_VIEW_TREEVIEW(object);

	if (view->title != NULL) {
		gal_view_treeview_detach (view);
		g_free(view->title);
		view->title = NULL;
	}

	if (G_OBJECT_CLASS (gal_view_treeview_parent_class)->dispose)
		(* G_OBJECT_CLASS (gal_view_treeview_parent_class)->dispose) (object);
}

static void
gal_view_treeview_class_init      (GObjectClass *object_class)
{
	GalViewClass *gal_view_class  = GAL_VIEW_CLASS(object_class);
	gal_view_treeview_parent_class  = g_type_class_ref (PARENT_TYPE);

	gal_view_class->edit          = gal_view_treeview_edit         ;
	gal_view_class->load          = gal_view_treeview_load         ;
	gal_view_class->save          = gal_view_treeview_save         ;
	gal_view_class->get_title     = gal_view_treeview_get_title    ;
	gal_view_class->set_title     = gal_view_treeview_set_title    ;
	gal_view_class->get_type_code = gal_view_treeview_get_type_code;
	gal_view_class->clone         = gal_view_treeview_clone        ;

	object_class->dispose         = gal_view_treeview_dispose      ;
}

static void
gal_view_treeview_init      (GalViewTreeView *gvm)
{
	gvm->title = NULL;

	gvm->tree = NULL;
}

/**
 * gal_view_treeview_new
 * @title: The name of the new view.
 *
 * Returns a new GalViewTreeView.  This is primarily for use by
 * GalViewFactoryTreeView.
 *
 * Returns: The new GalViewTreeView.
 */
GalView *
gal_view_treeview_new (const gchar *title)
{
	return gal_view_treeview_construct (g_object_new (GAL_TYPE_VIEW_TREEVIEW, NULL), title);
}

/**
 * gal_view_treeview_construct
 * @view: The view to construct.
 * @title: The name of the new view.
 *
 * constructs the GalViewTreeView.  To be used by subclasses and
 * language bindings.
 *
 * Returns: The GalViewTreeView.
 */
GalView *
gal_view_treeview_construct  (GalViewTreeView *view,
			      const gchar *title)
{
	view->title = g_strdup(title);
	return GAL_VIEW(view);
}

GType
gal_view_treeview_get_type        (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info =  {
			sizeof (GalViewTreeViewClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) gal_view_treeview_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (GalViewTreeView),
			0,             /* n_preallocs */
			(GInstanceInitFunc) gal_view_treeview_init,
		};

		type = g_type_register_static (PARENT_TYPE, "GalViewTreeView", &info, 0);
	}

	return type;
}

#if 0
static void
column_width_changed (ETable *table, double width, GalViewMinicard *view)
{
	d(g_print("%s: Old width = %f, New width = %f\n", G_GNUC_FUNCTION, view->column_width, width));
	if (view->column_width != width) {
		view->column_width = width;
		gal_view_changed(GAL_VIEW(view));
	}
}
#endif

void
gal_view_treeview_attach (GalViewTreeView *view, GtkTreeView *tree)
{
#if 0
	gal_view_treeview_detach (view);

	view->emvw = emvw;

	g_object_ref (view->emvw);

	g_object_set (view->emvw,
		      "column_width", view->column_width,
		      NULL);

	view->emvw_column_width_changed_id =
		g_signal_connect(view->emvw, "column_width_changed",
				 G_CALLBACK (column_width_changed), view);
#endif
}

void
gal_view_treeview_detach (GalViewTreeView *view)
{
#if 0
	if (view->emvw == NULL)
		return;
	if (view->emvw_column_width_changed_id) {
		g_signal_handler_disconnect (view->emvw,
					     view->emvw_column_width_changed_id);
		view->emvw_column_width_changed_id = 0;
	}
	g_object_unref (view->emvw);
	view->emvw = NULL;
#endif
}
