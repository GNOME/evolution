/*
 * e-html-editor-dom-functions.h
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

#ifndef E_HTML_EDITOR_DOM_FUNCTIONS_H
#define E_HTML_EDITOR_DOM_FUNCTIONS_H

#include <webkitdom/webkitdom.h>

G_BEGIN_DECLS

guint		e_html_editor_get_flags_for_element_on_coordinates
						(WebKitDOMDocument *document,
						 gint32 x,
						 gint32 y);

G_END_DECLS

#endif /* E_HTML_EDITOR_DOM_FUNCTIONS_H */
