/*
 * Copyright (C) 2016 Red Hat, Inc. (www.redhat.com)
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef E_EDITOR_DOM_FUNCTIONS_H
#define E_EDITOR_DOM_FUNCTIONS_H

#include <webkitdom/webkitdom.h>

#define E_UTIL_INCLUDE_WITHOUT_WEBKIT
#include <e-util/e-util.h>
#undef E_UTIL_INCLUDE_WITHOUT_WEBKIT

#include "e-editor-page.h"

/* stephenhay from https://mathiasbynens.be/demo/url-regex */
#define URL_PROTOCOLS "news|telnet|nntp|file|https?|s?ftp|webcal|localhost|ssh"
#define URL_PATTERN_BASE "(?=((?:(?:(?:" URL_PROTOCOLS ")\\:\\/\\/)|(?:www\\.|ftp\\.))[^\\s\\/\\$\\.\\?#].[^\\s]*+)"
#define URL_PATTERN_NO_NBSP ")((?:(?!&nbsp;).)*+)"
#define URL_PATTERN URL_PATTERN_BASE URL_PATTERN_NO_NBSP
#define URL_PATTERN_SPACE URL_PATTERN_BASE "\\s$" URL_PATTERN_NO_NBSP
/* Taken from camel-url-scanner.c */
#define URL_INVALID_TRAILING_CHARS ",.:;?!-|}])\""

/* http://www.w3.org/TR/html5/forms.html#valid-e-mail-address */
#define E_MAIL_PATTERN \
	"[a-zA-Z0-9.!#$%&'*+/=?^_`{|}~-]+@[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}"\
	"[a-zA-Z0-9])?(?:\\.[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?)*+"

#define E_MAIL_PATTERN_SPACE E_MAIL_PATTERN "\\s"

#define QUOTE_SYMBOL ">"

#define SPACES_PER_INDENTATION 3
#define SPACES_PER_LIST_LEVEL 3
#define SPACES_ORDERED_LIST_FIRST_LEVEL 6
#define TAB_LENGTH 8
#define MINIMAL_PARAGRAPH_WIDTH 5

G_BEGIN_DECLS

/* ******************** Tests ******************** */

gboolean	e_editor_dom_test_html_equal	(WebKitDOMDocument *document,
						 const gchar *html1,
						 const gchar *html2);

/* ******************** Actions ******************** */

void		e_editor_dom_delete_cell_contents
						(EEditorPage *editor_page);
void		e_editor_dom_delete_column	(EEditorPage *editor_page);
void		e_editor_dom_delete_row		(EEditorPage *editor_page);
void		e_editor_dom_delete_table	(EEditorPage *editor_page);
void		e_editor_dom_insert_column_after
						(EEditorPage *editor_page);
void		e_editor_dom_insert_column_before
						(EEditorPage *editor_page);
void		e_editor_dom_insert_row_above	(EEditorPage *editor_page);
void		e_editor_dom_insert_row_below	(EEditorPage *editor_page);
void		e_editor_dom_save_history_for_cut
						(EEditorPage *editor_page);

/* ******************** View ******************** */

gboolean	e_editor_dom_exec_command	(EEditorPage *editor_page,
						 EContentEditorCommand command,
						 const gchar *value);
void		e_editor_dom_force_spell_check_for_current_paragraph
						(EEditorPage *editor_page);
void		e_editor_dom_force_spell_check_in_viewport
						(EEditorPage *editor_page);
void		e_editor_dom_force_spell_check	(EEditorPage *editor_page);
void		e_editor_dom_turn_spell_check_off
						(EEditorPage *editor_page);
void		e_editor_dom_embed_style_sheet	(EEditorPage *editor_page,
						 const gchar *style_sheet_content);
void		e_editor_dom_remove_embedded_style_sheet
						(EEditorPage *editor_page);
void		e_editor_dom_register_input_event_listener_on_body
						(EEditorPage *editor_page);
void		e_editor_dom_remove_input_event_listener_from_body
						(EEditorPage *editor_page);
