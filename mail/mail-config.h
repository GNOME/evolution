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
#include <camel/camel.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

typedef struct {
	gint id;
	gchar *name;
	gchar *filename;
	gchar *script;
	gboolean random;
	gboolean html;
} MailConfigSignature;

typedef struct {
	char *name;
	char *address;
	char *organization;

	MailConfigSignature *text_signature;
	gboolean text_random;
	MailConfigSignature *html_signature;
	gboolean html_random;
} MailConfigIdentity;

typedef struct {
	char *url;
	gboolean keep_on_server;
	gboolean auto_check;
	int auto_check_time;
	gboolean save_passwd;
	gboolean enabled;
} MailConfigService;

typedef struct {
	char *name;
	
	MailConfigIdentity *id;
	MailConfigService *source;
	MailConfigService *transport;
	
	char *drafts_folder_name, *drafts_folder_uri;
	char *sent_folder_name, *sent_folder_uri;
	
	gboolean always_cc;
	char *cc_addrs;
	gboolean always_bcc;
	char *bcc_addrs;
	
	char *pgp_key;
	gboolean pgp_encrypt_to_self;
	gboolean pgp_always_sign;
	
	char *smime_key;
	gboolean smime_encrypt_to_self;
	gboolean smime_always_sign;
} MailConfigAccount;

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

/* signatures */
MailConfigSignature *signature_copy (const MailConfigSignature *sig);
void                 signature_destroy (MailConfigSignature *sig);

/* Identities */
MailConfigIdentity *identity_copy (const MailConfigIdentity *id);
void                identity_destroy (MailConfigIdentity *id);

/* Services */
MailConfigService *service_copy (const MailConfigService *source);
void               service_destroy (MailConfigService *source);
void               service_destroy_each (gpointer item, gpointer data);

/* Accounts */
MailConfigAccount *account_copy (const MailConfigAccount *account);
void               account_destroy (MailConfigAccount *account);
void               account_destroy_each (gpointer item, gpointer data);

/* Configuration */
void mail_config_init (void);
void mail_config_clear (void);
void mail_config_write (void);
void mail_config_write_on_exit (void);

/* General Accessor functions */
gboolean mail_config_is_configured            (void);
gboolean mail_config_is_corrupt               (void);

gboolean    mail_config_get_filter_log        (void);
void        mail_config_set_filter_log        (gboolean value);
const char *mail_config_get_filter_log_path   (void);
void        mail_config_set_filter_log_path   (const char *path);

const char *mail_config_get_last_filesel_dir  (void);
void        mail_config_set_last_filesel_dir  (const char *path);

gboolean mail_config_get_empty_trash_on_exit  (void);
void     mail_config_set_empty_trash_on_exit  (gboolean value);

gboolean mail_config_get_thread_list          (const char *uri);
void     mail_config_set_thread_list          (const char *uri, gboolean value);

gboolean mail_config_get_show_preview         (const char *uri);
void     mail_config_set_show_preview         (const char *uri, gboolean value);

gboolean mail_config_get_hide_deleted         (void);
void     mail_config_set_hide_deleted         (gboolean value);

int      mail_config_get_paned_size           (void);
void     mail_config_set_paned_size           (int size);

gboolean mail_config_get_send_html            (void);
void     mail_config_set_send_html            (gboolean send_html);

gboolean mail_config_get_confirm_unwanted_html (void);
void     mail_config_set_confirm_unwanted_html (gboolean html_warning);

gboolean mail_config_get_citation_highlight   (void);
void     mail_config_set_citation_highlight   (gboolean);

guint32  mail_config_get_citation_color       (void);
void     mail_config_set_citation_color       (guint32);

gint     mail_config_get_do_seen_timeout      (void);
void     mail_config_set_do_seen_timeout      (gboolean do_seen_timeout);

int      mail_config_get_mark_as_seen_timeout (void);
void     mail_config_set_mark_as_seen_timeout (int timeout);

gboolean mail_config_get_prompt_empty_subject (void);
void     mail_config_set_prompt_empty_subject (gboolean value);

gboolean mail_config_get_prompt_only_bcc (void);
void     mail_config_set_prompt_only_bcc (gboolean value);

gboolean mail_config_get_confirm_expunge (void);
void     mail_config_set_confirm_expunge (gboolean value);

gboolean mail_config_get_confirm_goto_next_folder (void);
void     mail_config_set_confirm_goto_next_folder (gboolean value);
gboolean mail_config_get_goto_next_folder (void);
void     mail_config_set_goto_next_folder (gboolean value);

