/*
 * e-html-editor-view-dom-functions.h
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

#ifndef E_HTML_EDITOR_VIEW_DOM_FUNCTIONS_H
#define E_HTML_EDITOR_VIEW_DOM_FUNCTIONS_H

#include <webkitdom/webkitdom.h>

#include "web-extensions/e-html-editor-web-extension.h"

#include "e-util-enums.h"

G_BEGIN_DECLS

gboolean	dom_exec_command		(WebKitDOMDocument *document,
						 EHTMLEditorViewCommand command,
						 const gchar *value);

void		dom_force_spell_check_for_current_paragraph
						(WebKitDOMDocument *document);

void		dom_force_spell_check		(WebKitDOMDocument *document);

void		dom_convert_and_insert_html_into_selection
						(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 const gchar *html,
						 gboolean is_html);

WebKitDOMElement *
		dom_quote_plain_text_element	(WebKitDOMDocument *document,
						 WebKitDOMElement *element);
G_END_DECLS

#endif /* E_HTML_EDITOR_VIEW_DOM_FUNCTIONS_H */
