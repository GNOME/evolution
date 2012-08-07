/*
 * e-editor-utils.h
 *
<<<<<<< HEAD
 * Copyright (C) 2012 Dan Vrátil <dvratil@redhat.com>
 *
=======
>>>>>>> Make the URL editor work
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

#ifndef E_EDITOR_UTILS_H
#define E_EDITOR_UTILS_H

#include <webkit/webkitdom.h>

<<<<<<< HEAD
G_BEGIN_DECLS

WebKitDOMElement *
		e_editor_dom_node_find_parent_element
						(WebKitDOMNode *node,
						 const gchar *tagname);

WebKitDOMElement *
		e_editor_dom_node_find_child_element
						(WebKitDOMNode *node,
						 const gchar *tagname);

G_END_DECLS
=======
WebKitDOMElement *	e_editor_dom_node_get_parent_element
							(WebKitDOMNode *node,
							 GType parent_type);
>>>>>>> Make the URL editor work

WebKitDOMElement *	e_editor_dom_node_find_child_element
							(WebKitDOMNode *node,
							 GType child_type);

#endif /* E_EDITOR_UTILS_H */
