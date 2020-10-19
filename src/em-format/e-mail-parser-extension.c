/*
 * e-mail-parser-extension.c
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

#include "e-mail-parser-extension.h"

G_DEFINE_ABSTRACT_TYPE (
	EMailParserExtension,
	e_mail_parser_extension,
	G_TYPE_OBJECT)

static void
e_mail_parser_extension_class_init (EMailParserExtensionClass *class)
{
	class->priority = G_PRIORITY_DEFAULT;
}

static void
e_mail_parser_extension_init (EMailParserExtension *extension)
{
}

/**
 * e_mail_parser_extension_parse
 * @extension: an #EMailParserExtension
 * @parser: an #EMailParser
 * @mime_part: (allow-none) a #CamelMimePart to parse
 * @part_id: a #GString to which parser will append ID of the parsed part.
 * @cancellable: (allow-none) A #GCancellable
 * @out_mail_parts: a #GQueue to deposit #EMailPart instances
 *
 * A virtual function reimplemented in all mail parser extensions. The function
 * decodes and parses the @mime_part, appending one or more #EMailPart<!-- -->s
 * to the @out_mail_parts queue.
 *
 * When the function is unable to parse the @mime_part (either because it's
 * broken or because it is a different MIME type then the extension is
 * specialized for), the function will return %FALSE to indicate to the
 * #EMailParser that it should pick another extension.
 *
 * When the @mime_part contains for example multipart/mixed of one RFC822
 * message with an attachment and of one image, then parser must make sure
 * that parts are appeded to @out_mail_parts in the correct order.
 *
 * part1.rfc822.plain_text
 * part1.rfc822.attachment
 * part2.image
 *
 * Implementation of this function must be thread-safe.
 *
 * Returns: %TRUE if the @mime_part was handled (even if no
 *          #EMailPart<!-- -->s were added to @out_mail_parts), or
 *          %FALSE if the @mime_part was not handled
 */
gboolean
e_mail_parser_extension_parse (EMailParserExtension *extension,
                               EMailParser *parser,
                               CamelMimePart *mime_part,
                               GString *part_id,
                               GCancellable *cancellable,
                               GQueue *out_mail_parts)
{
	EMailParserExtensionClass *class;

	g_return_val_if_fail (E_IS_MAIL_PARSER_EXTENSION (extension), FALSE);
	g_return_val_if_fail (E_IS_MAIL_PARSER (parser), FALSE);

	class = E_MAIL_PARSER_EXTENSION_GET_CLASS (extension);
	g_return_val_if_fail (class != NULL, FALSE);
	g_return_val_if_fail (class->parse != NULL, FALSE);

	/* Check for cancellation before calling the method. */
	if (g_cancellable_is_cancelled (cancellable))
		return FALSE;

	return class->parse (
		extension, parser, mime_part, part_id,
		cancellable, out_mail_parts);
}

