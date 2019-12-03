/*
 * e-mail-config-summary-page.h
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

#ifndef E_MAIL_CONFIG_SUMMARY_PAGE_H
#define E_MAIL_CONFIG_SUMMARY_PAGE_H

#include <gtk/gtk.h>

#include <mail/e-mail-config-page.h>
#include <mail/e-mail-config-service-backend.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_CONFIG_SUMMARY_PAGE \
	(e_mail_config_summary_page_get_type ())
#define E_MAIL_CONFIG_SUMMARY_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_CONFIG_SUMMARY_PAGE, EMailConfigSummaryPage))
#define E_MAIL_CONFIG_SUMMARY_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_CONFIG_SUMMARY_PAGE, EMailConfigSummaryPageClass))
#define E_IS_MAIL_CONFIG_SUMMARY_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_CONFIG_SUMMARY_PAGE))
#define E_IS_MAIL_CONFIG_SUMMARY_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_CONFIG_SUMMARY_PAGE))
#define E_MAIL_CONFIG_SUMMARY_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_CONFIG_SUMMARY_PAGE, EMailConfigSummaryPageClass))

#define E_MAIL_CONFIG_SUMMARY_PAGE_SORT_ORDER (500)

G_BEGIN_DECLS

typedef struct _EMailConfigSummaryPage EMailConfigSummaryPage;
typedef struct _EMailConfigSummaryPageClass EMailConfigSummaryPageClass;
typedef struct _EMailConfigSummaryPagePrivate EMailConfigSummaryPagePrivate;

struct _EMailConfigSummaryPage {
	GtkScrolledWindow parent;
	EMailConfigSummaryPagePrivate *priv;
};

struct _EMailConfigSummaryPageClass {
	GtkScrolledWindowClass parent_class;

	/* Signals */
	void		(*refresh)		(EMailConfigSummaryPage *page);
};

GType		e_mail_config_summary_page_get_type
						(void) G_GNUC_CONST;
EMailConfigPage *
		e_mail_config_summary_page_new	(void);
GtkBox *	e_mail_config_summary_page_get_internal_box
						(EMailConfigSummaryPage *page);
const gchar *	e_mail_config_summary_page_get_account_name
						(EMailConfigSummaryPage *page);
void		e_mail_config_summary_page_refresh
						(EMailConfigSummaryPage *page);
EMailConfigServiceBackend *
		e_mail_config_summary_page_get_account_backend
						(EMailConfigSummaryPage *page);
void		e_mail_config_summary_page_set_account_backend
						(EMailConfigSummaryPage *page,
						 EMailConfigServiceBackend *backend);
ESource *	e_mail_config_summary_page_get_account_source
						(EMailConfigSummaryPage *page);
ESource *	e_mail_config_summary_page_get_identity_source
						(EMailConfigSummaryPage *page);
void		e_mail_config_summary_page_set_identity_source
						(EMailConfigSummaryPage *page,
						 ESource *identity_source);
EMailConfigServiceBackend *
		e_mail_config_summary_page_get_transport_backend
						(EMailConfigSummaryPage *page);
void		e_mail_config_summary_page_set_transport_backend
						(EMailConfigSummaryPage *page,
						 EMailConfigServiceBackend *backend);
ESource *	e_mail_config_summary_page_get_transport_source
						(EMailConfigSummaryPage *page);

#endif /* E_MAIL_CONFIG_SUMMARY_PAGE_H */

