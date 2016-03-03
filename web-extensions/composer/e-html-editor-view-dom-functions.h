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
#include <e-util/e-emoticon.h>

G_BEGIN_DECLS

gboolean	dom_exec_command		(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 EHTMLEditorViewCommand command,
						 const gchar *value);

void		dom_force_spell_check_for_current_paragraph
						(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);

void		dom_force_spell_check_in_viewport
						(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);

void		dom_force_spell_check		(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);

void		dom_turn_spell_check_off	(WebKitDOMDocument *document,
                                                 EHTMLEditorWebExtension *extension);

void		dom_embed_style_sheet		(WebKitDOMDocument *document,
						 const gchar *style_sheet_content);

void		dom_remove_embed_style_sheet	(WebKitDOMDocument *document);

void		dom_register_input_event_listener_on_body
						(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);

void		dom_remove_input_event_listener_from_body
						(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);

void		dom_quote_and_insert_text_into_selection
						(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 const gchar *text);

void		dom_check_magic_links		(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 gboolean include_space_by_user);

void		dom_insert_smiley		(WebKitDOMDocument *document,
                                                 EHTMLEditorWebExtension *extension,
                                                 EEmoticon *emoticon);

void		dom_insert_smiley_by_name	(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 const gchar *name);

void		dom_check_magic_smileys		(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);

void		dom_convert_content		(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 const gchar *preferred_text);

void		dom_convert_and_insert_html_into_selection
						(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 const gchar *html,
						 gboolean is_html);

gboolean	dom_node_is_citation_node	(WebKitDOMNode *node);

void		dom_quote_plain_text_element_after_wrapping
						(WebKitDOMDocument *document,
						 WebKitDOMElement *element,
						 gint quote_level);

WebKitDOMNode * get_parent_block_node_from_child
						(WebKitDOMNode *node);

WebKitDOMElement *
		dom_insert_new_line_into_citation
						(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 const gchar *html_to_insert);

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
						 guint key_val,
						 guint state);

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

gint		dom_get_caret_offset		(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);
gint		dom_get_caret_position		(WebKitDOMDocument *document);

void		dom_drag_and_drop_end		(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);

void		dom_set_link_color		(WebKitDOMDocument *document,
						 const gchar *color);

void		dom_set_visited_link_color	(WebKitDOMDocument *document,
						 const gchar *color);

gboolean	dom_change_quoted_block_to_normal
						(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);
gboolean	dom_delete_last_character_on_line_in_quoted_block
						(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 guint key_val,
						 guint state);
gboolean	dom_fix_structure_after_delete_before_quoted_content
						(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 guint key_val,
						 guint state,
						 gboolean delete_key);
void		dom_disable_quote_marks_select	(WebKitDOMDocument *document);
void		dom_remove_node_and_parents_if_empty
						(WebKitDOMNode *node);
gboolean	return_pressed_in_empty_list_item
						(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 gboolean save_history);
void		dom_merge_siblings_if_necessary
						(WebKitDOMDocument *document,
						 WebKitDOMDocumentFragment *deleted_content);
void		body_key_up_event_process_return_key
						(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);
gboolean	key_press_event_process_backspace_key
						(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);
gboolean	key_press_event_process_delete_or_backspace_key
						(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 guint key_val,
						 guint state,
						 gboolean delete);
void		body_input_event_process	(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 WebKitDOMEvent *event);
void		body_key_up_event_process_backspace_or_delete
						(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 gboolean delete);
gboolean	key_press_event_process_return_key
						(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);
WebKitDOMElement *
		wrap_and_quote_element		(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension,
						 WebKitDOMElement *element);
gint		get_citation_level		(WebKitDOMNode *node,
						 gboolean set_plaintext_quoted);
void		dom_save_history_for_drop	(WebKitDOMDocument *document,
						 EHTMLEditorWebExtension *extension);

G_END_DECLS

#endif /* E_HTML_EDITOR_VIEW_DOM_FUNCTIONS_H */
