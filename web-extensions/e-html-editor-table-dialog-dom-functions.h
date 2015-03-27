/*
 * e-html-editor-table-dialog-dom-functions.h
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

#ifndef E_HTML_EDITOR_TABLE_DIALOG_DOM_FUNCTIONS_H
#define E_HTML_EDITOR_TABLE_DIALOG_DOM_FUNCTIONS_H

#include <webkitdom/webkitdom.h>

#include "e-html-editor-web-extension.h"

G_BEGIN_DECLS

void		e_html_editor_table_dialog_set_row_count
						(WebKitDOMDocument *document,
						 gulong expected_count);

gulong		e_html_editor_table_dialog_get_row_count
						(WebKitDOMDocument *document);

void		e_html_editor_table_dialog_set_column_count
						(WebKitDOMDocument *document,
						 gulong expected_columns);

gulong		e_html_editor_table_dialog_get_column_count
						(WebKitDOMDocument *document);

gboolean	e_html_editor_table_dialog_show (WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);
G_END_DECLS

#endif /* E_HTML_EDITOR_TABLE_DIALOG_DOM_FUNCTIONS_H */