void		e_editor_dom_quote_and_insert_text_into_selection
						(EEditorPage *editor_page,
						 const gchar *text,
						 gboolean is_html);
void		e_editor_dom_check_magic_links	(EEditorPage *editor_page,
						 gboolean include_space_by_user);
void		e_editor_dom_insert_smiley	(EEditorPage *editor_page,
                                                 EEmoticon *emoticon);
void		e_editor_dom_insert_smiley_by_name
						(EEditorPage *editor_page,
						 const gchar *name);
void		e_editor_dom_check_magic_smileys
						(EEditorPage *editor_page);
void		e_editor_dom_set_monospace_font_family_on_body
						(WebKitDOMElement *body,
						 gboolean html_mode);
void		e_editor_dom_convert_content	(EEditorPage *editor_page,
						 const gchar *preferred_text);
void		e_editor_dom_convert_and_insert_html_into_selection
						(EEditorPage *editor_page,
						 const gchar *html,
						 gboolean is_html);
gboolean	e_editor_dom_node_is_citation_node
						(WebKitDOMNode *node);
void		e_editor_dom_quote_plain_text_element_after_wrapping
						(EEditorPage *editor_page,
						 WebKitDOMElement *element,
						 gint quote_level);
WebKitDOMNode * e_editor_dom_get_parent_block_node_from_child
						(WebKitDOMNode *node);
WebKitDOMElement *
		e_editor_dom_insert_new_line_into_citation
						(EEditorPage *editor_page,
						 const gchar *html_to_insert);
WebKitDOMElement *
		e_editor_dom_quote_plain_text_element
						(EEditorPage *editor_page,
						 WebKitDOMElement *element);
void		e_editor_dom_convert_when_changing_composer_mode
						(EEditorPage *editor_page);
void		e_editor_dom_process_content_after_load
						(EEditorPage *editor_page);
GVariant *	e_editor_dom_get_inline_images_data
						(EEditorPage *editor_page,
						 const gchar *uid_domain);
void		e_editor_dom_insert_html	(EEditorPage *editor_page,
						 const gchar *html_text);
void		e_editor_dom_convert_element_from_html_to_plain_text
						(EEditorPage *editor_page,
						 WebKitDOMElement *element);
gchar *		e_editor_dom_process_content_for_draft
						(EEditorPage *editor_page,
						 gboolean only_inner_body);
gchar *		e_editor_dom_process_content_to_plain_text_for_exporting
						(EEditorPage *editor_page);
void		e_editor_dom_restore_images	(EEditorPage *editor_page,
						 GVariant *inline_images_to_restore);
gchar *		e_editor_dom_process_content_to_html_for_exporting
						(EEditorPage *editor_page);
gboolean	e_editor_dom_check_if_conversion_needed
						(EEditorPage *editor_page);
void		e_editor_dom_process_content_after_mode_change
						(EEditorPage *editor_page);
guint		e_editor_dom_get_caret_offset	(EEditorPage *editor_page);
guint		e_editor_dom_get_caret_position	(EEditorPage *editor_page);
void		e_editor_dom_drag_and_drop_end	(EEditorPage *editor_page);
void		e_editor_dom_set_link_color	(EEditorPage *editor_page,
						 const gchar *color);
void		e_editor_dom_set_visited_link_color
						(EEditorPage *editor_page,
						 const gchar *color);
gboolean	e_editor_dom_move_quoted_block_level_up
						(EEditorPage *editor_page);
gboolean	e_editor_dom_delete_last_character_on_line_in_quoted_block
						(EEditorPage *editor_page,
						 glong key_code,
						 gboolean control_key);
gboolean	e_editor_dom_fix_structure_after_delete_before_quoted_content
						(EEditorPage *editor_page,
						 glong key_code,
						 gboolean control_key,
						 gboolean delete_key);
void		e_editor_dom_disable_quote_marks_select
						(EEditorPage *editor_page);
void		e_editor_dom_remove_node_and_parents_if_empty
						(WebKitDOMNode *node);
gboolean	e_editor_dom_return_pressed_in_empty_list_item
						(EEditorPage *editor_page);
