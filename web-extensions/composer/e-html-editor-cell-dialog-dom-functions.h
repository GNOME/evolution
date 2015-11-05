/*
 * e-html-editor-cell-dialog-dom-functions.h
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

#ifndef E_HTML_EDITOR_CELL_DIALOG_DOM_FUNCTIONS_H
#define E_HTML_EDITOR_CELL_DIALOG_DOM_FUNCTIONS_H

#include <webkitdom/webkitdom.h>

#include "e-html-editor-web-extension.h"

G_BEGIN_DECLS

void		e_html_editor_cell_dialog_mark_current_cell_element
						(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 const gchar *id);

void		e_html_editor_cell_dialog_save_history_on_exit
						(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);

void		e_html_editor_cell_dialog_set_element_v_align
						(WebKitDOMDocument *document,
						 const gchar *v_align,
						 guint scope);

void		e_html_editor_cell_dialog_set_element_align
						(WebKitDOMDocument *document,
						 const gchar *align,
						 guint scope);

void		e_html_editor_cell_dialog_set_element_no_wrap
						(WebKitDOMDocument *document,
						 gboolean wrap_text,
						 guint scope);

void		e_html_editor_cell_dialog_set_element_header_style
						(WebKitDOMDocument *document,
						 gboolean header_style,
						 guint scope);

void		e_html_editor_cell_dialog_set_element_width
						(WebKitDOMDocument *document,
						 const gchar *width,
						 guint scope);

void		e_html_editor_cell_dialog_set_element_col_span
						(WebKitDOMDocument *document,
						 glong span,
						 guint scope);

void		e_html_editor_cell_dialog_set_element_row_span
						(WebKitDOMDocument *document,
						 glong span,
						 guint scope);

void		e_html_editor_cell_dialog_set_element_bg_color
						(WebKitDOMDocument *document,
						 const gchar *color,
						 guint scope);

G_END_DECLS

#endif /* E_HTML_EDITOR_CELL_DIALOG_DOM_FUNCTIONS_H */
