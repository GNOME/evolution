/*
 * Copyright (C) 2016 Red Hat, Inc. (www.redhat.com)
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-html-editor-test-dom-functions.h"

gboolean
dom_test_html_equal (WebKitDOMDocument *document,
		     const gchar *html1,
		     const gchar *html2)
{
	WebKitDOMElement *elem1, *elem2;
	gboolean res = FALSE;
	GError *error = NULL;

	g_return_val_if_fail (WEBKIT_DOM_IS_DOCUMENT (document), FALSE);
	g_return_val_if_fail (html1 != NULL, FALSE);
	g_return_val_if_fail (html2 != NULL, FALSE);

	elem1 = webkit_dom_document_create_element (document, "TestHtmlEqual", &error);
	if (error || !elem1) {
		g_warning ("%s: Failed to create elem1: %s", G_STRFUNC, error ? error->message : "Unknown error");
		g_clear_error (&error);
		return FALSE;
	}

	elem2 = webkit_dom_document_create_element (document, "TestHtmlEqual", &error);
	if (error || !elem2) {
		g_warning ("%s: Failed to create elem2: %s", G_STRFUNC, error ? error->message : "Unknown error");
		g_clear_error (&error);
		return FALSE;
	}

	webkit_dom_element_set_inner_html (elem1, html1, &error);
	if (!error) {
		webkit_dom_element_set_inner_html (elem2, html2, &error);

		if (!error) {
			webkit_dom_node_normalize (WEBKIT_DOM_NODE (elem1));
			webkit_dom_node_normalize (WEBKIT_DOM_NODE (elem2));

			res = webkit_dom_node_is_equal_node (WEBKIT_DOM_NODE (elem1), WEBKIT_DOM_NODE (elem2));
		} else {
			g_warning ("%s: Failed to set inner html2: %s", G_STRFUNC, error->message);
		}
	} else {
		g_warning ("%s: Failed to set inner html1: %s", G_STRFUNC, error->message);
	}

	g_clear_error (&error);

	return res;
}
