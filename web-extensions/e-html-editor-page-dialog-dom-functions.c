/*
 * e-html-editor-page-dialog-dom-functions.c
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

#include "e-html-editor-page-dialog-dom-functions.h"

#include "e-dom-utils.h"
#include "e-html-editor-selection-dom-functions.h"
#include "e-html-editor-web-extension.h"

void
e_html_editor_page_dialog_save_history (WebKitDOMDocument *document,
                                        EHTMLEditorWebExtension *extension)
{
	EHTMLEditorUndoRedoManager *manager;

	manager = e_html_editor_web_extension_get_undo_redo_manager (extension);
	if (!e_html_editor_undo_redo_manager_is_operation_in_progress (manager)) {
		EHTMLEditorHistoryEvent *ev;
		WebKitDOMHTMLElement *body;

		ev = g_new0 (EHTMLEditorHistoryEvent, 1);
		ev->type = HISTORY_PAGE_DIALOG;

		dom_selection_get_coordinates (
			document, &ev->before.start.x, &ev->before.start.y, &ev->before.end.x, &ev->before.end.y);
		body = webkit_dom_document_get_body (document);
		ev->data.dom.from = webkit_dom_node_clone_node (WEBKIT_DOM_NODE (body), FALSE);

		e_html_editor_undo_redo_manager_insert_history_event (manager, ev);
	}
}

void
e_html_editor_page_dialog_save_history_on_exit (WebKitDOMDocument *document,
                                                EHTMLEditorWebExtension *extension)
{
	EHTMLEditorHistoryEvent *ev = NULL;
	EHTMLEditorUndoRedoManager *manager;
	WebKitDOMHTMLElement *body;

	manager = e_html_editor_web_extension_get_undo_redo_manager (extension);
	ev = e_html_editor_undo_redo_manager_get_current_history_event (manager);
	body = webkit_dom_document_get_body (document);
	ev->data.dom.to = webkit_dom_node_clone_node (WEBKIT_DOM_NODE (body), FALSE);

	dom_selection_get_coordinates (
		document, &ev->after.start.x, &ev->after.start.y, &ev->after.end.x, &ev->after.end.y);
}
