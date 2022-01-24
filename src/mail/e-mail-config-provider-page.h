/*
 * e-mail-config-provider-page.h
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

/* This is derived from EMailConfigActivityPage mainly as a convenience
 * for Evolution-EWS, which queries available address books asynchronously. */

#ifndef E_MAIL_CONFIG_PROVIDER_PAGE_H
#define E_MAIL_CONFIG_PROVIDER_PAGE_H

#include <gtk/gtk.h>
#include <camel/camel.h>

#include <e-util/e-util.h>
#include <mail/e-mail-config-page.h>
#include <mail/e-mail-config-activity-page.h>
#include <mail/e-mail-config-service-backend.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_CONFIG_PROVIDER_PAGE \
	(e_mail_config_provider_page_get_type ())
#define E_MAIL_CONFIG_PROVIDER_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_CONFIG_PROVIDER_PAGE, EMailConfigProviderPage))
#define E_MAIL_CONFIG_PROVIDER_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_CONFIG_PROVIDER_PAGE, EMailConfigProviderPageClass))
#define E_IS_MAIL_CONFIG_PROVIDER_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_CONFIG_PROVIDER_PAGE))
#define E_IS_MAIL_CONFIG_PROVIDER_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_CONFIG_PROVIDER_PAGE))
#define E_MAIL_CONFIG_PROVIDER_PAGE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_CONFIG_PROVIDER_PAGE, EMailConfigProviderPageClass))

#define E_MAIL_CONFIG_PROVIDER_PAGE_SORT_ORDER (300)

G_BEGIN_DECLS

typedef struct _EMailConfigProviderPage EMailConfigProviderPage;
typedef struct _EMailConfigProviderPageClass EMailConfigProviderPageClass;
typedef struct _EMailConfigProviderPagePrivate EMailConfigProviderPagePrivate;

struct _EMailConfigProviderPage {
	EMailConfigActivityPage parent;
	EMailConfigProviderPagePrivate *priv;
};

struct _EMailConfigProviderPageClass {
	EMailConfigActivityPageClass parent_class;
};

GType		e_mail_config_provider_page_get_type
					(void) G_GNUC_CONST;
EMailConfigPage *
		e_mail_config_provider_page_new
					(EMailConfigServiceBackend *backend);
gboolean	e_mail_config_provider_page_is_empty
					(EMailConfigProviderPage *page);
EMailConfigServiceBackend *
		e_mail_config_provider_page_get_backend
					(EMailConfigProviderPage *page);
GtkBox *	e_mail_config_provider_page_get_placeholder
					(EMailConfigProviderPage *page,
					 const gchar *name);

void		e_mail_config_provider_add_widgets
					(CamelProvider *provider,
					 CamelSettings *settings,
					 GtkBox *main_box,
					 gboolean skip_first_section_name);

G_END_DECLS

#endif /* E_MAIL_CONFIG_PROVIDER_PAGE_H */
