/*
 * e-mail-config-defaults-page.h
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

#ifndef E_MAIL_CONFIG_DEFAULTS_PAGE_H
#define E_MAIL_CONFIG_DEFAULTS_PAGE_H

#include <gtk/gtk.h>
#include <libemail-engine/libemail-engine.h>

#include <mail/e-mail-config-page.h>
#include <mail/e-mail-config-activity-page.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_CONFIG_DEFAULTS_PAGE \
	(e_mail_config_defaults_page_get_type ())
#define E_MAIL_CONFIG_DEFAULTS_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_CONFIG_DEFAULTS_PAGE, EMailConfigDefaultsPage))
#define E_MAIL_CONFIG_DEFAULTS_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_CONFIG_DEFAULTS_PAGE, EMailConfigDefaultsPageClass))
#define E_IS_MAIL_CONFIG_DEFAULTS_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_CONFIG_DEFAULTS_PAGE))
#define E_IS_MAIL_CONFIG_DEFAULTS_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_CONFIG_DEFAULTS_PAGE))
#define E_MAIL_CONFIG_DEFAULTS_PAGE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_CONFIG_DEFAULTS_PAGE, EMailConfigDefaultsPageClass))

#define E_MAIL_CONFIG_DEFAULTS_PAGE_SORT_ORDER (500)

G_BEGIN_DECLS

typedef struct _EMailConfigDefaultsPage EMailConfigDefaultsPage;
typedef struct _EMailConfigDefaultsPageClass EMailConfigDefaultsPageClass;
typedef struct _EMailConfigDefaultsPagePrivate EMailConfigDefaultsPagePrivate;

struct _EMailConfigDefaultsPage {
	EMailConfigActivityPage parent;
	EMailConfigDefaultsPagePrivate *priv;
};

struct _EMailConfigDefaultsPageClass {
	EMailConfigActivityPageClass parent_class;
};

GType		e_mail_config_defaults_page_get_type
						(void) G_GNUC_CONST;
EMailConfigPage *
		e_mail_config_defaults_page_new	(EMailSession *session,
						 ESource *original_source,
						 ESource *collection_source,
						 ESource *account_source,
						 ESource *identity_source,
						 ESource *transport_source);
EMailSession *	e_mail_config_defaults_page_get_session
						(EMailConfigDefaultsPage *page);
ESource *	e_mail_config_defaults_page_get_account_source
						(EMailConfigDefaultsPage *page);
ESource *	e_mail_config_defaults_page_get_collection_source
						(EMailConfigDefaultsPage *page);
ESource *	e_mail_config_defaults_page_get_identity_source
						(EMailConfigDefaultsPage *page);
ESource *	e_mail_config_defaults_page_get_original_source
						(EMailConfigDefaultsPage *page);
ESource *	e_mail_config_defaults_page_get_transport_source
						(EMailConfigDefaultsPage *page);

G_END_DECLS

#endif /* E_MAIL_CONFIG_DEFAULTS_PAGE_H */

