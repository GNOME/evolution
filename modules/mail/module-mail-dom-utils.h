/*
 * module-mail-dom-utils.h
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

#ifndef MODULE_MAIL_DOM_UTILS_H
#define MODULE_MAIL_DOM_UTILS_H

#include <webkitdom/webkitdom.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

gchar *		module_mail_dom_utils_get_active_element_name
						(WebKitDOMDocument *document);
G_END_DECLS

#endif /* MODULE_MAIL_DOM_UTILS_H */
