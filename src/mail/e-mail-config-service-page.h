/*
 * e-mail-config-service-page.h
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

/* XXX This is very similar to ESourceConfig for address books and
 *     calendars, but not similar enough to easily unify the APIs.
 *     Probably with more thought and effort it could be done. */

#ifndef E_MAIL_CONFIG_SERVICE_PAGE_H
#define E_MAIL_CONFIG_SERVICE_PAGE_H

#include <camel/camel.h>

#include <e-util/e-util.h>
#include <mail/e-mail-config-activity-page.h>
#include <mail/e-mail-config-service-backend.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_CONFIG_SERVICE_PAGE \
	(e_mail_config_service_page_get_type ())
#define E_MAIL_CONFIG_SERVICE_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_CONFIG_SERVICE_PAGE, EMailConfigServicePage))
#define E_MAIL_CONFIG_SERVICE_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_CONFIG_SERVICE_PAGE, EMailConfigServicePageClass))
#define E_IS_MAIL_CONFIG_SERVICE_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_CONFIG_SERVICE_PAGE))
#define E_IS_MAIL_CONFIG_SERVICE_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_CONFIG_SERVICE_PAGE))
#define E_MAIL_CONFIG_SERVICE_PAGE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_CONFIG_SERVICE_PAGE, EMailConfigServicePageClass))

G_BEGIN_DECLS

typedef struct _EMailConfigServicePage EMailConfigServicePage;
typedef struct _EMailConfigServicePageClass EMailConfigServicePageClass;
typedef struct _EMailConfigServicePagePrivate EMailConfigServicePagePrivate;

struct _EMailConfigServicePage {
	EMailConfigActivityPage parent;
	EMailConfigServicePagePrivate *priv;
};

struct _EMailConfigServicePageClass {
	EMailConfigActivityPageClass parent_class;

	const gchar *extension_name;
	CamelProviderType provider_type;
	const gchar *default_backend_name;
};

GType		e_mail_config_service_page_get_type
						(void) G_GNUC_CONST;
EMailConfigServiceBackend *
		e_mail_config_service_page_get_active_backend
						(EMailConfigServicePage *page);
void		e_mail_config_service_page_set_active_backend
						(EMailConfigServicePage *page,
						 EMailConfigServiceBackend *backend);
const gchar *	e_mail_config_service_page_get_email_address
						(EMailConfigServicePage *page);
void		e_mail_config_service_page_set_email_address
						(EMailConfigServicePage *page,
						 const gchar *email_address);
ESourceRegistry *
		e_mail_config_service_page_get_registry
						(EMailConfigServicePage *page);
EMailConfigServiceBackend *
		e_mail_config_service_page_add_scratch_source
						(EMailConfigServicePage *page,
						 ESource *scratch_source,
						 ESource *opt_collection);
EMailConfigServiceBackend *
		e_mail_config_service_page_lookup_backend
						(EMailConfigServicePage *page,
						 const gchar *backend_name);
gboolean	e_mail_config_service_page_auto_configure
						(EMailConfigServicePage *page,
						 EConfigLookup *config_lookup,
						 gboolean *out_is_complete);

G_END_DECLS

#endif /* E_MAIL_CONFIG_SERVICE_PAGE_H */

