/*
 * e-html-editor-image-dialog-dom-functions.c
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

#include "e-html-editor-image-dialog-dom-functions.h"

#include "e-dom-utils.h"

static WebKitDOMElement *
get_current_image_element (WebKitDOMDocument *document)
{
	return webkit_dom_document_get_element_by_id (document, "-x-evo-current-img");
}

void
e_html_editor_image_dialog_set_element_url (WebKitDOMDocument *document,
                                            const gchar *url)
{
	WebKitDOMElement *image, *link;

	image = get_current_image_element (document);
	link = e_html_editor_dom_node_find_parent_element (
		WEBKIT_DOM_NODE (image), "A");

	if (link) {
		if (!url || !*url) {
			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (
					WEBKIT_DOM_NODE (link)),
				WEBKIT_DOM_NODE (image),
				WEBKIT_DOM_NODE (link), NULL);
			webkit_dom_node_remove_child (
				webkit_dom_node_get_parent_node (
					WEBKIT_DOM_NODE (link)),
				WEBKIT_DOM_NODE (link), NULL);
		} else {
			webkit_dom_html_anchor_element_set_href (
				WEBKIT_DOM_HTML_ANCHOR_ELEMENT (link), url);
		}
	} else {
		if (url && *url) {
			WebKitDOMDocument *document;

			document = webkit_dom_node_get_owner_document (
					WEBKIT_DOM_NODE (image));
			link = webkit_dom_document_create_element (
					document, "A", NULL);

			webkit_dom_html_anchor_element_set_href (
				WEBKIT_DOM_HTML_ANCHOR_ELEMENT (link), url);

			webkit_dom_node_insert_before (
				webkit_dom_node_get_parent_node (
					WEBKIT_DOM_NODE (image)),
				WEBKIT_DOM_NODE (link),
				WEBKIT_DOM_NODE (image), NULL);

			webkit_dom_node_append_child (
				WEBKIT_DOM_NODE (link),
				WEBKIT_DOM_NODE (image), NULL);
		}
	}
}

gchar *
e_html_editor_image_dialog_get_element_url (WebKitDOMDocument *document)
{
	gchar *value;
	WebKitDOMElement *image, *link;

	image = get_current_image_element (document);
	link = e_html_editor_dom_node_find_parent_element (
		WEBKIT_DOM_NODE (image), "A");

	value = webkit_dom_html_anchor_element_get_href (
		WEBKIT_DOM_HTML_ANCHOR_ELEMENT (link));

	return value;
}
