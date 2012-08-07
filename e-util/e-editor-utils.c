/*
 * e-editor-utils.c
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

#include "e-editor-utils.h"


WebKitDOMElement *
e_editor_dom_node_get_parent_element (WebKitDOMNode *node,
				      GType parent_type)
{
	while (node) {

		if (G_TYPE_CHECK_INSTANCE_TYPE (node, parent_type))
			return (WebKitDOMElement *) node;

		node = (WebKitDOMNode *) webkit_dom_node_get_parent_element (node);
	}

	return NULL;
}

WebKitDOMElement *
e_editor_dom_node_find_child_element (WebKitDOMNode *node,
				      GType child_type)
{
	WebKitDOMElement *link;
	WebKitDOMNode *start_node = node;

	do {
		if (G_TYPE_CHECK_INSTANCE_TYPE (node, child_type)) {
			return WEBKIT_DOM_ELEMENT (node);
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
