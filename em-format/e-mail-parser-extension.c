/*
 * e-mail-parser-extension.c
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

#include <camel/camel.h>

#include "e-mail-parser-extension.h"

G_DEFINE_INTERFACE (
	EMailParserExtension,
	e_mail_parser_extension,
	E_TYPE_MAIL_EXTENSION)

static guint32
mail_parser_extension_get_flags (EMailParserExtension *extension)
{
	return 0;
}

/**
 * EMailParserExtension:
 *
 * The #EMailParserExtension is an abstract interface for all extensions for
 * #EMailParser.
 */

static void
e_mail_parser_extension_default_init (EMailParserExtensionInterface *interface)
{
	interface->get_flags = mail_parser_extension_get_flags;
}

/**
 * e_mail_parser_extension_parse
 * @extension: an #EMailParserExtension
 * @parser: a #EMailParser
 * @mime_part: (allow-none) a #CamelMimePart to parse
 * @part_id: a #GString to which parser will append ID of the parsed part.
 * @flags: #EMailParserFlags
 * @cancellable: (allow-none) A #GCancellable
 *
 * A virtual function reimplemented in all mail parser extensions. The function
 * decodes and parses the @mime_part, creating one or more #EMailPart<!-//>s.
 *
 * When the function is unable to parse the @mime_part (either because it's broken
 * or because it is a different mimetype then the extension is specialized for), the
 * function will return @NULL indicating the #EMailParser, that it should pick
 * another extension.
 *
 * When the @mime_part contains for example multipart/mixed of one RFC822 message
 * with an attachment and of one image, then parser must make sure that the
 * returned #GSList is correctly ordered:
 *
 * part1.rfc822.plain_text
 * part1.rfc822.attachment
 * part2.image
 *
 * Implementation of this function must be thread-safe.
 *
 * Return value: Returns #GSList of #EMailPart<!-//>s when the part was succesfully
 * parsed, returns @NULL when the parser is not able to parse the part.
 */
GSList *
e_mail_parser_extension_parse (EMailParserExtension *extension,
                              EMailParser *parser,
                              CamelMimePart *mime_part,
                              GString *part_id,
                              GCancellable *cancellable)
{
	EMailParserExtensionInterface *interface;

	g_return_val_if_fail (E_IS_MAIL_PARSER_EXTENSION (extension), NULL);
	g_return_val_if_fail (E_IS_MAIL_PARSER (parser), NULL);

	interface = E_MAIL_PARSER_EXTENSION_GET_INTERFACE (extension);
	g_return_val_if_fail (interface->parse != NULL, NULL);

	return interface->parse (extension, parser, mime_part, part_id, cancellable);
}

guint32
e_mail_parser_extension_get_flags (EMailParserExtension *extension)
{
	EMailParserExtensionInterface *interface;

	g_return_val_if_fail (E_IS_MAIL_PARSER_EXTENSION (extension), 0);

	interface = E_MAIL_PARSER_EXTENSION_GET_INTERFACE (extension);
	g_return_val_if_fail (interface->get_flags != NULL, 0);

	return interface->get_flags (extension);
}
