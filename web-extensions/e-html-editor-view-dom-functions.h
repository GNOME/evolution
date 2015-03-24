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

#include "e-html-editor-web-extension.h"

#include <e-util/e-util-enums.h>

G_BEGIN_DECLS

gboolean	dom_exec_command		(WebKitDOMDocument *document,
						 EHTMLEditorViewCommand command,
						 const gchar *value);

void		dom_force_spell_check_for_current_paragraph
						(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);

void		dom_force_spell_check		(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);

void		dom_turn_spell_check_off	(WebKitDOMDocument *document);

void		dom_embed_style_sheet		(WebKitDOMDocument *document,
						 const gchar *style_sheet_content);

void		dom_remove_embed_style_sheet	(WebKitDOMDocument *document);

void		dom_quote_and_insert_text_into_selection
						(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 const gchar *text);

void		dom_check_magic_links		(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 gboolean include_space_by_user);

void		dom_convert_content		(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 const gchar *preferred_text);

void		dom_convert_and_insert_html_into_selection
						(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 const gchar *html,
						 gboolean is_html);

WebKitDOMElement *
		dom_quote_plain_text_element	(WebKitDOMDocument *document,
						 WebKitDOMElement *element);

void		dom_convert_when_changing_composer_mode
						(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);

void		dom_process_content_after_load	(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);

GVariant *	dom_get_inline_images_data	(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 const gchar *uid_domain);

void		dom_insert_html			(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 const gchar *html_text);

gboolean	dom_process_on_key_press	(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 guint key_val);

gchar *		dom_process_content_for_draft	(WebKitDOMDocument *document);

gchar *		dom_process_content_for_plain_text
						(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);

gchar *		dom_process_content_for_html	(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 const gchar *from_domain);

gboolean	dom_check_if_conversion_needed	(WebKitDOMDocument *document);

void		dom_process_content_after_mode_change
						(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);

gint		dom_get_caret_position		(WebKitDOMDocument *document);

void		dom_drag_and_drop_end		(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);

G_END_DECLS

#endif /* E_HTML_EDITOR_VIEW_DOM_FUNCTIONS_H */
