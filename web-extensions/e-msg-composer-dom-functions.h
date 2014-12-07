/*
 * e-msg-composer-dom-functions.h
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

#ifndef E_MSG_COMPOSER_DOM_FUNCTIONS_H
#define E_MSG_COMPOSER_DOM_FUNCTIONS_H

#include <webkitdom/webkitdom.h>

G_BEGIN_DECLS

gchar *		dom_get_active_signature_uid 	(WebKitDOMDocument *document);

gchar *		dom_get_raw_body_content_without_signature
						(WebKitDOMDocument *document);

gchar *		dom_get_raw_body_content	(WebKitDOMDocument *document);

G_END_DECLS

#endif /* E_MSG_COMPOSER_DOM_FUNCTIONS_H */
