/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef MAIL_CONFIG_H
#define MAIL_CONFIG_H

#include <gtk/gtk.h>

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>

#include <camel/camel.h>

#include "e-util/e-account.h"
#include "e-util/e-account-list.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

typedef struct {
	int id;
	char *name;
	char *filename;
	char *script;
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
	MAIL_CONFIG_REPLY_ATTACH
} MailConfigReplyStyle;

typedef enum {
	MAIL_CONFIG_DISPLAY_NORMAL,
	MAIL_CONFIG_DISPLAY_FULL_HEADERS,
	MAIL_CONFIG_DISPLAY_SOURCE,
	MAIL_CONFIG_DISPLAY_MAX
} MailConfigDisplayStyle;

typedef enum {
	MAIL_CONFIG_NOTIFY_NOT,
	MAIL_CONFIG_NOTIFY_BEEP,
	MAIL_CONFIG_NOTIFY_PLAY_SOUND,
} MailConfigNewMailNotify;

typedef enum {
	MAIL_CONFIG_XMAILER_NONE            = 0,
	MAIL_CONFIG_XMAILER_EVO             = 1,
	MAIL_CONFIG_XMAILER_OTHER           = 2,
	MAIL_CONFIG_XMAILER_RUPERT_APPROVED = 4
} MailConfigXMailerDisplayStyle;

typedef struct {
	char *tag;
	char *name;
	char *colour;
} MailConfigLabel;

#define LABEL_DEFAULTS_NUM 5
extern MailConfigLabel label_defaults[5];

/* signatures */
MailConfigSignature *signature_copy (const MailConfigSignature *sig);
void                 signature_destroy (MailConfigSignature *sig);

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
const char *mail_config_get_label_color_by_name (const char *name);
const char *mail_config_get_label_color_by_index (int index);

const char **mail_config_get_allowable_mime_types (void);

void mail_config_service_set_save_passwd (EAccountService *service, gboolean save_passwd);

gboolean      mail_config_find_account                 (EAccount *account);
EAccount     *mail_config_get_default_account          (void);
EAccount     *mail_config_get_account_by_name          (const char *account_name);
EAccount     *mail_config_get_account_by_uid           (const char *uid);
EAccount     *mail_config_get_account_by_source_url    (const char *url);
EAccount     *mail_config_get_account_by_transport_url (const char *url);
EAccountList *mail_config_get_accounts                 (void);
void          mail_config_add_account                  (EAccount *account);
void          mail_config_remove_account               (EAccount *account);

void          mail_config_set_default_account          (EAccount *account);

EAccountIdentity *mail_config_get_default_identity (void);
EAccountService  *mail_config_get_default_transport (void);

void mail_config_save_accounts (void);

GSList *mail_config_get_signature_list (void);
MailConfigSignature *mail_config_signature_new (gboolean html, const char *script);
void mail_config_signature_add          (MailConfigSignature *sig);
void mail_config_signature_delete       (MailConfigSignature *sig);
void mail_config_signature_set_name     (MailConfigSignature *sig, const char *name);
void mail_config_signature_set_html     (MailConfigSignature *sig, gboolean html);
void mail_config_signature_set_filename (MailConfigSignature *sig, const char *filename);


/* uri's got changed by the store, etc */
void mail_config_uri_renamed (GCompareFunc uri_cmp, const char *old, const char *new);
void mail_config_uri_deleted (GCompareFunc uri_cmp, const char *uri);


/* static utility functions */
char *mail_config_folder_to_cachename (CamelFolder *folder, const char *prefix);
char *mail_config_folder_to_safe_url (CamelFolder *folder);

gboolean mail_config_check_service (const char *url, CamelProviderType type, GList **authtypes, GtkWindow *window);



GtkType evolution_mail_config_get_type (void);

gboolean evolution_mail_config_factory_init (void);


typedef enum {
	MAIL_CONFIG_SIG_EVENT_NAME_CHANGED,
	MAIL_CONFIG_SIG_EVENT_CONTENT_CHANGED,
	MAIL_CONFIG_SIG_EVENT_HTML_CHANGED,
	MAIL_CONFIG_SIG_EVENT_ADDED,
	MAIL_CONFIG_SIG_EVENT_DELETED
} MailConfigSigEvent;

typedef void (*MailConfigSignatureClient)(MailConfigSigEvent, MailConfigSignature *sig, gpointer data);

void mail_config_signature_register_client (MailConfigSignatureClient client, gpointer data);
void mail_config_signature_unregister_client (MailConfigSignatureClient client, gpointer data);
void mail_config_signature_emit_event (MailConfigSigEvent event, MailConfigSignature *sig);

void mail_config_write_account_sig (EAccount *account, int i);
char *mail_config_signature_run_script (char *script);

typedef enum _mail_config_item_t {
	MAIL_CONFIG_ACCOUNTS,	/* should this be on e-account-list? */
	MAIL_CONFIG_COMPOSER_CHARSET,
	MAIL_CONFIG_COMPOSER_SEND_HTML,
	MAIL_CONFIG_COMPOSER_MAGIC_SMILEYS,
	MAIL_CONFIG_COMPOSER_SPELL_CHECK,
	MAIL_CONFIG_FORMAT_FORWARD_STYLE,
	MAIL_CONFIG_FORMAT_REPLY_STYLE,
	MAIL_CONFIG_TRASH_EMPTY_ON_EXIT,
	MAIL_CONFIG_TRASH_EMPTY_ON_EXIT_DAYS,
	MAIL_CONFIG_DISPLAY_CHARSET,
	MAIL_CONFIG_DISPLAY_HEADERS,
	MAIL_CONFIG_DISPLAY_LABELS,
	MAIL_CONFIG_DISPLAY_FONT_MONO,
	MAIL_CONFIG_DISPLAY_FONT_PROP,
	MAIL_CONFIG_DISPLAY_LOAD_HTTP,

	MAIL_CONFIG_ITEM_LAST
} mail_config_item_t;

gboolean mail_config_writable(mail_config_item_t item);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MAIL_CONFIG_H */
