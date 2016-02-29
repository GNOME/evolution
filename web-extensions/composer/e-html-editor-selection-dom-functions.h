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

#include "e-html-editor-web-extension.h"

#define UNICODE_ZERO_WIDTH_SPACE "\xe2\x80\x8b"
#define UNICODE_NBSP "\xc2\xa0"

/* stephenhay from https://mathiasbynens.be/demo/url-regex */
#define URL_PROTOCOLS "news|telnet|nntp|file|https?|s?ftp||webcal|localhost|ssh"
#define URL_PATTERN "((((" URL_PROTOCOLS ")\\:\\/\\/)|(www\\.|ftp\\.))[^\\s\\/\\$\\.\\?#].[^\\s]*)"

#define URL_PATTERN_SPACE URL_PATTERN "\\s"
/* Taken from camel-url-scanner.c */
#define URL_INVALID_TRAILING_CHARS ",.:;?!-|}])\""

/* http://www.w3.org/TR/html5/forms.html#valid-e-mail-address */
#define E_MAIL_PATTERN \
	"[a-zA-Z0-9.!#$%&'*+/=?^_`{|}~-]+@[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}"\
	"[a-zA-Z0-9])?(?:\\.[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?)*"

#define E_MAIL_PATTERN_SPACE E_MAIL_PATTERN "\\s"

#define QUOTE_SYMBOL ">"

#define SPACES_PER_INDENTATION 3
#define SPACES_PER_LIST_LEVEL 3
#define SPACES_ORDERED_LIST_FIRST_LEVEL 6
#define TAB_LENGTH 8
#define MINIMAL_PARAGRAPH_WIDTH 5

G_BEGIN_DECLS

void		dom_replace_base64_image_src	(WebKitDOMDocument *document,
						 const gchar *selector,
						 const gchar *base64_content,
						 const gchar *filename,
						 const gchar *uri);

WebKitDOMRange *
		dom_get_current_range		(WebKitDOMDocument *document);

gchar *		dom_selection_get_string	(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);

void		dom_move_caret_into_element	(WebKitDOMDocument *document,
						 WebKitDOMElement *element,
						 gboolean to_start);

void		dom_restore_caret_position	(WebKitDOMDocument *document);

void		dom_insert_base64_image		(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 const gchar *filename,
						 const gchar *uri,
						 const gchar *base64_content);

void		dom_selection_unlink		(WebKitDOMDocument *document,
                                                 EHTMLEditorWebExtension *extension);

void		dom_create_link			(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 const gchar *uri);

EHTMLEditorSelectionBlockFormat
		dom_get_list_format_from_node	(WebKitDOMNode *node);

void		dom_selection_indent		(WebKitDOMDocument *document,
						EHTMLEditorWebExtension *extension);

void		dom_selection_unindent		(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);

void		dom_selection_save		(WebKitDOMDocument *document);

void		dom_selection_restore		(WebKitDOMDocument *document);

gboolean	dom_selection_is_collapsed	(WebKitDOMDocument *document);

void		dom_scroll_to_caret		(WebKitDOMDocument *document);

void		dom_remove_wrapping_from_element
						(WebKitDOMElement *element);

void		dom_remove_quoting_from_element	(WebKitDOMElement *element);

void		dom_set_paragraph_style		(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 WebKitDOMElement *element,
						 gint width,
						 gint offset,
						 const gchar *style_to_add);

WebKitDOMElement *
		dom_create_selection_marker	(WebKitDOMDocument *document,
						 gboolean selection_start_marker);

void		dom_add_selection_markers_into_element_start
						(WebKitDOMDocument *document,
						 WebKitDOMElement *element,
						 WebKitDOMElement **selection_start_marker,
						 WebKitDOMElement **selection_end_marker);

void		dom_add_selection_markers_into_element_end
						(WebKitDOMDocument *document,
						 WebKitDOMElement *element,
						 WebKitDOMElement **selection_start_marker,
						 WebKitDOMElement **selection_end_marker);

WebKitDOMElement *
		dom_get_paragraph_element	(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 gint width,
						 gint offset);

WebKitDOMElement *
		dom_put_node_into_paragraph	(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 WebKitDOMNode *node,
						 gboolean with_input);

void		dom_selection_wrap		(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);

WebKitDOMElement *
		dom_wrap_paragraph_length	(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 WebKitDOMElement *paragraph,
						 gint length);

WebKitDOMElement *
		dom_wrap_paragraph		(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 WebKitDOMElement *paragraph);

void		dom_wrap_paragraphs_in_document	(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);

gboolean	dom_selection_is_underline	(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);

void		dom_selection_set_underline	(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 gboolean underline);

gboolean	dom_selection_is_subscript	(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);

void		dom_selection_set_subscript	(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 gboolean subscript);

gboolean	dom_selection_is_superscript	(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);

void		dom_selection_set_superscript	(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 gboolean superscript);

gboolean	dom_selection_is_strikethrough	(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);

void		dom_selection_set_strikethrough	(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 gboolean strikethrough);

gboolean	dom_selection_is_monospaced	(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);

void		dom_selection_set_monospaced	(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 gboolean monospaced);

gboolean	dom_selection_is_bold		(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);

void		dom_selection_set_bold		(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 gboolean bold);

gboolean	dom_selection_is_italic		(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);

void		dom_selection_set_italic	(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 gboolean italic);

gboolean	dom_selection_is_indented	(WebKitDOMDocument *document);

gboolean	dom_selection_is_citation	(WebKitDOMDocument *document);

guint		dom_selection_get_font_size	(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);

void		dom_selection_set_font_size	(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 guint font_size);

gchar *		dom_selection_get_font_name	(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);

void		dom_selection_set_font_name	(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 const gchar *font_size);

gchar *		dom_selection_get_font_color	(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);

void		dom_selection_set_font_color	(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 const gchar *font_color);

gchar *		dom_selection_get_background_color
						(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);

void		dom_selection_set_background_color
						(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 const gchar *font_color);

EHTMLEditorSelectionBlockFormat
		dom_selection_get_block_format	(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);

void		dom_selection_set_block_format	(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 EHTMLEditorSelectionBlockFormat format);

EHTMLEditorSelectionAlignment
		dom_selection_get_alignment	(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);

void		dom_selection_set_alignment	(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 EHTMLEditorSelectionAlignment alignment);

void		dom_selection_replace		(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 const gchar *replacement);

void		dom_replace_caret_word		(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 const gchar *replacement);

gchar *		dom_get_caret_word		(WebKitDOMDocument *document);

gboolean	dom_selection_has_text		(WebKitDOMDocument *document);

EHTMLEditorSelectionAlignment
		dom_get_list_alignment_from_node
						(WebKitDOMNode *node);

WebKitDOMElement *
		dom_prepare_paragraph		(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 gboolean with_selection);

void		dom_selection_set_on_point	(WebKitDOMDocument *document,
						 guint x,
						 guint y);

void		dom_selection_get_coordinates	(WebKitDOMDocument *document,
						 guint *start_x,
						 guint *start_y,
						 guint *end_x,
						 guint *end_y);
void		dom_remove_selection_markers	(WebKitDOMDocument *document);
gboolean	dom_is_selection_position_node	(WebKitDOMNode *node);

G_END_DECLS

#endif /* E_HTML_EDITOR_SELECTION_DOM_FUNCTIONS_H */
