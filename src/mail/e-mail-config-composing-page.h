/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_MAIL_CONFIG_COMPOSING_PAGE_H
#define E_MAIL_CONFIG_COMPOSING_PAGE_H

#include <gtk/gtk.h>
#include <libemail-engine/libemail-engine.h>

#include <mail/e-mail-config-page.h>
#include <mail/e-mail-config-activity-page.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_CONFIG_COMPOSING_PAGE \
	(e_mail_config_composing_page_get_type ())
#define E_MAIL_CONFIG_COMPOSING_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_CONFIG_COMPOSING_PAGE, EMailConfigComposingPage))
#define E_MAIL_CONFIG_COMPOSING_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_CONFIG_COMPOSING_PAGE, EMailConfigComposingPageClass))
#define E_IS_MAIL_CONFIG_COMPOSING_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_CONFIG_COMPOSING_PAGE))
#define E_IS_MAIL_CONFIG_COMPOSING_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_CONFIG_COMPOSING_PAGE))
#define E_MAIL_CONFIG_COMPOSING_PAGE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_CONFIG_COMPOSING_PAGE, EMailConfigComposingPageClass))

#define E_MAIL_CONFIG_COMPOSING_PAGE_SORT_ORDER (550)

G_BEGIN_DECLS

typedef struct _EMailConfigComposingPage EMailConfigComposingPage;
typedef struct _EMailConfigComposingPageClass EMailConfigComposingPageClass;
typedef struct _EMailConfigComposingPagePrivate EMailConfigComposingPagePrivate;

struct _EMailConfigComposingPage {
	EMailConfigActivityPage parent;
	EMailConfigComposingPagePrivate *priv;
};

struct _EMailConfigComposingPageClass {
	EMailConfigActivityPageClass parent_class;
};

GType		e_mail_config_composing_page_get_type
						(void) G_GNUC_CONST;
EMailConfigPage *
		e_mail_config_composing_page_new
						(ESource *identity_source);
ESource *	e_mail_config_composing_page_get_identity_source
						(EMailConfigComposingPage *page);

G_END_DECLS

#endif /* E_MAIL_CONFIG_COMPOSING_PAGE_H */
