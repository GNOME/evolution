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

#include "e-util-enums.h"

G_BEGIN_DECLS

void		dom_replace_base64_image_src	(WebKitDOMDocument *document,
						 const gchar *selector,
						 const gchar *base64_content,
						 const gchar *filename,
						 const gchar *uri);

void		dom_clear_caret_position_marker
						(WebKitDOMDocument *document);

WebKitDOMNode *
		dom_create_caret_position_node	(WebKitDOMDocument *document);
/*
WebKitDOMRange *
		dom_get_current_range		(WebKitDOMDocument *document);
*/
gchar *		dom_selection_get_string	(WebKitDOMDocument *document);

WebKitDOMElement *
		dom_save_caret_position		(WebKitDOMDocument *document);
/*
void		dom_move_caret_into_element	(WebKitDOMDocument *document,
						 WebKitDOMElement *element);
*/
void		dom_restore_caret_position	(WebKitDOMDocument *document);

void		dom_unlink			(WebKitDOMDocument *document);

void		dom_create_link			(WebKitDOMDocument *document,
						 const gchar *uri);

void		dom_selection_save		(WebKitDOMDocument *document);

void		dom_selection_restore		(WebKitDOMDocument *document);

gboolean	dom_selection_is_underline	(WebKitDOMDocument *document);

void		dom_selection_set_underline	(WebKitDOMDocument *document,
						 gboolean underline);

gboolean	dom_selection_is_subscript	(WebKitDOMDocument *document);

void		dom_selection_set_subscript	(WebKitDOMDocument *document,
						 gboolean subscript);

gboolean	dom_selection_is_superscript	(WebKitDOMDocument *document);

void		dom_selection_set_superscript	(WebKitDOMDocument *document,
						 gboolean superscript);

gboolean	dom_selection_is_strikethrough	(WebKitDOMDocument *document);

void		dom_selection_set_strikethrough	(WebKitDOMDocument *document,
						 gboolean strikethrough);

gboolean	dom_selection_is_monospaced	(WebKitDOMDocument *document);

void		dom_selection_set_monospaced	(WebKitDOMDocument *document,
						 gboolean monospaced);

gboolean	dom_selection_is_bold		(WebKitDOMDocument *document);

void		dom_selection_set_bold		(WebKitDOMDocument *document,
						 gboolean bold);

gboolean	dom_selection_is_italic		(WebKitDOMDocument *document);

void		dom_selection_set_italic	(WebKitDOMDocument *document,
						 gboolean italic);

gboolean	dom_selection_is_indented	(WebKitDOMDocument *document);

gboolean	dom_selection_is_citation	(WebKitDOMDocument *document);

guint		dom_selection_get_font_size	(WebKitDOMDocument *document);

void		dom_selection_set_font_size	(WebKitDOMDocument *document,
						 guint font_size);

gchar *		dom_selection_get_font_name	(WebKitDOMDocument *document);

void		dom_selection_set_font_name	(WebKitDOMDocument *document,
						 const gchar *font_size);

gchar *		dom_selection_get_font_color	(WebKitDOMDocument *document);

void		dom_selection_set_font_color	(WebKitDOMDocument *document,
						 const gchar *font_color);

gchar *		dom_selection_get_background_color
						(WebKitDOMDocument *document);

void		dom_selection_set_background_color
						(WebKitDOMDocument *document,
						 const gchar *font_color);

EHTMLEditorSelectionBlockFormat
		dom_selection_get_block_format	(WebKitDOMDocument *document);

void		dom_selection_set_block_format	(WebKitDOMDocument *document,
						 EHTMLEditorSelectionBlockFormat format);

EHTMLEditorSelectionAlignment
		dom_selection_get_alignment	(WebKitDOMDocument *document);

void		dom_selection_set_alignment	(WebKitDOMDocument *document,
						 EHTMLEditorSelectionAlignment alignment);

void		dom_selection_replace		(WebKitDOMDocument *document,
						 const gchar *replacement);

void		dom_replace_caret_word		(WebKitDOMDocument *document,
						 const gchar *replacement);

gchar *		dom_get_caret_word		(WebKitDOMDocument *document);

gboolean	dom_selection_has_text		(WebKitDOMDocument *document);

G_END_DECLS

#endif /* E_HTML_EDITOR_SELECTION_DOM_FUNCTIONS_H */
