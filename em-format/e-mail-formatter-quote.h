/*
 * e-mail-formatter-quote.h
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

#ifndef E_MAIL_FORMATTER_QUOTE_H
#define E_MAIL_FORMATTER_QUOTE_H

#include <em-format/e-mail-formatter.h>
#include <em-format/e-mail-formatter-extension.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_FORMATTER_QUOTE \
	(e_mail_formatter_quote_get_type ())
#define E_MAIL_FORMATTER_QUOTE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_FORMATTER_QUOTE, EMailFormatterQuote))
#define E_MAIL_FORMATTER_QUOTE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_FORMATTER_QUOTE, EMailFormatterQuoteClass))
#define E_IS_MAIL_FORMATTER_QUOTE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_FORMATTER_QUOTE))
#define E_IS_MAIL_FORMATTER_QUOTE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_FORMATTER_QUOTE))
#define E_MAIL_FORMATTER_QUOTE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_FORMATTER_QUOTE, EMailFormatterQuoteClass))

G_BEGIN_DECLS;

typedef struct _EMailFormatterQuote EMailFormatterQuote;
typedef struct _EMailFormatterQuoteClass EMailFormatterQuoteClass;
typedef struct _EMailFormatterQuotePrivate EMailFormatterQuotePrivate;
typedef struct _EMailFormatterQuoteContext EMailFormatterQuoteContext;

struct _EMailFormatterQuoteContext {
	EMailFormatterContext parent;

	guint32 qf_flags;
};

struct _EMailFormatterQuote {
	EMailFormatter parent;
	EMailFormatterQuotePrivate *priv;
};

struct _EMailFormatterQuoteClass {
	EMailFormatterClass parent_class;
};

GType		e_mail_formatter_quote_get_type	(void) G_GNUC_CONST;
EMailFormatter *
		e_mail_formatter_quote_new	(const gchar *credits,
						 EMailFormatterQuoteFlags flags);

G_END_DECLS

/* ------------------------------------------------------------------------- */

/* Standard GObject macros */
#define E_TYPE_MAIL_FORMATTER_QUOTE_EXTENSION \
	(e_mail_formatter_quote_extension_get_type ())

G_BEGIN_DECLS

/**
 * EMailFormatterQuoteExtension:
 *
 * This is an abstract base type for formatter extensions which are
 * intended only for use by #EMailFormatterQuote.
 **/
typedef struct _EMailFormatterQuoteExtension EMailFormatterQuoteExtension;
typedef struct _EMailFormatterQuoteExtensionClass EMailFormatterQuoteExtensionClass;

struct _EMailFormatterQuoteExtension {
	EMailFormatterExtension parent;
};

struct _EMailFormatterQuoteExtensionClass {
	EMailFormatterExtensionClass parent_class;
};

GType		e_mail_formatter_quote_extension_get_type
						(void) G_GNUC_CONST;

G_END_DECLS

#endif /* E_MAIL_FORMATTER_QUOTE_H */
