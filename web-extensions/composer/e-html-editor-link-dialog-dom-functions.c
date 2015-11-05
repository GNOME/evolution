/*
 * e-html-editor-link-dialog-dom-functions.c
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

#include "e-html-editor-link-dialog-dom-functions.h"

#include "e-html-editor-view-dom-functions.h"
#include "e-html-editor-selection-dom-functions.h"

#include <e-util/e-util-enums.h>
#include <web-extensions/e-dom-utils.h>

#define WEBKIT_DOM_USE_UNSTABLE_API
#include <webkitdom/WebKitDOMDOMSelection.h>
#include <webkitdom/WebKitDOMDOMWindowUnstable.h>

void
e_html_editor_link_dialog_ok (WebKitDOMDocument *document,
                              EHTMLEditorWebExtension *extension,
                              const gchar *url,
                              const gchar *inner_text)
{
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMRange *range;
	WebKitDOMElement *link;

	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	g_object_unref (dom_window);

	if (!dom_selection ||
	    (webkit_dom_dom_selection_get_range_count (dom_selection) == 0)) {
		g_object_unref (dom_selection);
		return;
	}

	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
	link = dom_node_find_parent_element (
		webkit_dom_range_get_start_container (range, NULL), "A");
	if (!link) {
		if ((webkit_dom_range_get_start_container (range, NULL) !=
			webkit_dom_range_get_end_container (range, NULL)) ||
		    (webkit_dom_range_get_start_offset (range, NULL) !=
			webkit_dom_range_get_end_offset (range, NULL))) {

			WebKitDOMDocumentFragment *fragment;
			fragment = webkit_dom_range_extract_contents (range, NULL);
			link = dom_node_find_child_element (WEBKIT_DOM_NODE (fragment), "A");
			webkit_dom_range_insert_node (
				range, WEBKIT_DOM_NODE (fragment), NULL);

			webkit_dom_dom_selection_set_base_and_extent (
				dom_selection,
				webkit_dom_range_get_start_container (range, NULL),
				webkit_dom_range_get_start_offset (range, NULL),
				webkit_dom_range_get_end_container (range, NULL),
				webkit_dom_range_get_end_offset (range, NULL),
				NULL);
		} else {
			WebKitDOMNode *node;
			/* get element that was clicked on */
			node = webkit_dom_range_get_common_ancestor_container (range, NULL);
			if (node && !WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (node)) {
				link = dom_node_find_parent_element (node, "A");
				if (link && !WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (link))
					link = NULL;
			} else
				link = WEBKIT_DOM_ELEMENT (node);
		}
	}

	if (link) {
		webkit_dom_html_anchor_element_set_href (
			WEBKIT_DOM_HTML_ANCHOR_ELEMENT (link), url);
		webkit_dom_element_set_inner_html (link, inner_text, NULL);
	} else {
		gchar *text;

		/* Check whether a text is selected or not */
		text = webkit_dom_range_get_text (range);
		if (text && *text) {
			dom_create_link (document, extension, url);
		} else {
			gchar *html = g_strdup_printf (
				"<a href=\"%s\">%s</a>", url, inner_text);

			dom_exec_command (
				document, extension, E_HTML_EDITOR_VIEW_COMMAND_INSERT_HTML, html);
			g_free (html);

		}

		g_free (text);
	}

	g_object_unref (range);
	g_object_unref (dom_selection);
}

GVariant *
e_html_editor_link_dialog_show (WebKitDOMDocument *document)
{
	GVariant *result = NULL;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;
	WebKitDOMRange *range;
	WebKitDOMElement *link;

	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);
	g_object_unref (dom_window);

	/* No selection at all */
	if (!dom_selection ||
	    webkit_dom_dom_selection_get_range_count (dom_selection) < 1) {
		result = g_variant_new ("(ss)", "", "");
		return result;
	}

	range = webkit_dom_dom_selection_get_range_at (dom_selection, 0, NULL);
	link = dom_node_find_parent_element (
		webkit_dom_range_get_start_container (range, NULL), "A");
	if (!link) {
		if ((webkit_dom_range_get_start_container (range, NULL) !=
			webkit_dom_range_get_end_container (range, NULL)) ||
		    (webkit_dom_range_get_start_offset (range, NULL) !=
			webkit_dom_range_get_end_offset (range, NULL))) {

			WebKitDOMDocumentFragment *fragment;
			fragment = webkit_dom_range_clone_contents (range, NULL);
			link = dom_node_find_child_element (WEBKIT_DOM_NODE (fragment), "A");
		} else {
			/* get element that was clicked on */
			WebKitDOMNode *node;

			node = webkit_dom_range_get_common_ancestor_container (range, NULL);
			if (node && !WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (node)) {
				link = dom_node_find_parent_element (node, "A");
				if (link && !WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (link))
					link = NULL;
			} else
				link = WEBKIT_DOM_ELEMENT (node);
		}
	}

	if (link) {
		gchar *href, *text;

		href = webkit_dom_html_anchor_element_get_href (
				WEBKIT_DOM_HTML_ANCHOR_ELEMENT (link));
		text = webkit_dom_html_element_get_inner_text (
				WEBKIT_DOM_HTML_ELEMENT (link));

		result = g_variant_new ("(ss)", href, text);

		g_free (text);
		g_free (href);
	} else {
		gchar *text;

		text = webkit_dom_range_get_text (range);
		if (text && *text)
			result = g_variant_new ("(ss)", "", text);

		g_free (text);
	}

	g_object_unref (range);
	g_object_unref (dom_selection);

	return result;
}
