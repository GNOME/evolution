/*
 * e-mail-send-account-override.h
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
	GObjectClass parent_class;

	/* Signals */
	void		(*changed)	(EMailSendAccountOverride *override);
};

GType		e_mail_send_account_override_get_type
					(void) G_GNUC_CONST;
EMailSendAccountOverride *
		e_mail_send_account_override_new
					(const gchar *config_filename);
void		e_mail_send_account_override_set_config_filename
					(EMailSendAccountOverride *override,
					 const gchar *config_filename);
gchar *		e_mail_send_account_override_dup_config_filename
					(EMailSendAccountOverride *override);
void		e_mail_send_account_override_set_prefer_folder
					(EMailSendAccountOverride *override,
					 gboolean prefer_folder);
gboolean	e_mail_send_account_override_get_prefer_folder
					(EMailSendAccountOverride *override);
gchar *		e_mail_send_account_override_get_account_uid
					(EMailSendAccountOverride *override,
					 const gchar *folder_uri,
					 CamelInternetAddress *recipients_to,
					 CamelInternetAddress *recipients_cc,
					 CamelInternetAddress *recipients_bcc,
					 gchar **out_alias_name,
					 gchar **out_alias_address);
void		e_mail_send_account_override_remove_for_account_uid
					(EMailSendAccountOverride *override,
					 const gchar *account_uid,
					 const gchar *alias_name,
					 const gchar *alias_address);
gchar *		e_mail_send_account_override_get_for_folder
					(EMailSendAccountOverride *override,
					 const gchar *folder_uri,
					 gchar **out_alias_name,
					 gchar **out_alias_address);
void		e_mail_send_account_override_set_for_folder
					(EMailSendAccountOverride *override,
					 const gchar *folder_uri,
					 const gchar *account_uid,
					 const gchar *alias_name,
					 const gchar *alias_address);
void		e_mail_send_account_override_remove_for_folder
					(EMailSendAccountOverride *override,
					 const gchar *folder_uri);
gchar *		e_mail_send_account_override_get_for_recipient
					(EMailSendAccountOverride *override,
					 CamelInternetAddress *recipients,
					 gchar **out_alias_name,
					 gchar **out_alias_address);
void		e_mail_send_account_override_set_for_recipient
					(EMailSendAccountOverride *override,
					 const gchar *recipient,
					 const gchar *account_uid,
					 const gchar *alias_name,
					 const gchar *alias_address);
void		e_mail_send_account_override_remove_for_recipient
					(EMailSendAccountOverride *override,
					 const gchar *recipient);
void		e_mail_send_account_override_list_for_account
					(EMailSendAccountOverride *override,
					 const gchar *account_uid,
					 const gchar *alias_name,
					 const gchar *alias_address,
					 GList **folder_overrides,
					 GList **recipient_overrides);
void		e_mail_send_account_override_freeze_save
					(EMailSendAccountOverride *override);
void		e_mail_send_account_override_thaw_save
					(EMailSendAccountOverride *override);

G_END_DECLS

#endif /* E_MAIL_SEND_ACCOUNT_OVERRIDE_H */
