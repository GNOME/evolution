/*
 * e-mail-part-headers.c
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

#include "e-mail-part-headers.h"

G_DEFINE_TYPE (
	EMailPartHeaders,
	e_mail_part_headers,
	E_TYPE_MAIL_PART)

static void
mail_part_headers_constructed (GObject *object)
{
	EMailPart *part;

	part = E_MAIL_PART (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_part_headers_parent_class)->
		constructed (object);

	e_mail_part_set_mime_type (part, E_MAIL_PART_HEADERS_MIME_TYPE);
}

static void
mail_part_headers_bind_dom_element (EMailPart *part,
                                    WebKitDOMElement *element)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *photo;
	gchar *addr, *uri;

	document = webkit_dom_node_get_owner_document (
		WEBKIT_DOM_NODE (element));
	photo = webkit_dom_document_get_element_by_id (
		document, "__evo-contact-photo");

	/* Contact photos disabled, the <img> tag is not there. */
	if (photo == NULL)
		return;

	addr = webkit_dom_element_get_attribute (photo, "data-mailaddr");
	uri = g_strdup_printf ("mail://contact-photo?mailaddr=%s", addr);

	webkit_dom_html_image_element_set_src (
		WEBKIT_DOM_HTML_IMAGE_ELEMENT (photo), uri);

	g_free (addr);
	g_free (uri);
}

static void
e_mail_part_headers_class_init (EMailPartHeadersClass *class)
{
	GObjectClass *object_class;
	EMailPartClass *mail_part_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = mail_part_headers_constructed;

	mail_part_class = E_MAIL_PART_CLASS (class);
	mail_part_class->bind_dom_element = mail_part_headers_bind_dom_element;
}

static void
e_mail_part_headers_init (EMailPartHeaders *part)
{
}

EMailPart *
e_mail_part_headers_new (CamelMimePart *mime_part,
                         const gchar *id)
{
	g_return_val_if_fail (id != NULL, NULL);

	return g_object_new (
		E_TYPE_MAIL_PART_HEADERS,
		"id", id, "mime-part", mime_part, NULL);
}

