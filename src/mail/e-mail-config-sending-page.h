/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_MAIL_CONFIG_SENDING_PAGE_H
#define E_MAIL_CONFIG_SENDING_PAGE_H

#include <mail/e-mail-config-page.h>
#include <mail/e-mail-config-service-page.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_CONFIG_SENDING_PAGE \
	(e_mail_config_sending_page_get_type ())
#define E_MAIL_CONFIG_SENDING_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_CONFIG_SENDING_PAGE, EMailConfigSendingPage))
#define E_MAIL_CONFIG_SENDING_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_CONFIG_SENDING_PAGE, EMailConfigSendingPageClass))
#define E_IS_MAIL_CONFIG_SENDING_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_CONFIG_SENDING_PAGE))
#define E_IS_MAIL_CONFIG_SENDING_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_CONFIG_SENDING_PAGE))
#define E_MAIL_CONFIG_SENDING_PAGE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_CONFIG_SENDING_PAGE, EMailConfigSendingPageClass))

#define E_MAIL_CONFIG_SENDING_PAGE_SORT_ORDER (400)

G_BEGIN_DECLS

typedef struct _EMailConfigSendingPage EMailConfigSendingPage;
typedef struct _EMailConfigSendingPageClass EMailConfigSendingPageClass;
typedef struct _EMailConfigSendingPagePrivate EMailConfigSendingPagePrivate;

struct _EMailConfigSendingPage {
	EMailConfigServicePage parent;
};

struct _EMailConfigSendingPageClass {
	EMailConfigServicePageClass parent_class;
};

GType		e_mail_config_sending_page_get_type
						(void) G_GNUC_CONST;
EMailConfigPage *
		e_mail_config_sending_page_new	(ESourceRegistry *registry);

G_END_DECLS

#endif /* E_MAIL_CONFIG_SENDING_PAGE_H */

