/*
 * e-html-editor-hrule-dialog-dom-functions.c
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

#include "e-html-editor-hrule-dialog-dom-functions.h"

#include "e-html-editor-selection-dom-functions.h"

#define WEBKIT_DOM_USE_UNSTABLE_API
#include <webkitdom/WebKitDOMDOMSelection.h>
#include <webkitdom/WebKitDOMDOMWindowUnstable.h>

gboolean
e_html_editor_hrule_dialog_find_hrule (WebKitDOMDocument *document)
{
	gboolean found = TRUE;
	WebKitDOMDOMWindow *window;
	WebKitDOMDOMSelection *selection;
	WebKitDOMElement *rule = NULL;

	window = webkit_dom_document_get_default_view (document);
	selection = webkit_dom_dom_window_get_selection (window);
	if (webkit_dom_dom_selection_get_range_count (selection) < 1)
		return FALSE;
/* FIXME WK2
	rule = e_html_editor_view_get_element_under_mouse_click (view); */
	if (!rule) {
		WebKitDOMElement *caret, *parent, *element;

		caret = e_html_editor_selection_dom_save_caret_position (document);
		parent = webkit_dom_node_get_parent_element (WEBKIT_DOM_NODE (caret));
		element = caret;

		while (!WEBKIT_DOM_IS_HTML_BODY_ELEMENT (parent)) {
			element = parent;
			parent = webkit_dom_node_get_parent_element (
				WEBKIT_DOM_NODE (parent));
		}

		rule = webkit_dom_document_create_element (document, "HR", NULL);

		/* Insert horizontal rule into body below the caret */
		webkit_dom_node_insert_before (
			WEBKIT_DOM_NODE (parent),
			WEBKIT_DOM_NODE (rule),
			webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (element)),
			NULL);

		e_html_editor_selection_dom_clear_caret_position_marker (document);

		found = FALSE;
	}

	webkit_dom_element_set_id (rule, "-x-evo-current-hr");

	return found;
}
