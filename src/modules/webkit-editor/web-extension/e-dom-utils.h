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

#define UNICODE_ZERO_WIDTH_SPACE "\xe2\x80\x8b"
#define UNICODE_NBSP "\xc2\xa0"

#define E_EVOLUTION_BLOCKQUOTE_STYLE "margin:0 0 0 .8ex; border-left:2px #729fcf solid;padding-left:1ex"

G_BEGIN_DECLS

void		e_dom_utils_create_and_add_css_style_sheet
						(WebKitDOMDocument *document,
						 const gchar *style_sheet_id);
WebKitDOMElement *
		dom_node_find_parent_element 	(WebKitDOMNode *node,
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
void		element_rename_attribute	(WebKitDOMElement *element,
						 const gchar *from,
						 const gchar *to);
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
EContentEditorBlockFormat
		dom_get_list_format_from_node	(WebKitDOMNode *node);
void		merge_list_into_list		(WebKitDOMNode *from,
						 WebKitDOMNode *to,
						 gboolean insert_before);
void		merge_lists_if_possible		(WebKitDOMNode *list);
WebKitDOMElement *
		get_parent_block_element	(WebKitDOMNode *node);
gchar *		dom_get_node_inner_html		(WebKitDOMNode *node);
void		dom_element_swap_attributes	(WebKitDOMElement *element,
                                                 const gchar *from,
                                                 const gchar *to);

G_END_DECLS

#endif /* E_DOM_UTILS_H */
