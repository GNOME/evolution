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
#include <e-util/e-xml-utils.h>
#include <libedataserver/e-xml-utils.h>

#include "gal-view-minicard.h"

static gpointer parent_class;

static void
view_minicard_column_width_changed (EAddressbookView *address_view,
                                    gdouble width)
{
	GalView *view;
	GalViewInstance *view_instance;
	GalViewMinicard *view_minicard;

	view_instance = e_addressbook_view_get_view_instance (address_view);
	view = gal_view_instance_get_current_view (view_instance);
	view_minicard = GAL_VIEW_MINICARD (view);

	if (view_minicard->column_width != width) {
		view_minicard->column_width = width;
		gal_view_changed (view);
	}
}

static void
view_minicard_finalize (GObject *object)
{
	GalViewMinicard *view = GAL_VIEW_MINICARD (object);

	if (view->title != NULL) {
		gal_view_minicard_detach (view);
		g_free (view->title);
		view->title = NULL;
	}

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
view_minicard_load (GalView *view,
                    const gchar *filename)
{
	GalViewMinicard *view_minicard;
	xmlDoc *doc;
	xmlNode *root;

	view_minicard = GAL_VIEW_MINICARD (view);

	doc = e_xml_parse_file (filename);
	g_return_if_fail (doc != NULL);

	root = xmlDocGetRootElement (doc);
	view_minicard->column_width =
		e_xml_get_double_prop_by_name_with_default (
		root, (guchar *) "column_width", 150);
	xmlFreeDoc (doc);
}

static void
view_minicard_save (GalView *view,
                    const gchar *filename)
{
	GalViewMinicard *view_minicard;
	xmlDoc *doc;
	xmlNode *root;

	view_minicard = GAL_VIEW_MINICARD (view);

	doc = xmlNewDoc ((guchar *) "1.0");
	root = xmlNewNode (NULL, (guchar *) "EMinicardViewState");
	e_xml_set_double_prop_by_name (
		root, (guchar *) "column_width",
		view_minicard->column_width);
	xmlDocSetRootElement (doc, root);
	e_xml_save_file (filename, doc);
	xmlFreeDoc (doc);
}

static const gchar *
view_minicard_get_title (GalView *view)
{
	GalViewMinicard *view_minicard;

	view_minicard = GAL_VIEW_MINICARD (view);

	return view_minicard->title;
}

static void
view_minicard_set_title (GalView *view,
                         const gchar *title)
{
	GalViewMinicard *view_minicard;

	view_minicard = GAL_VIEW_MINICARD (view);

	g_free (view_minicard->title);
	view_minicard->title = g_strdup (title);
}

static const gchar *
view_minicard_get_type_code (GalView *view)
{
	return "minicard";
}

static GalView *
view_minicard_clone (GalView *view)
{
	GalViewMinicard *view_minicard;
	GalViewMinicard *clone;

	view_minicard = GAL_VIEW_MINICARD(view);

	clone = g_object_new (GAL_TYPE_VIEW_MINICARD, NULL);
	clone->column_width = view_minicard->column_width;
	clone->title = g_strdup (view_minicard->title);

	return GAL_VIEW (clone);
}

static void
gal_view_minicard_class_init (GalViewMinicardClass *class)
{
	GObjectClass *object_class;
	GalViewClass *gal_view_class;

	parent_class = g_type_class_peek_parent (class);

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = view_minicard_finalize;

	gal_view_class = GAL_VIEW_CLASS (class);
	gal_view_class->edit = NULL;
	gal_view_class->load = view_minicard_load;
	gal_view_class->save = view_minicard_save;
	gal_view_class->get_title = view_minicard_get_title;
	gal_view_class->set_title = view_minicard_set_title;
	gal_view_class->get_type_code = view_minicard_get_type_code;
	gal_view_class->clone = view_minicard_clone;

}

static void
gal_view_minicard_init (GalViewMinicard *gvm)
{
	gvm->title = NULL;
	gvm->column_width = 150.0;

	gvm->emvw = NULL;
	gvm->emvw_column_width_changed_id = 0;
}

GType
gal_view_minicard_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (GalViewMinicardClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gal_view_minicard_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (GalViewMinicard),
			0,     /* n_preallocs */
			(GInstanceInitFunc) gal_view_minicard_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			GAL_TYPE_VIEW, "GalViewMinicard", &type_info, 0);
	}

	return type;
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
	return gal_view_minicard_construct (
		g_object_new (GAL_TYPE_VIEW_MINICARD, NULL), title);
}

/**
 * gal_view_minicard_construct
 * @view: The view to construct.
 * @title: The name of the new view.
 *
 * Constructs the GalViewMinicard.  To be used by subclasses and
 * language bindings.
 *
 * Returns: The GalViewMinicard.
 */
GalView *
gal_view_minicard_construct  (GalViewMinicard *view,
			      const gchar *title)
{
	view->title = g_strdup (title);

	return GAL_VIEW (view);
}

void
gal_view_minicard_attach (GalViewMinicard *view,
                          EAddressbookView *address_view)
{
	GObject *object;

	g_return_if_fail (GAL_IS_VIEW_MINICARD (view));
	g_return_if_fail (E_IS_ADDRESSBOOK_VIEW (address_view));

	object = e_addressbook_view_get_view_object (address_view);
	g_return_if_fail (E_IS_MINICARD_VIEW_WIDGET (object));

	gal_view_minicard_detach (view);
	view->emvw = g_object_ref (object);

	g_object_set (view->emvw, "column-width", view->column_width, NULL);

	view->emvw_column_width_changed_id =
		g_signal_connect_swapped (
			view->emvw, "column-width-changed",
			G_CALLBACK (view_minicard_column_width_changed),
			address_view);
}

void
gal_view_minicard_detach (GalViewMinicard *view)
{
	g_return_if_fail (GAL_IS_VIEW_MINICARD (view));

	if (view->emvw == NULL)
		return;

	if (view->emvw_column_width_changed_id > 0) {
		g_signal_handler_disconnect (
			view->emvw, view->emvw_column_width_changed_id);
		view->emvw_column_width_changed_id = 0;
	}

	g_object_unref (view->emvw);
	view->emvw = NULL;
}
