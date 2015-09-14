/*
 * e-html-editor-utils.h
 *
 * Copyright (C) 2012 Dan Vrátil <dvratil@redhat.com>
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

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_HTML_EDITOR_UTILS_H
#define E_HTML_EDITOR_UTILS_H

#include <e-util/e-util-enums.h>

#include <webkit/webkitdom.h>

G_BEGIN_DECLS

WebKitDOMElement *
		e_html_editor_dom_node_find_parent_element
						(WebKitDOMNode *node,
						 const gchar *tagname);

WebKitDOMElement *
		e_html_editor_dom_node_find_child_element
						(WebKitDOMNode *node,
						 const gchar *tagname);

gboolean	e_html_editor_node_is_selection_position_node
						(WebKitDOMNode *node);

WebKitDOMNode *	e_html_editor_get_parent_block_node_from_child
						(WebKitDOMNode *node);

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

WebKitDOMNode *	split_node_into_two		(WebKitDOMNode *item,
						 gint level);

WebKitDOMElement *
		create_selection_marker		(WebKitDOMDocument *document,
						 gboolean start);

void		add_selection_markers_into_element_start
						(WebKitDOMDocument *document,
						 WebKitDOMElement *element,
						 WebKitDOMElement **selection_start_marker,
						 WebKitDOMElement **selection_end_marker);

void		add_selection_markers_into_element_end
						(WebKitDOMDocument *document,
						 WebKitDOMElement *element,
						 WebKitDOMElement **selection_start_marker,
						 WebKitDOMElement **selection_end_marker);

void		remove_selection_markers	(WebKitDOMDocument *document);

gboolean	node_is_list			(WebKitDOMNode *node);

gboolean	node_is_list_or_item		(WebKitDOMNode *node);

EHTMLEditorSelectionBlockFormat
		get_list_format_from_node	(WebKitDOMNode *node);

void		merge_list_into_list		(WebKitDOMNode *from,
						 WebKitDOMNode *to,
						 gboolean insert_before);

void		merge_lists_if_possible		(WebKitDOMNode *list);

WebKitDOMElement *
		get_parent_block_element	(WebKitDOMNode *node);
G_END_DECLS

#endif /* E_HTML_EDITOR_UTILS_H */
