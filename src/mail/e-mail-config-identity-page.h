/*
 * e-mail-config-identity-page.h
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

#ifndef E_MAIL_CONFIG_IDENTITY_PAGE_H
#define E_MAIL_CONFIG_IDENTITY_PAGE_H

#include <gtk/gtk.h>
#include <libedataserver/libedataserver.h>

#include <mail/e-mail-config-page.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_CONFIG_IDENTITY_PAGE \
	(e_mail_config_identity_page_get_type ())
#define E_MAIL_CONFIG_IDENTITY_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_CONFIG_IDENTITY_PAGE, EMailConfigIdentityPage))
#define E_MAIL_CONFIG_IDENTITY_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_CONFIG_IDENTITY_PAGE, EMailConfigIdentityPageClass))
#define E_IS_MAIL_CONFIG_IDENTITY_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_CONFIG_IDENTITY_PAGE))
#define E_IS_MAIL_CONFIG_IDENTITY_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_CONFIG_IDENTITY_PAGE))
#define E_MAIL_CONFIG_IDENTITY_PAGE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_CONFIG_IDENTITY_PAGE, EMailConfigIdentityPageClass))

#define E_MAIL_CONFIG_IDENTITY_PAGE_SORT_ORDER (100)

G_BEGIN_DECLS

typedef struct _EMailConfigIdentityPage EMailConfigIdentityPage;
typedef struct _EMailConfigIdentityPageClass EMailConfigIdentityPageClass;
typedef struct _EMailConfigIdentityPagePrivate EMailConfigIdentityPagePrivate;

struct _EMailConfigIdentityPage {
	GtkScrolledWindow parent;
	EMailConfigIdentityPagePrivate *priv;
};

struct _EMailConfigIdentityPageClass {
	GtkScrolledWindowClass parent_class;
};

GType		e_mail_config_identity_page_get_type
						(void) G_GNUC_CONST;
EMailConfigPage *
		e_mail_config_identity_page_new	(ESourceRegistry *registry,
						 ESource *identity_source);
ESourceRegistry *
		e_mail_config_identity_page_get_registry
						(EMailConfigIdentityPage *page);
ESource *	e_mail_config_identity_page_get_identity_source
						(EMailConfigIdentityPage *page);
gboolean	e_mail_config_identity_page_get_show_account_info
						(EMailConfigIdentityPage *page);
void		e_mail_config_identity_page_set_show_account_info
						(EMailConfigIdentityPage *page,
						 gboolean show_account_info);
gboolean	e_mail_config_identity_page_get_show_email_address
						(EMailConfigIdentityPage *page);
void		e_mail_config_identity_page_set_show_email_address
						(EMailConfigIdentityPage *page,
						 gboolean show_email_address);
gboolean	e_mail_config_identity_page_get_show_instructions
						(EMailConfigIdentityPage *page);
void		e_mail_config_identity_page_set_show_instructions
						(EMailConfigIdentityPage *page,
						 gboolean show_instructions);
gboolean	e_mail_config_identity_page_get_show_signatures
						(EMailConfigIdentityPage *page);
void		e_mail_config_identity_page_set_show_signatures
						(EMailConfigIdentityPage *page,
						 gboolean show_signatures);
void		e_mail_config_identity_page_set_show_autodiscover_check
						(EMailConfigIdentityPage *page,
						 gboolean show_autodiscover);
gboolean	e_mail_config_identity_page_get_show_autodiscover_check
						(EMailConfigIdentityPage *page);
GtkWidget *	e_mail_config_identity_page_get_autodiscover_check
						(EMailConfigIdentityPage *page);

G_END_DECLS

#endif /* E_MAIL_CONFIG_IDENTITY_PAGE_H */