void		e_editor_dom_merge_siblings_if_necessary
						(EEditorPage *editor_page,
						 WebKitDOMDocumentFragment *deleted_content);
void		e_editor_dom_body_key_up_event_process_return_key
						(EEditorPage *editor_page);
gboolean	e_editor_dom_key_press_event_process_backspace_key
						(EEditorPage *editor_page);
gboolean	e_editor_dom_key_press_event_process_delete_or_backspace_key
						(EEditorPage *editor_page,
						 glong key_code,
						 gboolean control_key,
						 gboolean delete);
void		e_editor_dom_body_input_event_process
						(EEditorPage *editor_page,
						 WebKitDOMEvent *event);
void		e_editor_dom_body_key_up_event_process_backspace_or_delete
						(EEditorPage *editor_page,
						 gboolean delete);
gboolean	e_editor_dom_key_press_event_process_return_key
						(EEditorPage *editor_page);
WebKitDOMElement *
		e_editor_dom_wrap_and_quote_element
						(EEditorPage *editor_page,
						 WebKitDOMElement *element);
gint		e_editor_dom_get_citation_level	(WebKitDOMNode *node,
						 gboolean set_plaintext_quoted);
void		e_editor_dom_save_history_for_drop
						(EEditorPage *editor_page);
void		e_editor_dom_fix_file_uri_images
						(EEditorPage *editor_page);

/* ******************** Selection ******************** */

void		e_editor_dom_replace_base64_image_src
						(EEditorPage *editor_page,
						 const gchar *selector,
						 const gchar *base64_content,
						 const gchar *filename,
						 const gchar *uri);
WebKitDOMRange *
		e_editor_dom_get_current_range	(EEditorPage *editor_page);
void		e_editor_dom_move_caret_into_element
						(EEditorPage *editor_page,
						 WebKitDOMElement *element,
						 gboolean to_start);
void		e_editor_dom_insert_base64_image
						(EEditorPage *editor_page,
						 const gchar *base64_content,
						 const gchar *filename,
						 const gchar *uri);
void		e_editor_dom_insert_image	(EEditorPage *editor_page,
						 const gchar *uri);
void		e_editor_dom_replace_image_src	(EEditorPage *editor_page,
						 const gchar *selector,
						 const gchar *uri);
void		e_editor_dom_selection_unlink	(EEditorPage *editor_page);
void		e_editor_dom_create_link	(EEditorPage *editor_page,
						 const gchar *uri);
void		e_editor_dom_selection_indent	(EEditorPage *editor_page);
void		e_editor_dom_selection_unindent	(EEditorPage *editor_page);
void		e_editor_dom_selection_save	(EEditorPage *editor_page);
void		e_editor_dom_selection_restore	(EEditorPage *editor_page);
gboolean	e_editor_dom_selection_is_collapsed
						(EEditorPage *editor_page);
void		e_editor_dom_scroll_to_caret	(EEditorPage *editor_page);
void		e_editor_dom_remove_wrapping_from_element
						(WebKitDOMElement *element);
void		e_editor_dom_remove_quoting_from_element
						(WebKitDOMElement *element);
void		e_editor_dom_set_paragraph_style
						(EEditorPage *editor_page,
						 WebKitDOMElement *element,
						 gint width,
						 gint offset,
						 const gchar *style_to_add);
WebKitDOMElement *
		e_editor_dom_get_paragraph_element
						(EEditorPage *editor_page,
						 gint width,
						 gint offset);
WebKitDOMElement *
		e_editor_dom_put_node_into_paragraph
						(EEditorPage *editor_page,
						 WebKitDOMNode *node,
						 gboolean with_input);
void		e_editor_dom_selection_wrap	(EEditorPage *editor_page);
WebKitDOMElement *
		e_editor_dom_wrap_paragraph_length
						(EEditorPage *editor_page,
						 WebKitDOMElement *paragraph,
						 gint length);
WebKitDOMElement *
		e_editor_dom_wrap_paragraph	(EEditorPage *editor_page,
						 WebKitDOMElement *paragraph);
