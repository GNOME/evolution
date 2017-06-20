/*
 * e-mail-config-import-progress-page.h
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

#ifndef E_MAIL_CONFIG_IMPORT_PROGRESS_PAGE_H
#define E_MAIL_CONFIG_IMPORT_PROGRESS_PAGE_H

#include <gtk/gtk.h>

#include <mail/e-mail-config-page.h>

#include "e-mail-config-import-page.h"

/* Standard GObject macros */
#define E_TYPE_MAIL_CONFIG_IMPORT_PROGRESS_PAGE \
	(e_mail_config_import_progress_page_get_type ())
#define E_MAIL_CONFIG_IMPORT_PROGRESS_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_CONFIG_IMPORT_PROGRESS_PAGE, EMailConfigImportProgressPage))
#define E_MAIL_CONFIG_IMPORT_PROGRESS_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_CONFIG_IMPORT_PROGRESS_PAGE, EMailConfigImportProgressPageClass))
#define E_IS_MAIL_CONFIG_IMPORT_PROGRESS_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_CONFIG_IMPORT_PROGRESS_PAGE))
#define E_IS_MAIL_CONFIG_IMPORT_PROGRESS_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_CONFIG_IMPORT_PROGRESS_PAGE))
#define E_MAIL_CONFIG_IMPORT_PROGRESS_PAGE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_CONFIG_IMPORT_PROGRESS_PAGE, EMailConfigImportProgressPageClass))

/* Sort the page in terms of the page we want to appear after. */
#define E_MAIL_CONFIG_IMPORT_PROGRESS_PAGE_SORT_ORDER \
	(E_MAIL_CONFIG_IMPORT_PAGE_SORT_ORDER + 1)

G_BEGIN_DECLS

typedef struct _EMailConfigImportProgressPage EMailConfigImportProgressPage;
typedef struct _EMailConfigImportProgressPageClass EMailConfigImportProgressPageClass;
typedef struct _EMailConfigImportProgressPagePrivate EMailConfigImportProgressPagePrivate;

struct _EMailConfigImportProgressPage {
	GtkScrolledWindow parent;
	EMailConfigImportProgressPagePrivate *priv;
};

struct _EMailConfigImportProgressPageClass {
	GtkScrolledWindowClass parent_class;
};

GType		e_mail_config_import_progress_page_get_type
					(void) G_GNUC_CONST;
void		e_mail_config_import_progress_page_type_register
					(GTypeModule *type_module);
EMailConfigPage *
		e_mail_config_import_progress_page_new
					(EActivity *activity);
EActivity *
		e_mail_config_import_progress_page_get_activity
					(EMailConfigImportProgressPage *page);

G_END_DECLS

#endif /* E_MAIL_CONFIG_IMPORT_PROGRESS_PAGE_H */

