/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors:
 *    Jeffrey Stedfast <fejj@ximian.com>
 *    Dan Winship <danw@ximian.com>
 *
 *  Copyright 2001-2003 Ximian, Inc. (www.ximian.com)
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

#ifndef MAIL_ACCOUNT_GUI_H
#define MAIL_ACCOUNT_GUI_H

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <gtk/gtk.h>
#include <libgnomeui/gnome-file-entry.h>
#include <glade/glade-xml.h>
#include <camel/camel-provider.h>

#include "mail-config.h"
#include "em-account-prefs.h"

typedef struct {
	GtkWidget *container;

	GtkOptionMenu *type;
	GtkLabel *description;
	GtkEntry *hostname;
	GtkEntry *username;
	GtkEntry *path;
	GtkOptionMenu *use_ssl;
	GtkWidget *ssl_selected;
	GtkWidget *ssl_hbox;
	GtkWidget *no_ssl;
	GtkOptionMenu *authtype;
	GtkWidget *authitem;
	GtkToggleButton *remember;
	GtkButton *check_supported;
	
	CamelProvider *provider;
	CamelProviderType provider_type;
} MailAccountGuiService;

typedef struct {
	EAccount *account;
	EMAccountPrefs *dialog;
	GladeXML *xml;
	
	/* identity */
	GtkEntry *full_name;
	GtkEntry *email_address;
	GtkEntry *reply_to;
	GtkEntry *organization;
	
	/* signatures */
	GtkWidget *sig_option_menu;
	
	MailConfigSignature *def_signature;
	gboolean auto_signature;
	
	/* incoming mail */
	MailAccountGuiService source;
	GtkToggleButton *source_auto_check;
	GtkSpinButton *source_auto_check_min;
	
	/* extra incoming config */
	GHashTable *extra_config;
	
	/* outgoing mail */
	MailAccountGuiService transport;
	GtkToggleButton *transport_needs_auth;
	
	/* account management */
	GtkEntry *account_name;
	GtkToggleButton *default_account;
	
	/* special folders */
	GtkButton *drafts_folder_button;
	char *drafts_folder_uri;
	GtkButton *sent_folder_button;
	char *sent_folder_uri;
	GtkButton *restore_folders_button;

	/* always cc/bcc */
	GtkToggleButton *always_cc;
	GtkEntry *cc_addrs;
	GtkToggleButton *always_bcc;
	GtkEntry *bcc_addrs;
	
	/* Security */
	GtkEntry *pgp_key;
	GtkToggleButton *pgp_encrypt_to_self;
	GtkToggleButton *pgp_always_sign;
	GtkToggleButton *pgp_no_imip_sign;
	GtkToggleButton *pgp_always_trust;

	GtkToggleButton *smime_sign_default;
	GtkEntry *smime_sign_key;
	GtkButton *smime_sign_key_select;
	GtkButton *smime_sign_key_clear;
	GtkButton *smime_sign_select;
	GtkToggleButton *smime_encrypt_default;
	GtkToggleButton *smime_encrypt_to_self;
	GtkEntry *smime_encrypt_key;
	GtkButton *smime_encrypt_key_select;
	GtkButton *smime_encrypt_key_clear;
} MailAccountGui;


MailAccountGui *mail_account_gui_new (EAccount *account, EMAccountPrefs *dialog);
void mail_account_gui_setup (MailAccountGui *gui, GtkWidget *top);
gboolean mail_account_gui_save (MailAccountGui *gui);
void mail_account_gui_destroy (MailAccountGui *gui);

gboolean mail_account_gui_identity_complete (MailAccountGui *gui, GtkWidget **incomplete);
gboolean mail_account_gui_source_complete (MailAccountGui *gui, GtkWidget **incomplete);
gboolean mail_account_gui_transport_complete (MailAccountGui *gui, GtkWidget **incomplete);
gboolean mail_account_gui_management_complete (MailAccountGui *gui, GtkWidget **incomplete);

void mail_account_gui_build_extra_conf (MailAccountGui *gui, const char *url);

void mail_account_gui_auto_detect_extra_conf (MailAccountGui *gui);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MAIL_ACCOUNT_GUI_H */
