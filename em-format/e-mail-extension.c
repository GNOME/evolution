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

