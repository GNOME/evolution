/*
 *
 * gal-view-minicard.c: A Minicard View
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

#include "e-card-view.h"
#include "gal-view-minicard.h"

struct _GalViewMinicard {
	GalView parent;

	GWeakRef content_object_ref;
	gdouble column_width;
	ECardsSortBy sort_by;
};

G_DEFINE_TYPE (GalViewMinicard, gal_view_minicard, GAL_TYPE_VIEW)

static void
view_minicard_update_sort_fields (GalViewMinicard *self)
{
	ECardView *card_view;

	card_view = g_weak_ref_get (&self->content_object_ref);

	if (!card_view)
		return;

	if (self->sort_by == E_CARDS_SORT_BY_GIVEN_NAME) {
		EBookClientViewSortFields sort_fields[] = {
			{ E_CONTACT_GIVEN_NAME, E_BOOK_CURSOR_SORT_ASCENDING },
			{ E_CONTACT_FAMILY_NAME, E_BOOK_CURSOR_SORT_ASCENDING },
			{ E_CONTACT_FILE_AS, E_BOOK_CURSOR_SORT_ASCENDING },
			{ E_CONTACT_FIELD_LAST, E_BOOK_CURSOR_SORT_ASCENDING }
		};

		e_card_view_set_sort_fields (card_view, sort_fields);
	} else if (self->sort_by == E_CARDS_SORT_BY_FAMILY_NAME) {
		EBookClientViewSortFields sort_fields[] = {
			{ E_CONTACT_FAMILY_NAME, E_BOOK_CURSOR_SORT_ASCENDING },
			{ E_CONTACT_GIVEN_NAME, E_BOOK_CURSOR_SORT_ASCENDING },
			{ E_CONTACT_FILE_AS, E_BOOK_CURSOR_SORT_ASCENDING },
			{ E_CONTACT_FIELD_LAST, E_BOOK_CURSOR_SORT_ASCENDING }
		};

		e_card_view_set_sort_fields (card_view, sort_fields);
	} else /* if (self->sort_by == E_CARDS_SORT_BY_FILE_AS) */ {
		EBookClientViewSortFields sort_fields[] = {
			{ E_CONTACT_FILE_AS, E_BOOK_CURSOR_SORT_ASCENDING },
			{ E_CONTACT_FAMILY_NAME, E_BOOK_CURSOR_SORT_ASCENDING },
			{ E_CONTACT_GIVEN_NAME, E_BOOK_CURSOR_SORT_ASCENDING },
			{ E_CONTACT_FIELD_LAST, E_BOOK_CURSOR_SORT_ASCENDING }
		};

		e_card_view_set_sort_fields (card_view, sort_fields);
	}

	g_object_unref (card_view);
}

static void
view_minicard_finalize (GObject *object)
{
	GalViewMinicard *view = GAL_VIEW_MINICARD (object);

	gal_view_minicard_detach (view);

	g_weak_ref_clear (&view->content_object_ref);

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
	gchar *sort_by;

	view_minicard = GAL_VIEW_MINICARD (view);

	doc = e_xml_parse_file (filename);
	g_return_if_fail (doc != NULL);

	root = xmlDocGetRootElement (doc);
	view_minicard->column_width =
		e_xml_get_double_prop_by_name_with_default (
		root, (guchar *) "column_width", 225);

	sort_by = e_xml_get_string_prop_by_name (root, (const xmlChar *) "sort_by");
	if (g_strcmp0 (sort_by, "given-name") == 0)
		view_minicard->sort_by = E_CARDS_SORT_BY_GIVEN_NAME;
	else if (g_strcmp0 (sort_by, "family-name") == 0)
		view_minicard->sort_by = E_CARDS_SORT_BY_FAMILY_NAME;
	else /*if (g_strcmp0 (sort_by, "file-as") == 0)*/
		view_minicard->sort_by = E_CARDS_SORT_BY_FILE_AS;

	xmlFreeDoc (doc);

	view_minicard_update_sort_fields (view_minicard);
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

	switch (view_minicard->sort_by) {
	default:
	case E_CARDS_SORT_BY_FILE_AS:
		e_xml_set_string_prop_by_name (root, (const xmlChar *) "sort_by", "file-as");
		break;
	case E_CARDS_SORT_BY_GIVEN_NAME:
		e_xml_set_string_prop_by_name (root, (const xmlChar *) "sort_by", "given-name");
		break;
	case E_CARDS_SORT_BY_FAMILY_NAME:
		e_xml_set_string_prop_by_name (root, (const xmlChar *) "sort_by", "family-name");
		break;
	}

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
	GAL_VIEW_MINICARD (clone)->sort_by = view_minicard->sort_by;

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
	/* Left just in case it would be useful in the future, but it's unused now */
	gvm->column_width = 225.0;
	gvm->sort_by = E_CARDS_SORT_BY_FILE_AS;

	g_weak_ref_init (&gvm->content_object_ref, NULL);
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
	GObject *content_object;

	g_return_if_fail (GAL_IS_VIEW_MINICARD (view));
	g_return_if_fail (E_IS_ADDRESSBOOK_VIEW (address_view));

	content_object = e_addressbook_view_get_content_object (address_view);
	g_return_if_fail (E_IS_CARD_VIEW (content_object));

	gal_view_minicard_detach (view);

	g_weak_ref_set (&view->content_object_ref, content_object);

	view_minicard_update_sort_fields (view);
}

void
gal_view_minicard_detach (GalViewMinicard *view)
{
	g_return_if_fail (GAL_IS_VIEW_MINICARD (view));

	g_weak_ref_set (&view->content_object_ref, NULL);
}

ECardsSortBy
gal_view_minicard_get_sort_by (GalViewMinicard *self)
{
	g_return_val_if_fail (GAL_IS_VIEW_MINICARD (self), E_CARDS_SORT_BY_FILE_AS);

	return self->sort_by;
}

void
gal_view_minicard_set_sort_by (GalViewMinicard *self,
			       ECardsSortBy sort_by)
{
	g_return_if_fail (GAL_IS_VIEW_MINICARD (self));

	if (self->sort_by == sort_by)
		return;

	self->sort_by = sort_by;

	view_minicard_update_sort_fields (self);

	gal_view_changed (GAL_VIEW (self));
}
