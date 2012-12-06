/*
 * e-mail-parser-extension.h
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

#ifndef E_MAIL_PARSER_EXTENSION_H
#define E_MAIL_PARSER_EXTENSION_H

#include <em-format/e-mail-parser.h>
#include <camel/camel.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_PARSER_EXTENSION \
	(e_mail_parser_extension_get_type ())
#define E_MAIL_PARSER_EXTENSION(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_PARSER_EXTENSION, EMailParserExtension))
#define E_MAIL_PARSER_EXTENSION_INTERFACE(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_PARSER_EXTENSION, EMailParserExtensionInterface))
#define E_IS_MAIL_PARSER_EXTENSION(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_PARSER_EXTENSION))
#define E_IS_MAIL_PARSER_EXTENSION_INTERFACE(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_PARSER_EXTENSION))
#define E_MAIL_PARSER_EXTENSION_GET_INTERFACE(obj) \
	(G_TYPE_INSTANCE_GET_INTERFACE \
	((obj), E_TYPE_MAIL_PARSER_EXTENSION, EMailParserExtensionInterface))

#define EMP_EXTENSION_GET_PARSER(e) \
	E_MAIL_PARSER (e_extension_get_extensible (E_EXTENSION (e)))

G_BEGIN_DECLS

typedef struct _EMailParserExtension EMailParserExtension;
typedef struct _EMailParserExtensionInterface EMailParserExtensionInterface;

/**
 * EMailParserExtensionFlags:
 * @E_MAIL_PARSER_EXTENSION_INLINE:
 *    Don't parse as attachment.
 * @E_MAIL_PARSER_EXTENSION_INLINE_DISPOSITION:
 *    Always expand.
 * @E_MAIL_PARSER_EXTENSION_COMPOUND_TYPE:
 *    Always check what's inside.
 **/
typedef enum {
	E_MAIL_PARSER_EXTENSION_INLINE			= 1 << 0,
	E_MAIL_PARSER_EXTENSION_INLINE_DISPOSITION	= 1 << 1,
	E_MAIL_PARSER_EXTENSION_COMPOUND_TYPE		= 1 << 2
} EMailParserExtensionFlags;

struct _EMailParserExtensionInterface {
	GTypeInterface parent_interface;

	/* This is a NULL-terminated array of supported MIME types.
	 * The MIME types can be exact (e.g. "text/plain") or use a
	 * wildcard (e.g. "text/ *"). */
	const gchar **mime_types;

	gboolean	(*parse)	(EMailParserExtension *extension,
					 EMailParser *parser,
					 CamelMimePart *mime_part,
					 GString *part_id,
					 GCancellable *cancellable,
					 GQueue *out_mail_parts);

	guint32		(*get_flags)	(EMailParserExtension *extension);

};

GType		e_mail_parser_extension_get_type
						(void) G_GNUC_CONST;
gboolean	e_mail_parser_extension_parse	(EMailParserExtension *extension,
						 EMailParser *parser,
						 CamelMimePart *mime_part,
						 GString *part_id,
						 GCancellable *cancellable,
						 GQueue *out_mail_parts);
guint32		e_mail_parser_extension_get_flags
						(EMailParserExtension *extension);

G_END_DECLS

#endif /* E_MAIL_PARSER_EXTENSION_H */
