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

G_END_DECLS

#endif /* E_HTML_EDITOR_UTILS_H */
