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
#include <string.h>

WebKitDOMElement *
e_editor_dom_node_find_parent_element (WebKitDOMNode *node,
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
				return (WebKitDOMElement *) node;
			}

			g_free (node_tagname);
		}

		node = (WebKitDOMNode *) webkit_dom_node_get_parent_element (node);
	}

	return NULL;
}

WebKitDOMElement *
e_editor_dom_node_find_child_element (WebKitDOMNode *node,
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
				return (WebKitDOMElement *) node;
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
