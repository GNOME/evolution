/*
 * e-mail-config-auth-check.h
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

#ifndef E_MAIL_CONFIG_AUTH_CHECK_H
#define E_MAIL_CONFIG_AUTH_CHECK_H

#include <mail/e-mail-config-service-backend.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_CONFIG_AUTH_CHECK \
	(e_mail_config_auth_check_get_type ())
#define E_MAIL_CONFIG_AUTH_CHECK(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_CONFIG_AUTH_CHECK, EMailConfigAuthCheck))
#define E_MAIL_CONFIG_AUTH_CHECK_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_CONFIG_AUTH_CHECK, EMailConfigAuthCheckClass))
#define E_IS_MAIL_CONFIG_AUTH_CHECK(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_CONFIG_AUTH_CHECK))
#define E_IS_MAIL_CONFIG_AUTH_CHECK_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_CONFIG_AUTH_CHECK))
#define E_MAIL_CONFIG_AUTH_CHECK_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_CONFIG_AUTH_CHECK, EMailConfigAuthCheckClass))

G_BEGIN_DECLS

typedef struct _EMailConfigAuthCheck EMailConfigAuthCheck;
typedef struct _EMailConfigAuthCheckClass EMailConfigAuthCheckClass;
typedef struct _EMailConfigAuthCheckPrivate EMailConfigAuthCheckPrivate;

struct _EMailConfigAuthCheck {
	GtkBox parent;
	EMailConfigAuthCheckPrivate *priv;
};

struct _EMailConfigAuthCheckClass {
	GtkBoxClass parent_class;
};

GType		e_mail_config_auth_check_get_type
					(void) G_GNUC_CONST;
GtkWidget *	e_mail_config_auth_check_new
					(EMailConfigServiceBackend *backend);
EMailConfigServiceBackend *
		e_mail_config_auth_check_get_backend
					(EMailConfigAuthCheck *auth_check);
const gchar *	e_mail_config_auth_check_get_active_mechanism
					(EMailConfigAuthCheck *auth_check);
void		e_mail_config_auth_check_set_active_mechanism
					(EMailConfigAuthCheck *auth_check,
					 const gchar *active_mechanism);

G_END_DECLS

#endif /* E_MAIL_CONFIG_AUTH_CHECK_H */
