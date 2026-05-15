/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_MAIL_CONFIG_LOOKUP_PAGE_H
#define E_MAIL_CONFIG_LOOKUP_PAGE_H

#include <gtk/gtk.h>

#include <mail/e-mail-config-identity-page.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_CONFIG_LOOKUP_PAGE \
	(e_mail_config_lookup_page_get_type ())
#define E_MAIL_CONFIG_LOOKUP_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_CONFIG_LOOKUP_PAGE, EMailConfigLookupPage))
#define E_MAIL_CONFIG_LOOKUP_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_CONFIG_LOOKUP_PAGE, EMailConfigLookupPageClass))
#define E_IS_MAIL_CONFIG_LOOKUP_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_CONFIG_LOOKUP_PAGE))
#define E_IS_MAIL_CONFIG_LOOKUP_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_CONFIG_LOOKUP_PAGE))
#define E_MAIL_CONFIG_LOOKUP_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_CONFIG_LOOKUP_PAGE, EMailConfigLookupPageClass))

/* Since this is a transient page we define the sort order
 * as the previous page's sort order plus a small offset. */
#define E_MAIL_CONFIG_LOOKUP_PAGE_SORT_ORDER \
	(E_MAIL_CONFIG_IDENTITY_PAGE_SORT_ORDER + 10)

G_BEGIN_DECLS

typedef struct _EMailConfigLookupPage EMailConfigLookupPage;
typedef struct _EMailConfigLookupPageClass EMailConfigLookupPageClass;
typedef struct _EMailConfigLookupPagePrivate EMailConfigLookupPagePrivate;

struct _EMailConfigLookupPage {
	GtkScrolledWindow parent;
	EMailConfigLookupPagePrivate *priv;
};

struct _EMailConfigLookupPageClass {
	GtkScrolledWindowClass parent_class;
};

GType		e_mail_config_lookup_page_get_type
						(void) G_GNUC_CONST;
EMailConfigPage *
		e_mail_config_lookup_page_new	(void);

G_END_DECLS

#endif /* E_MAIL_CONFIG_LOOKUP_PAGE_H */

