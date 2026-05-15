/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_MAIL_CONFIG_SECURITY_PAGE_H
#define E_MAIL_CONFIG_SECURITY_PAGE_H

#include <gtk/gtk.h>
#include <libedataserver/libedataserver.h>

#include <mail/e-mail-config-page.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_CONFIG_SECURITY_PAGE \
	(e_mail_config_security_page_get_type ())
#define E_MAIL_CONFIG_SECURITY_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_CONFIG_SECURITY_PAGE, EMailConfigSecurityPage))
#define E_MAIL_CONFIG_SECURITY_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_CONFIG_SECURITY_PAGE, EMailConfigSecurityPageClass))
#define E_IS_MAIL_CONFIG_SECURITY_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_CONFIG_SECURITY_PAGE))
#define E_IS_MAIL_CONFIG_SECURITY_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_CONFIG_SECURITY_PAGE))
#define E_MAIL_CONFIG_SECURITY_PAGE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_CONFIG_SECURITY_PAGE, EMailConfigSecurityPageClass))

#define E_MAIL_CONFIG_SECURITY_PAGE_SORT_ORDER (600)

G_BEGIN_DECLS

typedef struct _EMailConfigSecurityPage EMailConfigSecurityPage;
typedef struct _EMailConfigSecurityPageClass EMailConfigSecurityPageClass;
typedef struct _EMailConfigSecurityPagePrivate EMailConfigSecurityPagePrivate;

struct _EMailConfigSecurityPage {
	GtkScrolledWindow parent;
	EMailConfigSecurityPagePrivate *priv;
};

struct _EMailConfigSecurityPageClass {
	GtkScrolledWindowClass parent_class;
};

GType		e_mail_config_security_page_get_type
						(void) G_GNUC_CONST;
EMailConfigPage *
		e_mail_config_security_page_new	(ESource *identity_source);
ESource *	e_mail_config_security_page_get_identity_source
						(EMailConfigSecurityPage *page);

G_END_DECLS

#endif /* E_MAIL_CONFIG_SECURITY_PAGE_H */

