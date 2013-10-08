/*
 * e-mail-send-account-override.h
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Copyright (C) 2013 Red Hat, Inc. (www.redhat.com)
 *
 */

#ifndef E_MAIL_SEND_ACCOUNT_OVERRIDE_H
#define E_MAIL_SEND_ACCOUNT_OVERRIDE_H

#include <glib-object.h>
#include <camel/camel.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_SEND_ACCOUNT_OVERRIDE \
	(e_mail_send_account_override_get_type ())
#define E_MAIL_SEND_ACCOUNT_OVERRIDE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_SEND_ACCOUNT_OVERRIDE, EMailSendAccountOverride))
#define E_MAIL_SEND_ACCOUNT_OVERRIDE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_SEND_ACCOUNT_OVERRIDE, EMailSendAccountOverrideClass))
#define E_IS_MAIL_SEND_ACCOUNT_OVERRIDE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_SEND_ACCOUNT_OVERRIDE))
#define E_IS_MAIL_SEND_ACCOUNT_OVERRIDE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_SEND_ACCOUNT_OVERRIDE))
#define E_MAIL_SEND_ACCOUNT_OVERRIDE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_SEND_ACCOUNT_OVERRIDE, EMailSendAccountOverrideClass))

G_BEGIN_DECLS

typedef struct _EMailSendAccountOverride EMailSendAccountOverride;
typedef struct _EMailSendAccountOverrideClass EMailSendAccountOverrideClass;
typedef struct _EMailSendAccountOverridePrivate EMailSendAccountOverridePrivate;

struct _EMailSendAccountOverride {
	GObject parent;
	EMailSendAccountOverridePrivate *priv;
};

struct _EMailSendAccountOverrideClass {
	GObjectClass parent;

	/* Signals */
	void	(* changed)	(EMailSendAccountOverride *account_override);
};

GType		e_mail_send_account_override_get_type		(void);
EMailSendAccountOverride *
		e_mail_send_account_override_new		(const gchar *config_filename);
void		e_mail_send_account_override_set_config_filename
								(EMailSendAccountOverride *account_override,
								 const gchar *config_filename);
gchar *		e_mail_send_account_override_dup_config_filename
								(EMailSendAccountOverride *account_override);
void		e_mail_send_account_override_set_prefer_folder	(EMailSendAccountOverride *account_override,
								 gboolean prefer_folder);
gboolean	e_mail_send_account_override_get_prefer_folder	(EMailSendAccountOverride *account_override);
gchar *		e_mail_send_account_override_get_account_uid	(EMailSendAccountOverride *account_override,
								 const gchar *folder_uri,
								 const CamelInternetAddress *recipients_to,
								 const CamelInternetAddress *recipients_cc,
								 const CamelInternetAddress *recipients_bcc);
void		e_mail_send_account_override_remove_for_account_uid
								(EMailSendAccountOverride *account_override,
								 const gchar *account_uid);
gchar *		e_mail_send_account_override_get_for_folder	(EMailSendAccountOverride *account_override,
								 const gchar *folder_uri);
void		e_mail_send_account_override_set_for_folder	(EMailSendAccountOverride *account_override,
								 const gchar *folder_uri,
								 const gchar *account_uid);
void		e_mail_send_account_override_remove_for_folder	(EMailSendAccountOverride *account_override,
								 const gchar *folder_uri);
gchar *		e_mail_send_account_override_get_for_recipient	(EMailSendAccountOverride *account_override,
								 const CamelInternetAddress *recipients);
void		e_mail_send_account_override_set_for_recipient	(EMailSendAccountOverride *account_override,
								 const gchar *recipient,
								 const gchar *account_uid);
void		e_mail_send_account_override_remove_for_recipient
								(EMailSendAccountOverride *account_override,
								 const gchar *recipient);
void		e_mail_send_account_override_list_for_account	(EMailSendAccountOverride *account_override,
								 const gchar *account_uid,
								 GSList **folder_overrides,
								 GSList **recipient_overrides);
void		e_mail_send_account_override_freeze_save	(EMailSendAccountOverride *account_override);
void		e_mail_send_account_override_thaw_save		(EMailSendAccountOverride *account_override);

G_END_DECLS

#endif /* E_MAIL_SEND_ACCOUNT_OVERRIDE_H */
