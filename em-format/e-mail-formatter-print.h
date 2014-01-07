/*
 * e-mail-formatter-print.h
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

#ifndef E_MAIL_FORMATTER_PRINT_H
#define E_MAIL_FORMATTER_PRINT_H

#include <em-format/e-mail-formatter.h>
#include <em-format/e-mail-formatter-extension.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_FORMATTER_PRINT \
	(e_mail_formatter_print_get_type ())
#define E_MAIL_FORMATTER_PRINT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_FORMATTER_PRINT, EMailFormatterPrint))
#define E_MAIL_FORMATTER_PRINT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_FORMATTER_PRINT, EMailFormatterPrintClass))
#define E_IS_MAIL_FORMATTER_PRINT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_FORMATTER_PRINT))
#define E_IS_MAIL_FORMATTER_PRINT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_FORMATTER_PRINT))
#define E_MAIL_FORMATTER_PRINT_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_FORMATTER_PRINT, EMailFormatterPrintClass))

G_BEGIN_DECLS;

typedef struct _EMailFormatterPrint EMailFormatterPrint;
typedef struct _EMailFormatterPrintClass EMailFormatterPrintClass;

struct _EMailFormatterPrint {
	EMailFormatter parent;
};

struct _EMailFormatterPrintClass {
	EMailFormatterClass parent_class;
};

GType		e_mail_formatter_print_get_type	(void) G_GNUC_CONST;
EMailFormatter *
		e_mail_formatter_print_new	(void);

G_END_DECLS

/* ------------------------------------------------------------------------- */

/* Standard GObject macros */
#define E_TYPE_MAIL_FORMATTER_PRINT_EXTENSION \
	(e_mail_formatter_print_extension_get_type ())

G_BEGIN_DECLS

/**
 * EMailFormatterPrintExtension:
 *
 * This is an abstract base type for formatter extensions which are
 * intended only for use by #EMailFormatterPrint.
 **/
typedef struct _EMailFormatterPrintExtension EMailFormatterPrintExtension;
typedef struct _EMailFormatterPrintExtensionClass EMailFormatterPrintExtensionClass;

struct _EMailFormatterPrintExtension {
	EMailFormatterExtension parent;
};

struct _EMailFormatterPrintExtensionClass {
	EMailFormatterExtensionClass parent_class;
};

GType		e_mail_formatter_print_extension_get_type
						(void) G_GNUC_CONST;

G_END_DECLS

#endif /* E_MAIL_FORMATTER_PRINT_H */
