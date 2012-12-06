/*
 * e-mail-extension.h
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

#ifndef E_MAIL_EXTENSION_H
#define E_MAIL_EXTENSION_H

#include <glib-object.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_EXTENSION \
	(e_mail_extension_get_type ())
#define E_MAIL_EXTENSION(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_EXTENSION, EMailExtension))
#define E_MAIL_EXTENSION_INTERFACE(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_EXTENSION, EMailExtensionInterface))
#define E_IS_MAIL_EXTENSION(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_EXTENSION))
#define E_IS_MAIL_EXTENSION_INTERFACE(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_EXTENSION))
#define E_MAIL_EXTENSION_GET_INTERFACE(obj) \
	(G_TYPE_INSTANCE_GET_INTERFACE \
	((obj), E_TYPE_MAIL_EXTENSION, EMailExtensionInterface))

G_BEGIN_DECLS

typedef struct _EMailExtension EMailExtension;
typedef struct _EMailExtensionInterface EMailExtensionInterface;

struct _EMailExtensionInterface {
	GTypeInterface parent_interface;

	/* This is a NULL-terminated array of supported MIME types.
	 * The MIME types can be exact (e.g. "text/plain") or use a
	 * wildcard (e.g. "text/ *"). */
	const gchar **mime_types;
};

GType		e_mail_extension_get_type		(void) G_GNUC_CONST;

G_END_DECLS

#endif /* E_MAIL_EXTENSION_H */
