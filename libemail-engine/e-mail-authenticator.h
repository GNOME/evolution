/*
 * e-mail-authenticator.h
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

#if !defined (__LIBEMAIL_ENGINE_H_INSIDE__) && !defined (LIBEMAIL_ENGINE_COMPILATION)
#error "Only <libemail-engine/libemail-engine.h> should be included directly."
#endif

#ifndef E_MAIL_AUTHENTICATOR_H
#define E_MAIL_AUTHENTICATOR_H

#include <camel/camel.h>
#include <libedataserver/libedataserver.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_AUTHENTICATOR \
	(e_mail_authenticator_get_type ())
#define E_MAIL_AUTHENTICATOR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_AUTHENTICATOR, EMailAuthenticator))
#define E_MAIL_AUTHENTICATOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_AUTHENTICATOR, EMailAuthenticatorClass))
#define E_IS_MAIL_AUTHENTICATOR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_AUTHENTICATOR))
#define E_IS_MAIL_AUTHENTICATOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_AUTHENTICATOR))
#define E_MAIL_AUTHENTICATOR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_AUTHENTICATOR, EMailAuthenticatorClass))

G_BEGIN_DECLS

typedef struct _EMailAuthenticator EMailAuthenticator;
typedef struct _EMailAuthenticatorClass EMailAuthenticatorClass;
typedef struct _EMailAuthenticatorPrivate EMailAuthenticatorPrivate;

/**
 * EMailAuthenticator:
 *
 * Contains only private data that should be read and manipulated using the
 * functions below.
 **/
struct _EMailAuthenticator {
	GObject parent;
	EMailAuthenticatorPrivate *priv;
};

struct _EMailAuthenticatorClass {
	GObjectClass parent_class;
};

GType		e_mail_authenticator_get_type	(void);
ESourceAuthenticator *
		e_mail_authenticator_new	(CamelService *service,
						 const gchar *mechanism);
CamelService *	e_mail_authenticator_get_service
						(EMailAuthenticator *auth);
const gchar *	e_mail_authenticator_get_mechanism
						(EMailAuthenticator *auth);

G_END_DECLS

#endif /* E_MAIL_AUTHENTICATOR_H */
