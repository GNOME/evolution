/*
 * e-dom-utils.h
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

#ifndef E_DOM_UTILS_H
#define E_DOM_UTILS_H

#define E_UTIL_INCLUDE_WITHOUT_WEBKIT
#include <e-util/e-util.h>
#undef E_UTIL_INCLUDE_WITHOUT_WEBKIT

#include <webkitdom/webkitdom.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

void 		e_dom_utils_replace_local_image_links
						(WebKitDOMDocument *document);
gboolean	e_dom_utils_document_has_selection
						(WebKitDOMDocument *document);
gchar *		e_dom_utils_get_document_content_html
						(WebKitDOMDocument *document);
gchar *		e_dom_utils_get_selection_content_html
						(WebKitDOMDocument *document);
gchar *		e_dom_utils_get_selection_content_text
						(WebKitDOMDocument *document);
void		e_dom_utils_create_and_add_css_style_sheet
						(WebKitDOMDocument *document,
						 const gchar *style_sheet_id);
void		e_dom_utils_add_css_rule_into_style_sheet
						(WebKitDOMDocument *document,
						 const gchar *style_sheet_id,
						 const gchar *selector,
						 const gchar *style);
void		e_dom_utils_eab_contact_formatter_bind_dom
						(WebKitDOMDocument *document);
void		e_dom_utils_bind_focus_on_elements
						(WebKitDOMDocument *document,
						 GDBusConnection *connection);
void		e_dom_utils_e_mail_display_bind_dom
						(WebKitDOMDocument *document,
						 GDBusConnection *connection);
WebKitDOMElement *
		e_dom_utils_find_element_by_selector
						(WebKitDOMDocument *document,
						 const gchar *selector);
WebKitDOMElement *
		e_dom_utils_find_element_by_id	(WebKitDOMDocument *document,
						 const gchar *element_id);
gboolean	e_dom_utils_element_exists
						(WebKitDOMDocument *document,
						 const gchar *element_id);
gchar *		e_dom_utils_get_active_element_name
						(WebKitDOMDocument *document);
void		e_dom_utils_e_mail_part_headers_bind_dom_element
						(WebKitDOMDocument *document,
						 const gchar *element_id);
void		e_dom_utils_element_set_inner_html
						(WebKitDOMDocument *document,
						 const gchar *element_id,
						 const gchar *inner_html);
void		e_dom_utils_remove_element	(WebKitDOMDocument *document,
						 const gchar *element_id);
void		e_dom_utils_element_remove_child_nodes
						(WebKitDOMDocument *document,
						 const gchar *element_id);
void		e_dom_utils_hide_element	(WebKitDOMDocument *document,
						 const gchar *element_id,
                                                 gboolean hide);
gboolean	e_dom_utils_element_is_hidden	(WebKitDOMDocument *document,
						 const gchar *element_id);
WebKitDOMElement *
		e_dom_utils_get_element_from_point
						(WebKitDOMDocument *document,
						 gint32 x,
						 gint32 y);
WebKitDOMDocument *
		e_dom_utils_get_document_from_point
						(WebKitDOMDocument *document,
						 gint32 x,
						 gint32 y);
/* VCard Inline Module DOM functions */
void		e_dom_utils_module_vcard_inline_bind_dom
						(WebKitDOMDocument *document,
						 const gchar *element_id,
						 GDBusConnection *connection);
void		e_dom_utils_module_vcard_inline_update_button
						(WebKitDOMDocument *document,
						 const gchar *button_id,
						 const gchar *html_label,
						 const gchar *access_key);
void		e_dom_utils_module_vcard_inline_set_iframe_src
						(WebKitDOMDocument *document,
						 const gchar *button_id,
						 const gchar *src);
WebKitDOMElement *
		dom_node_find_parent_element 	(WebKitDOMNode *node,
						 const gchar *tagname);
WebKitDOMElement *
		dom_node_find_child_element 	(WebKitDOMNode *node,
						 const gchar *tagname);
gboolean	element_has_id			(WebKitDOMElement *element,
						 const gchar* id);
gboolean	element_has_tag			(WebKitDOMElement *element,
						 const gchar* tag);
gboolean	element_has_class		(WebKitDOMElement *element,
						 const gchar* class);
void		element_add_class		(WebKitDOMElement *element,
						 const gchar* class);
void		element_remove_class		(WebKitDOMElement *element,
						 const gchar* class);
void		remove_node			(WebKitDOMNode *node);
void		remove_node_if_empty		(WebKitDOMNode *node);
WebKitDOMNode *	split_list_into_two		(WebKitDOMNode *item,
						 gint level);

WebKitDOMElement *
		dom_create_selection_marker	(WebKitDOMDocument *document,
						 gboolean start);

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

void		dom_remove_selection_markers	(WebKitDOMDocument *document);

gboolean	node_is_list			(WebKitDOMNode *node);

gboolean	node_is_list_or_item		(WebKitDOMNode *node);

EHTMLEditorSelectionBlockFormat
		dom_get_list_format_from_node	(WebKitDOMNode *node);

void		merge_list_into_list		(WebKitDOMNode *from,
						 WebKitDOMNode *to,
						 gboolean insert_before);

void		merge_lists_if_possible		(WebKitDOMNode *list);
WebKitDOMElement *
		get_parent_block_element	(WebKitDOMNode *node);

gchar *		dom_get_node_inner_html		(WebKitDOMNode *node);

WebKitDOMDocument *
		e_dom_utils_find_document_with_uri
						(WebKitDOMDocument *root_document,
						 const gchar *find_document_uri);

G_END_DECLS

#endif /* E_DOM_UTILS_H */
