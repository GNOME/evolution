/*
 * e-mail-parser-extension.h
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef E_MAIL_PARSER_EXTENSION_H
#define E_MAIL_PARSER_EXTENSION_H

#include <camel/camel.h>
#include <em-format/e-mail-parser.h>
#include <em-format/e-mail-formatter-enums.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_PARSER_EXTENSION \
	(e_mail_parser_extension_get_type ())
#define E_MAIL_PARSER_EXTENSION(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_PARSER_EXTENSION, EMailParserExtension))
#define E_MAIL_PARSER_EXTENSION_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_PARSER_EXTENSION, EMailParserExtensionClass))
#define E_IS_MAIL_PARSER_EXTENSION(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_PARSER_EXTENSION))
#define E_IS_MAIL_PARSER_EXTENSION_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_PARSER_EXTENSION))
#define E_MAIL_PARSER_EXTENSION_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_PARSER_EXTENSION, EMailParserExtensionClass))

G_BEGIN_DECLS

typedef struct _EMailParserExtension EMailParserExtension;
typedef struct _EMailParserExtensionClass EMailParserExtensionClass;
typedef struct _EMailParserExtensionPrivate EMailParserExtensionPrivate;

/**
 * EMailParserExtension:
 *
 * The #EMailParserExtension is an abstract interface for all extensions for
 * #EMailParser.
 */
struct _EMailParserExtension {
	GObject parent;
	EMailParserExtensionPrivate *priv;
};

struct _EMailParserExtensionClass {
	GObjectClass parent_class;

	/* This is a NULL-terminated array of supported MIME types.
	 * The MIME types can be exact (e.g. "text/plain") or use a
	 * wildcard (e.g. "text/ *"). */
	const gchar **mime_types;

	/* This is used to prioritize extensions with identical MIME
	 * types.  Lower values win.  Defaults to G_PRIORITY_DEFAULT. */
	gint priority;

	/* See the flag descriptions above. */
	EMailParserExtensionFlags flags;

	gboolean	(*parse)		(EMailParserExtension *extension,
						 EMailParser *parser,
						 CamelMimePart *mime_part,
						 GString *part_id,
						 GCancellable *cancellable,
						 GQueue *out_mail_parts);
};

GType		e_mail_parser_extension_get_type
						(void) G_GNUC_CONST;
gboolean	e_mail_parser_extension_parse	(EMailParserExtension *extension,
						 EMailParser *parser,
						 CamelMimePart *mime_part,
						 GString *part_id,
						 GCancellable *cancellable,
						 GQueue *out_mail_parts);

G_END_DECLS

#endif /* E_MAIL_PARSER_EXTENSION_H */
