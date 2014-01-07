/*
 * e-mail-session.h
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
 *
 * Authors:
 *		Jeffrey Stedfast <fejj@ximian.com>
 *		Srinivasa Ragavan <sragavan@gnome.org>
 *		
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_MAIL_UI_SESSION_H
#define E_MAIL_UI_SESSION_H

#include <camel/camel.h>
#include <e-util/e-util.h>
#include <libemail-engine/libemail-engine.h>

#include <mail/e-mail-account-store.h>
#include <mail/e-mail-label-list-store.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_UI_SESSION \
	(e_mail_ui_session_get_type ())
#define E_MAIL_UI_SESSION(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_UI_SESSION, EMailUISession))
#define E_MAIL_UI_SESSION_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_UI_SESSION, EMailUISessionClass))
#define E_IS_MAIL_UI_SESSION(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_UI_SESSION))
#define E_IS_MAIL_UI_SESSION_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_UI_SESSION))
#define E_MAIL_UI_SESSION_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_UI_SESSION, EMailUISessionClass))

G_BEGIN_DECLS

typedef struct _EMailUISession EMailUISession;
typedef struct _EMailUISessionClass EMailUISessionClass;
typedef struct _EMailUISessionPrivate EMailUISessionPrivate;

struct _EMailUISession {
	EMailSession parent;
	EMailUISessionPrivate *priv;
};

struct _EMailUISessionClass {
	EMailSessionClass parent_class;

	void		(*activity_added)	(EMailUISession *session,
						 EActivity *activity);

};

GType		e_mail_ui_session_get_type	(void);
EMailSession *	e_mail_ui_session_new		(ESourceRegistry *registry);
EMailAccountStore *
		e_mail_ui_session_get_account_store
						(EMailUISession *session);
gboolean	e_mail_ui_session_get_check_junk
						(EMailUISession *session);
void		e_mail_ui_session_set_check_junk
						(EMailUISession *session,
						 gboolean check_junk);
EMailLabelListStore *
		e_mail_ui_session_get_label_store
						(EMailUISession *session);
EPhotoCache *	e_mail_ui_session_get_photo_cache
						(EMailUISession *session);
void		e_mail_ui_session_add_activity	(EMailUISession *session,
						 EActivity *activity);
CamelCertTrust	e_mail_ui_session_trust_prompt	(CamelSession *session,
						 CamelService *service,
						 GTlsCertificate *certificate,
						 GTlsCertificateFlags errors);
gboolean	e_mail_ui_session_check_known_address_sync
						(EMailUISession *session,
						 CamelInternetAddress *addr,
						 gboolean check_local_only,
						 GCancellable *cancellable,
						 gboolean *out_known_address,
						 GError **error);

G_END_DECLS

#endif /* E_MAIL_UI_SESSION_H */
