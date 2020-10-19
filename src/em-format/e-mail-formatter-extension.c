/*
 * e-mail-formatter-extension.c
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

#include "e-mail-formatter-extension.h"

G_DEFINE_ABSTRACT_TYPE (
	EMailFormatterExtension,
	e_mail_formatter_extension,
	G_TYPE_OBJECT)

static void
e_mail_formatter_extension_class_init (EMailFormatterExtensionClass *class)
{
	class->priority = G_PRIORITY_DEFAULT;
}

static void
e_mail_formatter_extension_init (EMailFormatterExtension *extension)
{
}

/**
 * e_mail_formatter_extension_format
 * @extension: an #EMailFormatterExtension
 * @formatter: an #EMailFormatter
 * @context: an #EMailFormatterContext
 * @part: an #EMailPart to be formatter
 * @stream: a #GOutputStream to which the output should be written
 * @cancellable: (allow-none) a #GCancellable
 *
 * A virtual function reimplemented in all mail formatter extensions. The
 * function formats @part, generated HTML (or other format that can be
 * displayed to user) and writes it to the @stream.
 *
 * When the function is unable to format the @part (either because it's broken
 * or because it is a different mimetype then the extension is specialized
 * for), the function will return @FALSE indicating the #EMailFormatter, that
 * it should pick another extension.
 *
 * Implementation of this function must be thread-safe.
 *
 * Returns: Returns @TRUE when the @part was successfully formatted and
 * data were written to the @stream, @FALSE otherwise.
 */
gboolean
e_mail_formatter_extension_format (EMailFormatterExtension *extension,
                                   EMailFormatter *formatter,
                                   EMailFormatterContext *context,
                                   EMailPart *part,
                                   GOutputStream *stream,
                                   GCancellable *cancellable)
{
	EMailFormatterExtensionClass *class;

	g_return_val_if_fail (E_IS_MAIL_FORMATTER_EXTENSION (extension), FALSE);
	g_return_val_if_fail (E_IS_MAIL_FORMATTER (formatter), FALSE);
	g_return_val_if_fail (context != NULL, FALSE);
	g_return_val_if_fail (part != NULL, FALSE);
	g_return_val_if_fail (G_IS_OUTPUT_STREAM (stream), FALSE);

	class = E_MAIL_FORMATTER_EXTENSION_GET_CLASS (extension);
	g_return_val_if_fail (class != NULL, FALSE);
	g_return_val_if_fail (class->format != NULL, FALSE);

	return class->format (extension, formatter, context, part, stream, cancellable);
}
