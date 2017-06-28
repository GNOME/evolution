/*
 * e-mail-config-welcome-page.h
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

#ifndef E_MAIL_CONFIG_WELCOME_PAGE_H
#define E_MAIL_CONFIG_WELCOME_PAGE_H

#include <gtk/gtk.h>

#include <mail/e-mail-config-page.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_CONFIG_WELCOME_PAGE \
	(e_mail_config_welcome_page_get_type ())
#define E_MAIL_CONFIG_WELCOME_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_CONFIG_WELCOME_PAGE, EMailConfigWelcomePage))
#define E_MAIL_CONFIG_WELCOME_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_CONFIG_WELCOME_PAGE, EMailConfigWelcomePageClass))
#define E_IS_MAIL_CONFIG_WELCOME_PAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_CONFIG_WELCOME_PAGE))
#define E_IS_MAIL_CONFIG_WELCOME_PAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_CONFIG_WELCOME_PAGE))
#define E_MAIL_CONFIG_WELCOME_PAGE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_CONFIG_WELCOME_PAGE, EMailConfigWelcomePageClass))

#define E_MAIL_CONFIG_WELCOME_PAGE_SORT_ORDER (0)

G_BEGIN_DECLS

typedef struct _EMailConfigWelcomePage EMailConfigWelcomePage;
typedef struct _EMailConfigWelcomePageClass EMailConfigWelcomePageClass;
typedef struct _EMailConfigWelcomePagePrivate EMailConfigWelcomePagePrivate;

struct _EMailConfigWelcomePage {
	GtkScrolledWindow parent;
	EMailConfigWelcomePagePrivate *priv;
};

struct _EMailConfigWelcomePageClass {
	GtkScrolledWindowClass parent_class;
};

GType		e_mail_config_welcome_page_get_type
						(void) G_GNUC_CONST;
EMailConfigPage *
		e_mail_config_welcome_page_new	(void);
const gchar *	e_mail_config_welcome_page_get_text
						(EMailConfigWelcomePage *page);
void		e_mail_config_welcome_page_set_text
						(EMailConfigWelcomePage *page,
						 const gchar *text);
GtkBox *	e_mail_config_welcome_page_get_main_box
						(EMailConfigWelcomePage *page);

G_END_DECLS

#endif /* E_MAIL_CONFIG_WELCOME_PAGE_H */

