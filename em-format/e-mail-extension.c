/*
 * e-mail-extension.c
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

#include "e-mail-extension.h"

#include <glib-object.h>

G_DEFINE_INTERFACE (EMailExtension, e_mail_extension, G_TYPE_OBJECT)

static void
e_mail_extension_default_init (EMailExtensionInterface *iface)
{

}

/**
 * EMailExtension:
 *
 * The #EMailExtension is an abstract interface for all extensions for
 * #EMailParser and #EmailFormatter.
 *
 * The interface is further extended by #EMailParserExtension and
 * #EMailFormatterExtension interfaces which define final API for both types
 * of extensions.
 */

/**
 * e_mail_extension_get_mime_types:
 * @extension: an #EMailExtension
 *
 * A virtual function reimplemented in all mail extensions that returns a
 * @NULL-terminated array of mime types that the particular extension is able
 * to process.
 *
 * The mime-types can be either full (like text/plain), or with common subtype,
 * e.g. text/ *. User should try to find the best mathing mime-type handler and
 * use the latter type only as a fallback.
 *
 * Return value: a @NULL-terminated array or @NULL
 */
const gchar **
e_mail_extension_get_mime_types (EMailExtension *extension)
{
	EMailExtensionInterface *interface;

	g_return_val_if_fail (E_IS_MAIL_EXTENSION (extension), NULL);

	interface = E_MAIL_EXTENSION_GET_INTERFACE (extension);
	g_return_val_if_fail (interface->mime_types != NULL, NULL);

	return interface->mime_types (extension);
}
