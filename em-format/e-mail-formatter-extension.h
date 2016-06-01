/*
 * e-mail-formatter-extension.h
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

#ifndef E_MAIL_FORMATTER_EXTENSION_H
#define E_MAIL_FORMATTER_EXTENSION_H

#include <gtk/gtk.h>
#include <camel/camel.h>
#include <em-format/e-mail-part.h>
#include <em-format/e-mail-formatter.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_FORMATTER_EXTENSION \
	(e_mail_formatter_extension_get_type ())
#define E_MAIL_FORMATTER_EXTENSION(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_FORMATTER_EXTENSION, EMailFormatterExtension))
#define E_MAIL_FORMATTER_EXTENSION_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_FORMATTER_EXTENSION, EMailFormatterExtensionClass))
#define E_IS_MAIL_FORMATTER_EXTENSION(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_FORMATTER_EXTENSION))
#define E_IS_MAIL_FORMATTER_EXTENSION_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_FORMATTER_EXTENSION))
#define E_MAIL_FORMATTER_EXTENSION_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_FORMATTER_EXTENSION, EMailFormatterExtensionClass))

G_BEGIN_DECLS

/**
 * EMailFormatterExtension:
 *
 * The #EMailFormatterExtension is an abstract class for all extensions for
 * #EMailFormatter.
 */
typedef struct _EMailFormatterExtension EMailFormatterExtension;
typedef struct _EMailFormatterExtensionClass EMailFormatterExtensionClass;
typedef struct _EMailFormatterExtensionPrivate EMailFormatterExtensionPrivate;

struct _EMailFormatterExtension {
	GObject parent;
	EMailFormatterExtensionPrivate *priv;
};

struct _EMailFormatterExtensionClass {
	GObjectClass parent_class;

	/* This is a short name for the extension (optional). */
	const gchar *display_name;

	/* This is a longer description of the extension (optional). */
	const gchar *description;

	/* This is a NULL-terminated array of supported MIME types.
	 * The MIME types can be exact (e.g. "text/plain") or use a
	 * wildcard (e.g. "text/ *"). */
	const gchar **mime_types;

	/* This is used to prioritize extensions with identical MIME
	 * types.  Lower values win.  Defaults to G_PRIORITY_DEFAULT. */
	gint priority;

	gboolean	(*format)	(EMailFormatterExtension *extension,
					 EMailFormatter *formatter,
					 EMailFormatterContext *context,
					 EMailPart *part,
					 GOutputStream *stream,
					 GCancellable *cancellable);
};

GType		e_mail_formatter_extension_get_type
						(void) G_GNUC_CONST;
gboolean	e_mail_formatter_extension_format
						(EMailFormatterExtension *extension,
						 EMailFormatter *formatter,
						 EMailFormatterContext *context,
						 EMailPart *part,
						 GOutputStream *stream,
						 GCancellable *cancellable);

G_END_DECLS

#endif /* E_MAIL_FORMATTER_EXTENSION_H */
