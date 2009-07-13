/*
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
 * Authors:
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef MAIL_CONFIG_H
#define MAIL_CONFIG_H

#include <gtk/gtk.h>
#include <camel/camel-provider.h>
#include <libedataserver/e-account.h>
#include <libedataserver/e-account-list.h>

#include <e-util/e-signature.h>
#include <e-util/e-signature-list.h>

G_BEGIN_DECLS

typedef struct _MailConfigSignature {
	gint id;
	gchar *name;
	gchar *filename;
	gchar *script;
	gboolean html;
} MailConfigSignature;

typedef enum {
	MAIL_CONFIG_HTTP_NEVER,
	MAIL_CONFIG_HTTP_SOMETIMES,
	MAIL_CONFIG_HTTP_ALWAYS
} MailConfigHTTPMode;

typedef enum {
	MAIL_CONFIG_FORWARD_ATTACHED,
	MAIL_CONFIG_FORWARD_INLINE,
	MAIL_CONFIG_FORWARD_QUOTED
} MailConfigForwardStyle;

typedef enum {
	MAIL_CONFIG_REPLY_QUOTED,
	MAIL_CONFIG_REPLY_DO_NOT_QUOTE,
	MAIL_CONFIG_REPLY_ATTACH,
	MAIL_CONFIG_REPLY_OUTLOOK
} MailConfigReplyStyle;

typedef enum {
	MAIL_CONFIG_DISPLAY_NORMAL,
	MAIL_CONFIG_DISPLAY_FULL_HEADERS,
	MAIL_CONFIG_DISPLAY_SOURCE,
	MAIL_CONFIG_DISPLAY_MAX
} MailConfigDisplayStyle;

typedef enum {
	MAIL_CONFIG_XMAILER_NONE            = 0,
	MAIL_CONFIG_XMAILER_EVO             = 1,
	MAIL_CONFIG_XMAILER_OTHER           = 2,
	MAIL_CONFIG_XMAILER_RUPERT_APPROVED = 4
} MailConfigXMailerDisplayStyle;

/* Configuration */
void mail_config_init (void);
void mail_config_clear (void);
void mail_config_write (void);
void mail_config_write_on_exit (void);

GConfClient *mail_config_get_gconf_client (void);

/* General Accessor functions */
gboolean mail_config_is_configured            (void);
gboolean mail_config_is_corrupt               (void);

GSList *mail_config_get_labels (void);

const gchar **mail_config_get_allowable_mime_types (void);

void mail_config_service_set_save_passwd (EAccountService *service, gboolean save_passwd);

/* accounts */
gboolean mail_config_find_account (EAccount *account);
EAccount *mail_config_get_default_account (void);
EAccount *mail_config_get_account_by_name (const gchar *account_name);
EAccount *mail_config_get_account_by_uid (const gchar *uid);
EAccount *mail_config_get_account_by_source_url (const gchar *url);
EAccount *mail_config_get_account_by_transport_url (const gchar *url);

EAccountList *mail_config_get_accounts (void);
void mail_config_add_account (EAccount *account);
void mail_config_remove_account (EAccount *account);
void mail_config_set_default_account (EAccount *account);
gint mail_config_get_address_count (void);
gint mail_config_get_message_limit (void);
gboolean mail_config_get_enable_magic_spacebar (void);

void mail_config_remove_account_proxies (EAccount *account);
void mail_config_prune_proxies (void);
gint mail_config_has_proxies (EAccount *account);

EAccountIdentity *mail_config_get_default_identity (void);
EAccountService  *mail_config_get_default_transport (void);

void mail_config_save_accounts (void);

/* signatures */
ESignature *mail_config_signature_new (const gchar *filename, gboolean script, gboolean html);
ESignature *mail_config_get_signature_by_uid (const gchar *uid);
ESignature *mail_config_get_signature_by_name (const gchar *name);

ESignatureList *mail_config_get_signatures (void);
void mail_config_add_signature (ESignature *signature);
void mail_config_remove_signature (ESignature *signature);

void mail_config_save_signatures (void);

gchar *mail_config_signature_run_script (const gchar *script);

/* uri's got changed by the store, etc */
void mail_config_uri_renamed (GCompareFunc uri_cmp, const gchar *old, const gchar *new);
void mail_config_uri_deleted (GCompareFunc uri_cmp, const gchar *uri);

/* static utility functions */
gchar *mail_config_folder_to_cachename (CamelFolder *folder, const gchar *prefix);
gchar *mail_config_folder_to_safe_url (CamelFolder *folder);
guint mail_config_get_error_timeout  (void);
guint mail_config_get_error_level  (void);

gint mail_config_get_sync_timeout (void);

void mail_config_reload_junk_headers (void);
gboolean mail_config_get_lookup_book (void);
gboolean mail_config_get_lookup_book_local_only (void);

gboolean mail_config_scripts_disabled (void);

GType evolution_mail_config_get_type (void);

gboolean evolution_mail_config_factory_init (void);

G_END_DECLS

#endif /* MAIL_CONFIG_H */
