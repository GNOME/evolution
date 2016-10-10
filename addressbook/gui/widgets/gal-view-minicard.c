/*
 *
 * gal-view-minicard.c: An Minicard View
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <libxml/parser.h>

#include "gal-view-minicard.h"

G_DEFINE_TYPE (
	GalViewMinicard,
	gal_view_minicard,
	GAL_TYPE_VIEW)

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

	gal_view_minicard_detach (view);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (gal_view_minicard_parent_class)->finalize (object);
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
		root, (guchar *) "column_width", 225);
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

static GalView *
view_minicard_clone (GalView *view)
{
	GalViewMinicard *view_minicard;
	GalView *clone;

	/* Chain up to parent's clone() method. */
	clone = GAL_VIEW_CLASS (gal_view_minicard_parent_class)->clone (view);

	view_minicard = GAL_VIEW_MINICARD (view);
	GAL_VIEW_MINICARD (clone)->column_width = view_minicard->column_width;

	return clone;
}

static void
gal_view_minicard_class_init (GalViewMinicardClass *class)
{
	GObjectClass *object_class;
	GalViewClass *gal_view_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = view_minicard_finalize;

	gal_view_class = GAL_VIEW_CLASS (class);
	gal_view_class->type_code = "minicard";
	gal_view_class->load = view_minicard_load;
	gal_view_class->save = view_minicard_save;
	gal_view_class->clone = view_minicard_clone;

}

static void
gal_view_minicard_init (GalViewMinicard *gvm)
{
	gvm->column_width = 225.0;

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
	return g_object_new (GAL_TYPE_VIEW_MINICARD, "title", title, NULL);
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
