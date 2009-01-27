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

#include <glib.h>
#include <glib-object.h>

#include <camel/camel-provider.h>

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

struct _GConfClient *mail_config_get_gconf_client (void);

/* General Accessor functions */
GSList *mail_config_get_labels (void);

const char **mail_config_get_allowable_mime_types (void);

void mail_config_service_set_save_passwd (struct _EAccountService *service, gboolean save_passwd);

/* accounts */
struct _EAccount *mail_config_get_account_by_source_url (const char *url);
struct _EAccount *mail_config_get_account_by_transport_url (const char *url);

int mail_config_get_address_count (void);
int mail_config_get_message_limit (void);
gboolean mail_config_get_enable_magic_spacebar (void);

struct _EAccountService  *mail_config_get_default_transport (void);

/* signatures */
struct _ESignature *mail_config_signature_new (const char *filename, gboolean script, gboolean html);

char *mail_config_signature_run_script (const char *script);


/* uri's got changed by the store, etc */
void mail_config_uri_renamed (GCompareFunc uri_cmp, const char *old, const char *new);
void mail_config_uri_deleted (GCompareFunc uri_cmp, const char *uri);

/* static utility functions */
char *mail_config_folder_to_cachename (struct _CamelFolder *folder, const char *prefix);
char *mail_config_folder_to_safe_url (struct _CamelFolder *folder);
guint mail_config_get_error_timeout  (void);
guint mail_config_get_error_level  (void);

gint mail_config_get_sync_timeout (void);

void mail_config_reload_junk_headers (void);
gboolean mail_config_get_lookup_book (void);
gboolean mail_config_get_lookup_book_local_only (void);

GType evolution_mail_config_get_type (void);

gboolean evolution_mail_config_factory_init (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MAIL_CONFIG_H */
