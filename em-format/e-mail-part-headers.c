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

#include <config.h>
#include <glib/gi18n-lib.h>

#define E_MAIL_PART_HEADERS_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_PART_HEADERS, EMailPartHeadersPrivate))

struct _EMailPartHeadersPrivate {
	GMutex property_lock;
	gchar **default_headers;
};

enum {
	PROP_0,
	PROP_DEFAULT_HEADERS
};

G_DEFINE_TYPE (
	EMailPartHeaders,
	e_mail_part_headers,
	E_TYPE_MAIL_PART)

static const gchar *basic_headers[] = {
	N_("From"),
	N_("Reply-To"),
	N_("To"),
	N_("Cc"),
	N_("Bcc"),
	N_("Subject"),
	N_("Date"),
	N_("Newsgroups"),
	N_("Face"),
	NULL
};

static void
mail_part_headers_set_property (GObject *object,
                                guint property_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_DEFAULT_HEADERS:
			e_mail_part_headers_set_default_headers (
				E_MAIL_PART_HEADERS (object),
				g_value_get_boxed (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_part_headers_get_property (GObject *object,
                                guint property_id,
                                GValue *value,
                                GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_DEFAULT_HEADERS:
			g_value_take_boxed (
				value,
				e_mail_part_headers_dup_default_headers (
				E_MAIL_PART_HEADERS (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_part_headers_finalize (GObject *object)
{
	EMailPartHeadersPrivate *priv;

	priv = E_MAIL_PART_HEADERS_GET_PRIVATE (object);

	g_mutex_clear (&priv->property_lock);

	g_strfreev (priv->default_headers);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_mail_part_headers_parent_class)->finalize (object);
}

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

	g_type_class_add_private (class, sizeof (EMailPartHeadersPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_part_headers_set_property;
	object_class->get_property = mail_part_headers_get_property;
	object_class->finalize = mail_part_headers_finalize;
	object_class->constructed = mail_part_headers_constructed;

	mail_part_class = E_MAIL_PART_CLASS (class);
	mail_part_class->bind_dom_element = mail_part_headers_bind_dom_element;

	g_object_class_install_property (
		object_class,
		PROP_DEFAULT_HEADERS,
		g_param_spec_boxed (
			"default-headers",
			"Default Headers",
			"Headers to display by default",
			G_TYPE_STRV,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));
}

static void
e_mail_part_headers_init (EMailPartHeaders *part)
{
	part->priv = E_MAIL_PART_HEADERS_GET_PRIVATE (part);

	g_mutex_init (&part->priv->property_lock);
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

gchar **
e_mail_part_headers_dup_default_headers (EMailPartHeaders *part)
{
	gchar **default_headers;

	g_return_val_if_fail (E_IS_MAIL_PART_HEADERS (part), NULL);

	g_mutex_lock (&part->priv->property_lock);

	default_headers = g_strdupv (part->priv->default_headers);

	g_mutex_unlock (&part->priv->property_lock);

	return default_headers;
}

void
e_mail_part_headers_set_default_headers (EMailPartHeaders *part,
                                         const gchar * const *default_headers)
{
	g_return_if_fail (E_IS_MAIL_PART_HEADERS (part));

	if (default_headers == NULL)
		default_headers = basic_headers;

	g_mutex_lock (&part->priv->property_lock);

	g_strfreev (part->priv->default_headers);
	part->priv->default_headers = g_strdupv ((gchar **) default_headers);

	g_mutex_unlock (&part->priv->property_lock);

	g_object_notify (G_OBJECT (part), "default-headers");
}

