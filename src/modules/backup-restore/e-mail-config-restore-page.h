/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_MAIL_CONFIG_RESTORE_PAGE_H
#define E_MAIL_CONFIG_RESTORE_PAGE_H

#include <gtk/gtk.h>

#include <mail/e-mail-config-page.h>
#include <mail/e-mail-config-welcome-page.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_CONFIG_RESTORE_PAGE \
	(e_mail_config_restore_page_get_type ())
#define E_MAIL_CONFIG_RESTORE_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_CONFIG_RESTORE_PAGE, EMailConfigRestorePage))
#define E_MAIL_CONFIG_RESTORE_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_CONFIG_RESTORE_PAGE, EMailConfigRestorePageClass))
#define E_IS_MAIL_CONFIG_RESTORE_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_CONFIG_RESTORE_PAGE))
#define E_IS_MAIL_CONFIG_RESTORE_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_CONFIG_RESTORE_PAGE))
#define E_MAIL_CONFIG_RESTORE_PAGE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_CONFIG_RESTORE_PAGE, EMailConfigRestorePageClass))

#define E_MAIL_CONFIG_RESTORE_PAGE_SORT_ORDER \
	(E_MAIL_CONFIG_WELCOME_PAGE_SORT_ORDER + 10)

G_BEGIN_DECLS

typedef struct _EMailConfigRestorePage EMailConfigRestorePage;
typedef struct _EMailConfigRestorePageClass EMailConfigRestorePageClass;
typedef struct _EMailConfigRestorePagePrivate EMailConfigRestorePagePrivate;

struct _EMailConfigRestorePage {
	GtkScrolledWindow parent;
	EMailConfigRestorePagePrivate *priv;
};

struct _EMailConfigRestorePageClass {
	GtkScrolledWindowClass parent_class;
};

GType		e_mail_config_restore_page_get_type
						(void) G_GNUC_CONST;
void		e_mail_config_restore_page_type_register
						(GTypeModule *type_module);
EMailConfigPage *
		e_mail_config_restore_page_new	(void);
const gchar *	e_mail_config_restore_page_get_filename
						(EMailConfigRestorePage *page);

/* This is a stand-alone function to validate the given backup file.
 * It resides in this file because EMailConfigRestorePage uses it. */
gboolean	evolution_backup_restore_validate_backup_file
						(const gchar *filename,
						 GError **error);
gboolean	evolution_backup_restore_check_prog_exists
						(const gchar *prog,
						 GError **error);

G_END_DECLS

#endif /* E_MAIL_CONFIG_RESTORE_PAGE_H */

