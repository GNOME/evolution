/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@helixcode.com>
 *
 *  Copyright 2001 Helix Code, Inc. (www.helixcode.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef MAIL_CONFIG_H
#define MAIL_CONFIG_H

#include <glib.h>
#include <camel/camel.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

typedef struct {
	gchar *name;
	gchar *address;
	gchar *organization;
	gchar *signature;
} MailConfigIdentity;

typedef struct {
	gchar *url;
	gboolean keep_on_server;
	gboolean auto_check;
	gint auto_check_time;
	gboolean save_passwd;
	gboolean enabled;
} MailConfigService;

typedef struct {
	gchar *name;
	gboolean default_account;
	
	MailConfigIdentity *id;
	MailConfigService *source;
	MailConfigService *transport;
	
	gchar *drafts_folder_name, *drafts_folder_uri;
	gchar *sent_folder_name, *sent_folder_uri;
} MailConfigAccount;

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

gboolean mail_config_get_thread_list          (void);
void     mail_config_set_thread_list          (gboolean value);

gboolean mail_config_get_view_source          (void);
void     mail_config_set_view_source          (gboolean value);

gboolean mail_config_get_hide_deleted          (void);
void     mail_config_set_hide_deleted          (gboolean value);

gint     mail_config_get_paned_size           (void);
void     mail_config_set_paned_size           (gint size);

gboolean mail_config_get_send_html            (void);
void     mail_config_set_send_html            (gboolean send_html);

gboolean mail_config_get_citation_highlight   (void);
void     mail_config_set_citation_highlight   (gboolean);

guint32  mail_config_get_citation_color       (void);
void     mail_config_set_citation_color       (guint32);

gint     mail_config_get_mark_as_seen_timeout (void);
void     mail_config_set_mark_as_seen_timeout (gint timeout);

gboolean mail_config_get_prompt_empty_subject (void);
void     mail_config_set_prompt_empty_subject (gboolean value);

gint     mail_config_get_pgp_type (void);
void     mail_config_set_pgp_type (gint pgp_type);

const char *mail_config_get_pgp_path (void);
void        mail_config_set_pgp_path (const char *pgp_path);

const MailConfigAccount  *mail_config_get_default_account       (void);
const MailConfigAccount  *mail_config_get_account_by_name       (const char *account_name);
const MailConfigAccount  *mail_config_get_account_by_source_url (const char *url);
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

/* convenience functions to help ease the transition over to the new codebase */
GSList *mail_config_get_sources (void);

/* static utility functions */
char *mail_config_folder_to_cachename (CamelFolder *folder, const char *prefix);

gboolean  mail_config_check_service (const char *url, CamelProviderType type, GList **authtypes);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MAIL_CONFIG_H */
