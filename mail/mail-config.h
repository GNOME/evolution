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

#include <glib.h>
#include <glib-object.h>

#include "camel/camel-provider.h" /* can't forward-declare enums, bah */

struct _EAccount;
struct _EAccountList;
struct _EAccountService;

struct _ESignature;
struct _ESignatureList;

struct _GConfClient;
struct _GtkWindow;

struct _CamelFolder;

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

typedef struct _MailConfigSignature {
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

/* Configuration */
void mail_config_init (void);
void mail_config_clear (void);
void mail_config_write (void);
void mail_config_write_on_exit (void);

struct _GConfClient *mail_config_get_gconf_client (void);

/* General Accessor functions */
gboolean mail_config_is_configured            (void);
gboolean mail_config_is_corrupt               (void);

GSList *mail_config_get_labels (void);
const char *mail_config_get_label_color_by_name (const char *name);
const char *mail_config_get_label_color_by_index (int index);

const char **mail_config_get_allowable_mime_types (void);

void mail_config_service_set_save_passwd (struct _EAccountService *service, gboolean save_passwd);

/* accounts */
gboolean mail_config_find_account (struct _EAccount *account);
struct _EAccount *mail_config_get_default_account (void);
struct _EAccount *mail_config_get_account_by_name (const char *account_name);
struct _EAccount *mail_config_get_account_by_uid (const char *uid);
struct _EAccount *mail_config_get_account_by_source_url (const char *url);
struct _EAccount *mail_config_get_account_by_transport_url (const char *url);

struct _EAccountList *mail_config_get_accounts (void);
void mail_config_add_account (struct _EAccount *account);
void mail_config_remove_account (struct _EAccount *account);
void mail_config_set_default_account (struct _EAccount *account);

struct _EAccountIdentity *mail_config_get_default_identity (void);
struct _EAccountService  *mail_config_get_default_transport (void);

void mail_config_save_accounts (void);

/* signatures */
struct _ESignature *mail_config_signature_new (const char *filename, gboolean script, gboolean html);
struct _ESignature *mail_config_get_signature_by_uid (const char *uid);
struct _ESignature *mail_config_get_signature_by_name (const char *name);

struct _ESignatureList *mail_config_get_signatures (void);
void mail_config_add_signature (struct _ESignature *signature);
void mail_config_remove_signature (struct _ESignature *signature);

void mail_config_save_signatures (void);

char *mail_config_signature_run_script (const char *script);


/* uri's got changed by the store, etc */
void mail_config_uri_renamed (GCompareFunc uri_cmp, const char *old, const char *new);
void mail_config_uri_deleted (GCompareFunc uri_cmp, const char *uri);

/* static utility functions */
char *mail_config_folder_to_cachename (struct _CamelFolder *folder, const char *prefix);
char *mail_config_folder_to_safe_url (struct _CamelFolder *folder);

GType evolution_mail_config_get_type (void);

gboolean evolution_mail_config_factory_init (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MAIL_CONFIG_H */
