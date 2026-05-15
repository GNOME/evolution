/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_MAIL_CONFIG_RECEIVING_PAGE_H
#define E_MAIL_CONFIG_RECEIVING_PAGE_H

#include <mail/e-mail-config-page.h>
#include <mail/e-mail-config-service-page.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_CONFIG_RECEIVING_PAGE \
	(e_mail_config_receiving_page_get_type ())
#define E_MAIL_CONFIG_RECEIVING_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_CONFIG_RECEIVING_PAGE, EMailConfigReceivingPage))
#define E_MAIL_CONFIG_RECEIVING_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_CONFIG_RECEIVING_PAGE, EMailConfigReceivingPageClass))
#define E_IS_MAIL_CONFIG_RECEIVING_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_CONFIG_RECEIVING_PAGE))
#define E_IS_MAIL_CONFIG_RECEIVING_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_CONFIG_RECEIVING_PAGE))
#define E_MAIL_CONFIG_RECEIVING_PAGE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_CONFIG_RECEIVING_PAGE, EMailConfigReceivingPageClass))

#define E_MAIL_CONFIG_RECEIVING_PAGE_SORT_ORDER (200)

G_BEGIN_DECLS

typedef struct _EMailConfigReceivingPage EMailConfigReceivingPage;
typedef struct _EMailConfigReceivingPageClass EMailConfigReceivingPageClass;
typedef struct _EMailConfigReceivingPagePrivate EMailConfigReceivingPagePrivate;

struct _EMailConfigReceivingPage {
	EMailConfigServicePage parent;
};

struct _EMailConfigReceivingPageClass {
	EMailConfigServicePageClass parent_class;
};

GType		e_mail_config_receiving_page_get_type
						(void) G_GNUC_CONST;
EMailConfigPage *
		e_mail_config_receiving_page_new
						(ESourceRegistry *registry);

G_END_DECLS

#endif /* E_MAIL_CONFIG_RECEIVING_PAGE_H */

