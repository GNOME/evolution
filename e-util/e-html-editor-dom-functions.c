/*
 * e-html-editor-dom-functions.c
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
 */

#include "e-html-editor-dom-functions.h"
#include "e-html-editor-defines.h"

#include "e-dom-utils.h"

guint
get_e_html_editor_flags_for_element_on_coordinates (WebKitDOMDocument *document,
                                                    gint32 x,
                                                    gint32 y)
{
	guint flags = 0;
	WebKitDOMElement *element, *tmp;

	tmp = webkit_dom_document_get_element_by_id (document, "-x-evo-current-image");
	if (tmp)
		webkit_dom_element_remove_attribute (tmp, "id");
	tmp = webkit_dom_document_get_element_by_id (document, "-x-evo-table-cell");
	if (tmp)
		webkit_dom_element_remove_attribute (tmp, "id");

	element = e_dom_utils_get_element_from_point (document, x, y);
	if (!element) {
		flags |= E_HTML_EDITOR_NODE_IS_TEXT;
		return flags;
	}

	if (WEBKIT_DOM_IS_HTML_HR_ELEMENT (element))
		flags |= E_HTML_EDITOR_NODE_IS_HR;

	if (WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (element) ||
	    (dom_node_find_parent_element (WEBKIT_DOM_NODE (element), "A") != NULL))
		flags |= E_HTML_EDITOR_NODE_IS_ANCHOR;

	if (WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT (element) ||
	    (dom_node_find_parent_element (WEBKIT_DOM_NODE (element), "IMG") != NULL)) {

		flags |= E_HTML_EDITOR_NODE_IS_IMAGE;

		webkit_dom_element_set_id (element, "-x-evo-current-image");
	}

	if (WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT (element) ||
	    (dom_node_find_parent_element (WEBKIT_DOM_NODE (element), "TD") != NULL) ||
	    (dom_node_find_parent_element (WEBKIT_DOM_NODE (element), "TH") != NULL)) {

		flags |= E_HTML_EDITOR_NODE_IS_TABLE_CELL;

		webkit_dom_element_set_id (element, "-x-evo-table-cell");
	}

	if (flags && E_HTML_EDITOR_NODE_IS_TABLE_CELL &&
	    (WEBKIT_DOM_IS_HTML_TABLE_ELEMENT (element) ||
	    (dom_node_find_parent_element (WEBKIT_DOM_NODE (element), "TABLE") != NULL));

		flags |= E_HTML_EDITOR_NODE_IS_TABLE;
	}

	if (flags == 0)
		flags |= E_HTML_EDITOR_NODE_IS_TEXT;

	return flags;
}
