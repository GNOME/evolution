/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_MAIL_CONFIG_RESTORE_READY_PAGE_H
#define E_MAIL_CONFIG_RESTORE_READY_PAGE_H

#include <gtk/gtk.h>

#include <mail/e-mail-config-page.h>

#include "e-mail-config-restore-page.h"

/* Standard GObject macros */
#define E_TYPE_MAIL_CONFIG_RESTORE_READY_PAGE \
	(e_mail_config_restore_ready_page_get_type ())
#define E_MAIL_CONFIG_RESTORE_READY_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_CONFIG_RESTORE_READY_PAGE, EMailConfigRestoreReadyPage))
#define E_MAIL_CONFIG_RESTORE_READY_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_CONFIG_RESTORE_READY_PAGE, EMailConfigRestoreReadyPageClass))
#define E_IS_MAIL_CONFIG_RESTORE_READY_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_CONFIG_RESTORE_READY_PAGE))
#define E_IS_MAIL_CONFIG_RESTORE_READY_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_CONFIG_RESTORE_READY_PAGE))
#define E_MAIL_CONFIG_RESTORE_READY_PAGE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_CONFIG_RESTORE_READY_PAGE, EMailConfigRestoreReadyPageClass))

#define E_MAIL_CONFIG_RESTORE_READY_PAGE_SORT_ORDER \
	(E_MAIL_CONFIG_RESTORE_PAGE_SORT_ORDER + 1)

G_BEGIN_DECLS

typedef struct _EMailConfigRestoreReadyPage EMailConfigRestoreReadyPage;
typedef struct _EMailConfigRestoreReadyPageClass EMailConfigRestoreReadyPageClass;
typedef struct _EMailConfigRestoreReadyPagePrivate EMailConfigRestoreReadyPagePrivate;

struct _EMailConfigRestoreReadyPage {
	GtkScrolledWindow parent;
	EMailConfigRestoreReadyPagePrivate *priv;
};

struct _EMailConfigRestoreReadyPageClass {
	GtkScrolledWindowClass parent_class;
};

GType		e_mail_config_restore_ready_page_get_type
						(void) G_GNUC_CONST;
void		e_mail_config_restore_ready_page_type_register
						(GTypeModule *type_module);
EMailConfigPage *
		e_mail_config_restore_ready_page_new
						(void);

G_END_DECLS

#endif /* E_MAIL_CONFIG_RESTORE_READY_PAGE_H */

