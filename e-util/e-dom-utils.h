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

#include <webkitdom/webkitdom.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

void 		e_dom_utils_replace_local_image_links
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
		e_html_editor_dom_node_find_parent_element (
						WebKitDOMNode *node,
						const gchar *tagname);
WebKitDOMElement *
		e_html_editor_dom_node_find_child_element (
						WebKitDOMNode *node,
						const gchar *tagname);
G_END_DECLS

#endif /* E_DOM_UTILS_H */
