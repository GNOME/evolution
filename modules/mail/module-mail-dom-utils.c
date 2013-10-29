/*
 * module-mail-dom-utils.c
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

#include "module-mail-dom-utils.h"

#include <config.h>

gchar *
module_mail_dom_utils_get_active_element_name (WebKitDOMDocument *document)
{
	WebKitDOMElement *element;
	/* FIXME XXX Do version that checks underlying documents */

	element = webkit_dom_html_document_get_active_element (
			WEBKIT_DOM_HTML_DOCUMENT (document));
	if (!element)
		return NULL;

	return webkit_dom_node_get_local_name (WEBKIT_DOM_NODE (element));
}