CamelPgpType mail_config_pgp_type_detect_from_path (const char *pgp);

CamelPgpType mail_config_get_pgp_type (void);
void         mail_config_set_pgp_type (CamelPgpType pgp_type);

const char *mail_config_get_pgp_path (void);
void        mail_config_set_pgp_path (const char *pgp_path);

MailConfigHTTPMode mail_config_get_http_mode (void);
void               mail_config_set_http_mode (MailConfigHTTPMode);

MailConfigForwardStyle mail_config_get_default_forward_style (void);
void                   mail_config_set_default_forward_style (MailConfigForwardStyle style);

MailConfigDisplayStyle mail_config_get_message_display_style (void);
void                   mail_config_set_message_display_style (MailConfigDisplayStyle style);

MailConfigNewMailNotify mail_config_get_new_mail_notify (void);
void                    mail_config_set_new_mail_notify (MailConfigNewMailNotify type);
const char             *mail_config_get_new_mail_notify_sound_file (void);
void                    mail_config_set_new_mail_notify_sound_file (const char *filename);

const char *mail_config_get_default_charset (void);
void        mail_config_set_default_charset (const char *charset);

void mail_config_service_set_save_passwd (MailConfigService *service, gboolean save_passwd);

gboolean                  mail_config_find_account              (const MailConfigAccount *account);
const MailConfigAccount  *mail_config_get_default_account       (void);
int                       mail_config_get_default_account_num   (void);
const MailConfigAccount  *mail_config_get_account_by_name       (const char *account_name);
const MailConfigAccount  *mail_config_get_account_by_source_url (const char *url);
const MailConfigAccount  *mail_config_get_account_by_transport_url (const char *url);
const GSList             *mail_config_get_accounts              (void);
void                      mail_config_add_account               (MailConfigAccount *account);
const GSList             *mail_config_remove_account            (MailConfigAccount *account);

void                      mail_config_set_default_account       (const MailConfigAccount *account);

const MailConfigIdentity *mail_config_get_default_identity (void);
const MailConfigService  *mail_config_get_default_transport (void);

const MailConfigService  *mail_config_get_default_news    (void);
const GSList             *mail_config_get_news            (void);
void                      mail_config_add_news            (MailConfigService *news);
const GSList             *mail_config_remove_news         (MailConfigService *news);
GtkType evolution_mail_config_get_type (void);

/* convenience functions to help ease the transition over to the new codebase */
GSList *mail_config_get_sources (void);

/* static utility functions */
char *mail_config_folder_to_cachename (CamelFolder *folder, const char *prefix);
char *mail_config_folder_to_safe_url (CamelFolder *folder);

gboolean mail_config_check_service (const char *url, CamelProviderType type, GList **authtypes, GtkWindow *window);



gboolean evolution_mail_config_factory_init (void);

GList * mail_config_get_signature_list (void);
gint    mail_config_get_signatures_random (void);
MailConfigSignature *mail_config_signature_add (gboolean html);
void mail_config_signature_delete (MailConfigSignature *sig);
void mail_config_signature_write (MailConfigSignature *sig);
void mail_config_signature_set_name (MailConfigSignature *sig, const gchar *name);
void mail_config_signature_set_filename (MailConfigSignature *sig, const gchar *filename);
void mail_config_signature_set_random (MailConfigSignature *sig, gboolean random);

typedef enum {
	MAIL_CONFIG_SIG_EVENT_NAME_CHANGED,
	MAIL_CONFIG_SIG_EVENT_RANDOM_ON,
	MAIL_CONFIG_SIG_EVENT_RANDOM_OFF,
	MAIL_CONFIG_SIG_EVENT_ADDED,
	MAIL_CONFIG_SIG_EVENT_DELETED
} MailConfigSigEvent;

typedef void (*MailConfigSignatureClient)(MailConfigSigEvent, MailConfigSignature *sig, gpointer data);

void mail_config_signature_register_client (MailConfigSignatureClient client, gpointer data);
void mail_config_signature_unregister_client (MailConfigSignatureClient client, gpointer data);
void mail_config_signature_emit_event (MailConfigSigEvent event, MailConfigSignature *sig);

void mail_config_write_account_sig (MailConfigAccount *account, gint i);
void mail_config_signature_run_script (gchar *script);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MAIL_CONFIG_H */
