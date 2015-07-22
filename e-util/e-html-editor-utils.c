/*
 * e-html-editor-utils.c
 *
 * Copyright (C) 2012 Dan Vrátil <dvratil@redhat.com>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-html-editor-utils.h"
#include "e-web-view.h"
#include <string.h>

/**
 * e_html_editor_dom_node_find_parent_element:
 * @node: Start node
 * @tagname: Tag name of element to search
 *
 * Recursively searches for first occurance of element with given @tagname
 * that is parent of given @node.
 *
 * Returns: A #WebKitDOMElement with @tagname representing parent of @node or
 * @NULL when @node has no parent with given @tagname. When @node matches @tagname,
 * then the @node is returned.
 */
WebKitDOMElement *
e_html_editor_dom_node_find_parent_element (WebKitDOMNode *node,
                                            const gchar *tagname)
{
	gint taglen = strlen (tagname);

	while (node) {

		if (WEBKIT_DOM_IS_ELEMENT (node)) {
			gchar *node_tagname;

			node_tagname = webkit_dom_element_get_tag_name (
						WEBKIT_DOM_ELEMENT (node));

			if (node_tagname &&
			    (strlen (node_tagname) == taglen) &&
			    (g_ascii_strncasecmp (node_tagname, tagname, taglen) == 0)) {
				g_free (node_tagname);
				return WEBKIT_DOM_ELEMENT (node);
			}

			g_free (node_tagname);
		}

		node = WEBKIT_DOM_NODE (webkit_dom_node_get_parent_element (node));
	}

	return NULL;
}

/**
 * e_html_editor_dom_node_find_child_element:
 * @node: Start node
 * @tagname: Tag name of element to search.
 *
 * Recursively searches for first occurence of element with given @tagname that
 * is a child of @node.
 *
 * Returns: A #WebKitDOMElement with @tagname representing a child of @node or
 * @NULL when @node has no child with given @tagname. When @node matches @tagname,
 * then the @node is returned.
 */
WebKitDOMElement *
e_html_editor_dom_node_find_child_element (WebKitDOMNode *node,
                                           const gchar *tagname)
{
	WebKitDOMNode *start_node = node;
	gint taglen = strlen (tagname);

	do {
		if (WEBKIT_DOM_IS_ELEMENT (node)) {
			gchar *node_tagname;

			node_tagname = webkit_dom_element_get_tag_name (
					WEBKIT_DOM_ELEMENT (node));

			if (node_tagname &&
			    (strlen (node_tagname) == taglen) &&
			    (g_ascii_strncasecmp (node_tagname, tagname, taglen) == 0)) {
				g_free (node_tagname);
				return WEBKIT_DOM_ELEMENT (node);
			}

			g_free (node_tagname);
		}

		if (webkit_dom_node_has_child_nodes (node)) {
			node = webkit_dom_node_get_first_child (node);
		} else if (webkit_dom_node_get_next_sibling (node)) {
			node = webkit_dom_node_get_next_sibling (node);
		} else {
			node = webkit_dom_node_get_parent_node (node);
		}
	} while (!webkit_dom_node_is_same_node (node, start_node));

	return NULL;
}

gboolean
e_html_editor_node_is_selection_position_node (WebKitDOMNode *node)
{
	WebKitDOMElement *element;

	if (!node || !WEBKIT_DOM_IS_ELEMENT (node))
		return FALSE;

	element = WEBKIT_DOM_ELEMENT (node);

	return element_has_id (element, "-x-evo-selection-start-marker") ||
	       element_has_id (element, "-x-evo-selection-end-marker");
}

WebKitDOMNode *
e_html_editor_get_parent_block_node_from_child (WebKitDOMNode *node)
{
	WebKitDOMNode *parent = node;

	if (!WEBKIT_DOM_IS_ELEMENT (parent) ||
	    e_html_editor_node_is_selection_position_node (parent))
		parent = webkit_dom_node_get_parent_node (parent);

	if (element_has_class (WEBKIT_DOM_ELEMENT (parent), "-x-evo-temp-text-wrapper") ||
	    element_has_class (WEBKIT_DOM_ELEMENT (parent), "-x-evo-quoted") ||
	    element_has_class (WEBKIT_DOM_ELEMENT (parent), "-x-evo-quote-character") ||
	    element_has_class (WEBKIT_DOM_ELEMENT (parent), "-x-evo-signature") ||
	    WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (parent) ||
	    element_has_tag (WEBKIT_DOM_ELEMENT (parent), "b") ||
	    element_has_tag (WEBKIT_DOM_ELEMENT (parent), "i") ||
	    element_has_tag (WEBKIT_DOM_ELEMENT (parent), "u"))
		parent = webkit_dom_node_get_parent_node (parent);

	if (element_has_class (WEBKIT_DOM_ELEMENT (parent), "-x-evo-quoted"))
		parent = webkit_dom_node_get_parent_node (parent);

	return parent;
}