void		e_editor_dom_wrap_paragraphs_in_document
						(EEditorPage *editor_page);
gboolean	e_editor_dom_selection_is_underline
						(EEditorPage *editor_page);
void		e_editor_dom_selection_set_underline
						(EEditorPage *editor_page,
						 gboolean underline);
gboolean	e_editor_dom_selection_is_subscript
						(EEditorPage *editor_page);
void		e_editor_dom_selection_set_subscript
						(EEditorPage *editor_page,
						 gboolean subscript);
gboolean	e_editor_dom_selection_is_superscript
						(EEditorPage *editor_page);
void		e_editor_dom_selection_set_superscript
						(EEditorPage *editor_page,
						 gboolean superscript);
gboolean	e_editor_dom_selection_is_strikethrough
						(EEditorPage *editor_page);
void		e_editor_dom_selection_set_strikethrough
						(EEditorPage *editor_page,
						 gboolean strikethrough);
gboolean	e_editor_dom_selection_is_monospace
						(EEditorPage *editor_page);
void		e_editor_dom_selection_set_monospace
						(EEditorPage *editor_page,
						 gboolean monospaced);
gboolean	e_editor_dom_selection_is_bold	(EEditorPage *editor_page);
void		e_editor_dom_selection_set_bold	(EEditorPage *editor_page,
						 gboolean bold);
gboolean	e_editor_dom_selection_is_italic
						(EEditorPage *editor_page);
void		e_editor_dom_selection_set_italic
						(EEditorPage *editor_page,
						 gboolean italic);
gboolean	e_editor_dom_selection_is_indented
						(EEditorPage *editor_page);
gboolean	e_editor_dom_selection_is_citation
						(EEditorPage *editor_page);
guint		e_editor_dom_selection_get_font_size
						(EEditorPage *editor_page);
void		e_editor_dom_selection_set_font_size
						(EEditorPage *editor_page,
						 guint font_size);
gchar *		e_editor_dom_selection_get_font_name
						(EEditorPage *editor_page);
void		e_editor_dom_selection_set_font_name
						(EEditorPage *editor_page,
						 const gchar *font_size);
gchar *		e_editor_dom_selection_get_font_color
						(EEditorPage *editor_page);
void		e_editor_dom_selection_set_font_color
						(EEditorPage *editor_page,
						 const gchar *font_color);
gchar *		e_editor_dom_selection_get_background_color
						(EEditorPage *editor_page);
void		e_editor_dom_selection_set_background_color
						(EEditorPage *editor_page,
						 const gchar *bg_color);
EContentEditorBlockFormat
		e_editor_dom_selection_get_block_format
						(EEditorPage *editor_page);
void		e_editor_dom_selection_set_block_format
						(EEditorPage *editor_page,
						 EContentEditorBlockFormat format);
EContentEditorAlignment
		e_editor_dom_selection_get_alignment
						(EEditorPage *editor_page);
void		e_editor_dom_selection_set_alignment
						(EEditorPage *editor_page,
						 EContentEditorAlignment alignment);
void		e_editor_dom_selection_replace	(EEditorPage *editor_page,
						 const gchar *replacement);
void		e_editor_dom_replace_caret_word	(EEditorPage *editor_page,
						 const gchar *replacement);
gchar *		e_editor_dom_get_caret_word	(EEditorPage *editor_page);
EContentEditorAlignment
		e_editor_dom_get_list_alignment_from_node
						(WebKitDOMNode *node);
WebKitDOMElement *
		e_editor_dom_prepare_paragraph	(EEditorPage *editor_page,
						 gboolean with_selection);
void		e_editor_dom_selection_set_on_point
						(EEditorPage *editor_page,
						 guint x,
						 guint y);
void		e_editor_dom_selection_get_coordinates
						(EEditorPage *editor_page,
						 guint *start_x,
						 guint *start_y,
						 guint *end_x,
						 guint *end_y);
gboolean	e_editor_dom_is_selection_position_node
						(WebKitDOMNode *node);

G_END_DECLS

#endif /* E_EDITOR_DOM_FUNCTIONS_H */
