/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors:
 *    Jeffrey Stedfast <fejj@ximian.com>
 *    Dan Winship <danw@ximian.com>
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
#include "mail-accounts.h"

typedef struct {
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
	char *name, *uri;
} MailAccountGuiFolder;

typedef struct {
	GtkWidget *top;
	MailConfigAccount *account;
	MailAccountsTab *dialog;
	GladeXML *xml;
	
	/* identity */
	GtkEntry *full_name;
	GtkEntry *email_address;
	GtkEntry *organization;

	/* signatures */
	GtkWidget *sig_option_text;
	GtkWidget *sig_option_html;
	GtkWidget *sig_new_text;
	GtkWidget *sig_new_html;
	GtkWidget *sig_edit_text;
	GtkWidget *sig_edit_html;
	MailConfigSignature *text_signature;
	gboolean text_random;
	MailConfigSignature *html_signature;
	gboolean html_random;

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
	MailAccountGuiFolder drafts_folder;
	GtkButton *sent_folder_button;
	MailAccountGuiFolder sent_folder;
	
	/* always cc/bcc */
	GtkToggleButton *always_cc;
	GtkEntry *cc_addrs;
	GtkToggleButton *always_bcc;
	GtkEntry *bcc_addrs;
	
	/* Security */
	GtkEntry *pgp_key;
	GtkToggleButton *pgp_encrypt_to_self;
	GtkToggleButton *pgp_always_sign;
	GtkEntry *smime_key;
	GtkToggleButton *smime_encrypt_to_self;
	GtkToggleButton *smime_always_sign;
} MailAccountGui;


MailAccountGui *mail_account_gui_new (MailConfigAccount *account, MailAccountsTab *dialog);
void mail_account_gui_setup (MailAccountGui *gui, GtkWidget *top);
gboolean mail_account_gui_save (MailAccountGui *gui);
void mail_account_gui_destroy (MailAccountGui *gui);

gboolean mail_account_gui_identity_complete (MailAccountGui *gui, GtkWidget **incomplete);
gboolean mail_account_gui_source_complete (MailAccountGui *gui, GtkWidget **incomplete);
gboolean mail_account_gui_transport_complete (MailAccountGui *gui, GtkWidget **incomplete);
gboolean mail_account_gui_management_complete (MailAccountGui *gui, GtkWidget **incomplete);

void mail_account_gui_build_extra_conf (MailAccountGui *gui, const char *url);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MAIL_ACCOUNT_GUI_H */
