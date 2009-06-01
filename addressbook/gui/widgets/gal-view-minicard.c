/*
 *
 * gal-view-minicard.c: An Minicard View
 *
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

#include <libxml/parser.h>

#include <libedataserver/e-xml-utils.h>

#include <e-util/e-xml-utils.h>

#include "gal-view-minicard.h"

#define PARENT_TYPE gal_view_get_type ()
#define d(x)

static GalViewClass *gal_view_minicard_parent_class;

static void
gal_view_minicard_load (GalView *view,
			const gchar *filename)
{
	xmlDoc *doc;

	doc = e_xml_parse_file (filename);
	if (doc) {
		xmlNode *root = xmlDocGetRootElement(doc);
		GAL_VIEW_MINICARD (view)->column_width = e_xml_get_double_prop_by_name_with_default (root, (const guchar *)"column_width", 150);
		xmlFreeDoc(doc);
	}
}

static void
gal_view_minicard_save (GalView *view,
			const gchar *filename)
{
	xmlDoc *doc;
	xmlNode *root;

	doc = xmlNewDoc((const guchar *)"1.0");
	root = xmlNewNode (NULL, (const guchar *)"EMinicardViewState");
	e_xml_set_double_prop_by_name (root, (const guchar *)"column_width", GAL_VIEW_MINICARD (view)->column_width);
	xmlDocSetRootElement(doc, root);
	e_xml_save_file (filename, doc);
	xmlFreeDoc(doc);
}

static const gchar *
gal_view_minicard_get_title       (GalView *view)
{
	return GAL_VIEW_MINICARD(view)->title;
}

static void
gal_view_minicard_set_title       (GalView *view,
				 const gchar *title)
{
	g_free(GAL_VIEW_MINICARD(view)->title);
	GAL_VIEW_MINICARD(view)->title = g_strdup(title);
}

static const gchar *
gal_view_minicard_get_type_code (GalView *view)
{
	return "minicard";
}

static GalView *
gal_view_minicard_clone       (GalView *view)
{
	GalViewMinicard *gvm, *new;

	gvm = GAL_VIEW_MINICARD(view);

	new               = g_object_new (GAL_TYPE_VIEW_MINICARD, NULL);
	new->title        = g_strdup (gvm->title);
	new->column_width = gvm->column_width;

	return GAL_VIEW(new);
}

static void
gal_view_minicard_dispose         (GObject *object)
{
	GalViewMinicard *view = GAL_VIEW_MINICARD(object);

	if (view->title != NULL) {
		gal_view_minicard_detach (view);
		g_free(view->title);
		view->title = NULL;
	}

	if (G_OBJECT_CLASS (gal_view_minicard_parent_class)->dispose)
		(* G_OBJECT_CLASS (gal_view_minicard_parent_class)->dispose) (object);
}

static void
gal_view_minicard_class_init      (GObjectClass *object_class)
{
	GalViewClass *gal_view_class  = GAL_VIEW_CLASS(object_class);
	gal_view_minicard_parent_class  = g_type_class_ref (PARENT_TYPE);

	gal_view_class->edit          = NULL;
	gal_view_class->load          = gal_view_minicard_load;
	gal_view_class->save          = gal_view_minicard_save;
	gal_view_class->get_title     = gal_view_minicard_get_title;
	gal_view_class->set_title     = gal_view_minicard_set_title;
	gal_view_class->get_type_code = gal_view_minicard_get_type_code;
	gal_view_class->clone         = gal_view_minicard_clone;

	object_class->dispose         = gal_view_minicard_dispose;
}

static void
gal_view_minicard_init      (GalViewMinicard *gvm)
{
	gvm->title = NULL;
	gvm->column_width = 150.0;

	gvm->emvw = NULL;
	gvm->emvw_column_width_changed_id = 0;
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
	return gal_view_minicard_construct (g_object_new (GAL_TYPE_VIEW_MINICARD, NULL), title);
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

GType
gal_view_minicard_get_type        (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info =  {
			sizeof (GalViewMinicardClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) gal_view_minicard_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (GalViewMinicard),
			0,             /* n_preallocs */
			(GInstanceInitFunc) gal_view_minicard_init,
		};

		type = g_type_register_static (PARENT_TYPE, "GalViewMinicard", &info, 0);
	}

	return type;
}

static void
column_width_changed (EMinicardViewWidget *w, double width, EABView *address_view)
{
	GalViewMinicard *view = GAL_VIEW_MINICARD (gal_view_instance_get_current_view (address_view->view_instance));
	GtkScrolledWindow *scrolled_window;
	GtkAdjustment *adj;
	GtkAdjustment *adj_new;

	d(g_print("%s: Old width = %f, New width = %f\n", G_STRFUNC, view->column_width, width));
	if (view->column_width != width) {
		view->column_width = width;
		gal_view_changed(GAL_VIEW(view));
	}

	scrolled_window = GTK_SCROLLED_WINDOW (address_view->widget);
	adj = gtk_scrolled_window_get_hadjustment (scrolled_window);
	adj_new = GTK_ADJUSTMENT (gtk_adjustment_new (adj->value, adj->lower, adj->upper,
						     adj->page_size, adj->page_increment,
						     adj->page_size));
	gtk_scrolled_window_set_hadjustment (scrolled_window, adj_new);
}

void
gal_view_minicard_attach (GalViewMinicard *view, EABView *address_view)
{
	EMinicardViewWidget *emvw = E_MINICARD_VIEW_WIDGET (address_view->object);
	gal_view_minicard_detach (view);

	view->emvw = emvw;

	g_object_ref (view->emvw);

	g_object_set (view->emvw,
		      "column_width", view->column_width,
		      NULL);

	view->emvw_column_width_changed_id =
		g_signal_connect(view->emvw, "column_width_changed",
				 G_CALLBACK (column_width_changed), address_view);
}

void
gal_view_minicard_detach (GalViewMinicard *view)
{
	if (view->emvw == NULL)
		return;
	if (view->emvw_column_width_changed_id) {
		g_signal_handler_disconnect (view->emvw,
					     view->emvw_column_width_changed_id);
		view->emvw_column_width_changed_id = 0;
	}
	g_object_unref (view->emvw);
	view->emvw = NULL;
}
