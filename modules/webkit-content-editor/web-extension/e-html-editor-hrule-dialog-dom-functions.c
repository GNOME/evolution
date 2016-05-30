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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define WEBKIT_DOM_USE_UNSTABLE_API
#include <webkitdom/WebKitDOMDOMSelection.h>
#include <webkitdom/WebKitDOMDOMWindowUnstable.h>

#include <web-extensions/e-dom-utils.h>

#include "e-html-editor-selection-dom-functions.h"
#include "e-html-editor-web-extension.h"
#include "e-html-editor-undo-redo-manager.h"

#include "e-html-editor-hrule-dialog-dom-functions.h"

static WebKitDOMElement *
get_current_hrule_element (WebKitDOMDocument *document)
{
	return webkit_dom_document_get_element_by_id (document, "-x-evo-current-hr");
}

gboolean
e_html_editor_hrule_dialog_find_hrule (WebKitDOMDocument *document,
                                       EHTMLEditorWebExtension *extension,
                                       WebKitDOMNode *node_under_mouse_click)
{
	EHTMLEditorUndoRedoManager *manager;
	gboolean created = FALSE;
	WebKitDOMElement *rule;

	if (node_under_mouse_click && WEBKIT_DOM_IS_HTML_HR_ELEMENT (node_under_mouse_click)) {
		rule = WEBKIT_DOM_ELEMENT (node_under_mouse_click);
		webkit_dom_element_set_id (rule, "-x-evo-current-hr");
	} else {
		WebKitDOMElement *selection_start, *parent;

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

		created = TRUE;
	}

	manager = e_html_editor_web_extension_get_undo_redo_manager (extension);
	if (!e_html_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		EHTMLEditorHistoryEvent *ev;

		ev = g_new0 (EHTMLEditorHistoryEvent, 1);
		ev->type = HISTORY_HRULE_DIALOG;

		dom_selection_get_coordinates (
			document, &ev->before.start.x, &ev->before.start.y, &ev->before.end.x, &ev->before.end.y);
		if (!created)
			ev->data.dom.from = webkit_dom_node_clone_node_with_error (
				WEBKIT_DOM_NODE (rule), FALSE, NULL);
		else
			ev->data.dom.from = NULL;

		e_html_editor_undo_redo_manager_insert_history_event (manager, ev);
	}

	return created;
}

void
e_html_editor_hrule_dialog_save_history_on_exit (WebKitDOMDocument *document,
                                                 EHTMLEditorWebExtension *extension)
{
	EHTMLEditorUndoRedoManager *manager;
	EHTMLEditorHistoryEvent *ev = NULL;
	WebKitDOMElement *element;

	element = get_current_hrule_element (document);
	g_return_if_fail (element != NULL);

	webkit_dom_element_remove_attribute (element, "id");

	manager = e_html_editor_web_extension_get_undo_redo_manager (extension);
	ev = e_html_editor_undo_redo_manager_get_current_history_event (manager);
	ev->data.dom.to = webkit_dom_node_clone_node_with_error (
		WEBKIT_DOM_NODE (element), TRUE, NULL);

	if (!webkit_dom_node_is_equal_node (ev->data.dom.from, ev->data.dom.to)) {
		dom_selection_get_coordinates (
			document, &ev->after.start.x, &ev->after.start.y, &ev->after.end.x, &ev->after.end.y);
	} else {
		e_html_editor_undo_redo_manager_remove_current_history_event (manager);
	}
}
