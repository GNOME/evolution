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

#ifndef MAIL_CONFIG_DRUID_H
#define MAIL_CONFIG_DRUID_H

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <gnome.h>
#include <camel.h>

#define MAIL_CONFIG_DRUID_TYPE        (mail_config_druid_get_type ())
#define MAIL_CONFIG_DRUID(o)          (GTK_CHECK_CAST ((o), MAIL_CONFIG_DRUID_TYPE, MailConfigDruid))
#define MAIL_CONFIG_DRUID_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), MAIL_CONFIG_DRUID_TYPE, MailConfigDruidClass))
#define IS_MAIL_CONFIG_DRUID(o)       (GTK_CHECK_TYPE ((o), MAIL_CONFIG_DRUID_TYPE))
#define IS_MAIL_CONFIG_DRUID_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), MAIL_CONFIG_DRUID_TYPE))

struct _MailConfigDruid {
	GtkWindow parent;
	
	GladeXML *gui;
	
	GList *providers;
	
	GnomeDruid *druid;
	
	/* account management */
	GtkWidget *account_text;
	GtkEntry *account_name;
	GtkCheckBox *default_account;
	
	/* identity */
	GtkWidget *identity_text;
	GtkEntry *full_name;
	GtkEntry *email_address;
	GtkEntry *reply_to;
	GtkEntry *organization;
	GnomeFileEntry *signature;
	
	/* incoming mail */
	GtkWidget *incoming_text;
	GtkOptionMenu *incoming_type;
	GtkEntry *incoming_hostname;
	GtkEntry *incoming_username;
	GtkEntry *incoming_path;
	GtkCheckBox *incoming_delete_mail;
	
	/* authentication */
	GtkWidget *auth_text;
	GtkOptionMenu *auth_type;
	GtkEntry *password;
	GtkCheckBox *save_password;
	
	/* outgoing mail */
	GtkWidget *outgoing_text;
	GtkOptionMenu *outgoing_type;
	GtkEntry *outgoing_hostname;
	GtkCheckBox *outgoing_requires_auth;
	
	const CamelProvider *source_provider;
	const CamelProvider *transport_provider;
	
};

typedef struct _MailConfigDialog MailConfigDialog;

typedef struct {
	GtkWindowClass parent_class;
	
	/* signals */
	
} MailConfigDruidClass;

GtkType mail_config_druid_get_type (void);

MailConfigDruid *mail_config_druid_new (void);

char *mail_config_druid_get_account_name (MailConfigDruid *druid);

gboolean mail_config_druid_get_default_account (MailConfigDruid *druid);

char *mail_config_druid_get_full_name (MailConfigDruid *druid);

char *mail_config_druid_get_email_address (MailConfigDruid *druid);

char *mail_config_druid_get_reply_to (MailConfigDruid *druid);

char *mail_config_druid_get_organization (MailConfigDruid *druid);

char *mail_config_druid_get_sigfile (MailConfigDruid *druid);

int mail_config_druid_get_incoming_type (MailConfigDruid *druid);

char *mail_config_druid_get_incoming_hostname (MailConfigDruid *druid);

char *mail_config_druid_get_incoming_username (MailConfigDruid *druid);

char *mail_config_druid_get_incoming_path (MailConfigDruid *druid);

gboolean mail_config_druid_get_incoming_delete_mail (MailConfigDruid *druid);

int mail_config_druid_get_auth_type (MailConfigDruid *druid);

char *mail_config_druid_get_passwd (MailConfigDruid *druid);

gboolean mail_config_druid_get_save_passwd (MailConfigDruid *druid);

int mail_config_druid_get_outgoing_type (MailConfigDruid *druid);

char *mail_config_druid_get_outgoing_hostname (MailConfigDruid *druid);

gboolean mail_config_druid_get_outgoing_requires_auth (MailConfigDruid *druid);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MAIL_CONFIG_DRUID_H */
