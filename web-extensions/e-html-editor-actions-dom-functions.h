/*
 * e-html-editor-actions-dom-functions.h
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

#ifndef E_HTML_EDITOR_ACTIONS_DOM_FUNCTIONS_H
#define E_HTML_EDITOR_ACTIONS_DOM_FUNCTIONS_H

#include <webkitdom/webkitdom.h>

#include "e-html-editor-web-extension.h"

G_BEGIN_DECLS

void		e_html_editor_dialog_delete_cell_contents
						(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);

void		e_html_editor_dialog_delete_column
						(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);

void		e_html_editor_dialog_delete_row
						(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);

void		e_html_editor_dialog_delete_table
						(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);

void		e_html_editor_dialog_insert_column_after
						(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);

void		e_html_editor_dialog_insert_column_before
						(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);

void		e_html_editor_dialog_insert_row_above
						(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);

void		e_html_editor_dialog_insert_row_below
						(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);

void		dom_save_history_for_cut	(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);
G_END_DECLS

#endif /* E_HTML_EDITOR_ACTIONS_DOM_FUNCTIONS_H */
