/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_MAIL_PRINT_CONFIG_HEADERS_H
#define E_MAIL_PRINT_CONFIG_HEADERS_H

#include <e-util/e-util.h>
#include <em-format/e-mail-part-headers.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_PRINT_CONFIG_HEADERS \
	(e_mail_print_config_headers_get_type ())
#define E_MAIL_PRINT_CONFIG_HEADERS(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_PRINT_CONFIG_HEADERS, EMailPrintConfigHeaders))
#define E_MAIL_PRINT_CONFIG_HEADERS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_PRINT_CONFIG_HEADERS, EMailPrintConfigHeadersClass))
#define E_IS_MAIL_PRINT_CONFIG_HEADERS(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_PRINT_CONFIG_HEADERS))
#define E_IS_MAIL_PRINT_CONFIG_HEADERS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_PRINT_CONFIG_HEADERS))
#define E_MAIL_PRINT_CONFIG_HEADERS_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_PRINT_CONFIG_HEADERS, EMailPrintConfigHeadersClass))

G_BEGIN_DECLS

typedef struct _EMailPrintConfigHeaders EMailPrintConfigHeaders;
typedef struct _EMailPrintConfigHeadersClass EMailPrintConfigHeadersClass;
typedef struct _EMailPrintConfigHeadersPrivate EMailPrintConfigHeadersPrivate;

struct _EMailPrintConfigHeaders {
	ETreeViewFrame parent;
	EMailPrintConfigHeadersPrivate *priv;
};

struct _EMailPrintConfigHeadersClass {
	ETreeViewFrameClass parent_class;
};

GType		e_mail_print_config_headers_get_type
					(void) G_GNUC_CONST;
GtkWidget *	e_mail_print_config_headers_new
					(EMailPartHeaders *part);
EMailPartHeaders *
		e_mail_print_config_headers_ref_part
					(EMailPrintConfigHeaders *config);

G_END_DECLS

#endif /* E_MAIL_PRINT_CONFIG_HEADERS_H */

