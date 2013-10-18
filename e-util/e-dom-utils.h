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

#if (!defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)) && \
	!defined (WEBEXTENSIONS_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

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

G_END_DECLS

#endif /* E_DOM_UTILS_H */
