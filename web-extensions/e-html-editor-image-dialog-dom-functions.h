/*
 * e-html-editor-image-dialog-dom-functions.h
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

#ifndef E_HTML_EDITOR_IMAGE_DIALOG_DOM_FUNCTIONS_H
#define E_HTML_EDITOR_IMAGE_DIALOG_DOM_FUNCTIONS_H

#include <webkitdom/webkitdom.h>

#include "e-html-editor-web-extension.h"

G_BEGIN_DECLS

void		e_html_editor_image_dialog_mark_image
						(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 WebKitDOMNode *node_under_mouse_click);

void		e_html_editor_image_dialog_save_history_on_exit
						(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);

void		e_html_editor_image_dialog_set_element_url
						(WebKitDOMDocument *document,
						 const gchar *url);

gchar *		e_html_editor_image_dialog_get_element_url
						(WebKitDOMDocument *document);

G_END_DECLS

#endif /* E_HTML_EDITOR_IMAGE_DIALOG_DOM_FUNCTIONS_H */
