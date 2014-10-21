/*
 * e-html-editor-selection-dom-functions.h
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

#ifndef E_HTML_EDITOR_SELECTION_DOM_FUNCTIONS_H
#define E_HTML_EDITOR_SELECTION_DOM_FUNCTIONS_H

#include <webkitdom/webkitdom.h>

G_BEGIN_DECLS

void		e_html_editor_selection_dom_replace_base64_image_src
						(WebKitDOMDocument *document,
						 const gchar *selector,
						 const gchar *base64_content,
						 const gchar *filename,
						 const gchar *uri);

void		e_html_editor_selection_dom_clear_caret_position_marker
						(WebKitDOMDocument *document);
/*
WebKitDOMNode *
		e_html_editor_selection_dom_get_caret_position_node
						(WebKitDOMDocument *document);

WebKitDOMRange *
		e_html_editor_selection_dom_get_current_range
						(WebKitDOMDocument *document);
*/
WebKitDOMElement *
		e_html_editor_selection_dom_save_caret_position
						(WebKitDOMDocument *document);
/*
void
		e_html_editor_selection_dom_move_caret_into_element
						(WebKitDOMDocument *document,
						 WebKitDOMElement *element);
*/
void		e_html_editor_selection_dom_restore_caret_position
						(WebKitDOMDocument *document);

void		e_html_editor_selection_dom_unlink
						(WebKitDOMDocument *document);

void		e_html_editor_selection_dom_create_link
						(WebKitDOMDocument *document,
						 const gchar *uri);

void		e_html_editor_selection_dom_save
						(WebKitDOMDocument *document);

void		e_html_editor_selection_dom_restore
						(WebKitDOMDocument *document);

G_END_DECLS

#endif /* E_HTML_EDITOR_SELECTION_DOM_FUNCTIONS_H */
