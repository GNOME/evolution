/*
 * e-mail-config-import-page.h
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

#ifndef E_MAIL_CONFIG_IMPORT_PAGE_H
#define E_MAIL_CONFIG_IMPORT_PAGE_H

#include <gtk/gtk.h>

#include <e-util/e-util.h>

#include <mail/e-mail-config-page.h>
#include <mail/e-mail-config-summary-page.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_CONFIG_IMPORT_PAGE \
	(e_mail_config_import_page_get_type ())
#define E_MAIL_CONFIG_IMPORT_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_CONFIG_IMPORT_PAGE, EMailConfigImportPage))
#define E_MAIL_CONFIG_IMPORT_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_CONFIG_IMPORT_PAGE, EMailConfigImportPageClass))
#define E_IS_MAIL_CONFIG_IMPORT_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_CONFIG_IMPORT_PAGE))
#define E_IS_MAIL_CONFIG_IMPORT_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_CONFIG_IMPORT_PAGE))
#define E_MAIL_CONFIG_IMPORT_PAGE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_CONFIG_IMPORT_PAGE, EMailConfigImportPageClass))

/* Sort the page in terms of the page we want to appear after. */
#define E_MAIL_CONFIG_IMPORT_PAGE_SORT_ORDER \
	(E_MAIL_CONFIG_SUMMARY_PAGE_SORT_ORDER + 10)

G_BEGIN_DECLS

typedef struct _EMailConfigImportPage EMailConfigImportPage;
typedef struct _EMailConfigImportPageClass EMailConfigImportPageClass;
typedef struct _EMailConfigImportPagePrivate EMailConfigImportPagePrivate;

struct _EMailConfigImportPage {
	GtkScrolledWindow parent;
	EMailConfigImportPagePrivate *priv;
};

struct _EMailConfigImportPageClass {
	GtkScrolledWindowClass parent_class;
};

GType		e_mail_config_import_page_get_type
						(void) G_GNUC_CONST;
void		e_mail_config_import_page_type_register
						(GTypeModule *type_module);
EMailConfigPage *
		e_mail_config_import_page_new	(void);
guint		e_mail_config_import_page_get_n_importers
						(EMailConfigImportPage *page);
void		e_mail_config_import_page_import
						(EMailConfigImportPage *page,
						 EActivity *activity,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
gboolean	e_mail_config_import_page_import_finish
						(EMailConfigImportPage *page,
						 GAsyncResult *result,
						 GError **error);

G_END_DECLS

#endif /* E_MAIL_CONFIG_IMPORT_PAGE_H */
