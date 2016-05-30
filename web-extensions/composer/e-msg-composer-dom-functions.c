/*
 * e-msg-composer-dom-functions.c
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

#include <string.h>

#include "e-msg-composer-dom-functions.h"

gchar *
dom_get_active_signature_uid (WebKitDOMDocument *document)
{
	gchar *uid = NULL;
	WebKitDOMElement *element;

	if ((element = webkit_dom_document_query_selector (document, ".-x-evo-signature[id]", NULL)))
		uid = webkit_dom_element_get_id (element);

	return uid;
}

gchar *
dom_get_raw_body_content_without_signature (WebKitDOMDocument *document)
{
	gulong ii, length;
	WebKitDOMNodeList *list;
	GString* content;

	content = g_string_new (NULL);

	list = webkit_dom_document_query_selector_all (
		document, "body > *:not(.-x-evo-signature-wrapper)", NULL);
	length = webkit_dom_node_list_get_length (list);
	for (ii = 0; ii < length; ii++) {
		WebKitDOMNode *node = webkit_dom_node_list_item (list, ii);

		if (!WEBKIT_DOM_IS_HTML_QUOTE_ELEMENT (node)) {
			gchar *text;

			text = webkit_dom_html_element_get_inner_text (WEBKIT_DOM_HTML_ELEMENT (node));
			g_string_append (content, text);
			g_free (text);

			if (WEBKIT_DOM_IS_HTML_DIV_ELEMENT (node))
				g_string_append (content, "\n");
			else
				g_string_append (content, " ");
		}
	}

	return g_string_free (content, FALSE);
}

gchar *
dom_get_raw_body_content (WebKitDOMDocument *document)
{
	WebKitDOMHTMLElement *body;

	body = webkit_dom_document_get_body (document);

	return  webkit_dom_html_element_get_inner_text (body);
}
