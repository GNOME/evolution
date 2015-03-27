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

#include "e-dom-utils.h"
#include "e-html-editor-selection-dom-functions.h"

#define WEBKIT_DOM_USE_UNSTABLE_API
#include <webkitdom/WebKitDOMDOMSelection.h>
#include <webkitdom/WebKitDOMDOMWindowUnstable.h>

gboolean
e_html_editor_hrule_dialog_find_hrule (WebKitDOMDocument *document,
                                       EHTMLEditorWebExtension *extension,
                                       WebKitDOMNode *node_under_mouse_click)
{
	if (node_under_mouse_click && WEBKIT_DOM_IS_HTML_HR_ELEMENT (node_under_mouse_click)) {
		webkit_dom_element_set_id (WEBKIT_DOM_ELEMENT (node_under_mouse_click), "-x-evo-current-hr");

		return FALSE;
	} else {
		WebKitDOMElement *selection_start, *parent, *rule;

		dom_selection_save (document);

		selection_start = webkit_dom_document_get_element_by_id (
			document, "-x-evo-selection-start-marker");
		parent = get_parent_block_element (WEBKIT_DOM_NODE (selection_start));

		rule = webkit_dom_document_create_element (document, "HR", NULL);
		webkit_dom_element_set_id (rule, "-x-evo-current-hr");

		/* Insert horizontal rule into body below the caret */
		webkit_dom_node_insert_before (
			webkit_dom_node_get_parent_node (WEBKIT_DOM_NODE (parent)),
			WEBKIT_DOM_NODE (rule),
			webkit_dom_node_get_next_sibling (WEBKIT_DOM_NODE (parent)),
			NULL);

		dom_selection_restore (document);

		e_html_editor_web_extension_set_content_changed (extension);
	}
	return TRUE;
}
