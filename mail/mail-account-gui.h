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

#include <camel/camel-provider.h>

struct _EAccount;
struct _EMAccountPrefs;

typedef struct _MailAccountGuiService {
	struct _GtkWidget *container;

	struct _GtkOptionMenu *type;
	struct _GtkLabel *description;
	struct _GtkEntry *hostname;
	struct _GtkEntry *username;
	struct _GtkEntry *path;
	struct _GtkWidget *ssl_frame;
	struct _GtkOptionMenu *use_ssl;
	struct _GtkWidget *ssl_selected;
	struct _GtkWidget *ssl_hbox;
	struct _GtkWidget *no_ssl;
	struct _GtkOptionMenu *authtype;
	struct _GtkWidget *authitem;
	struct _GtkToggleButton *remember;
	struct _GtkButton *check_supported;

	struct _GtkToggleButton *needs_auth;
	
	CamelProvider *provider;
	CamelProviderType provider_type;
} MailAccountGuiService;

typedef struct _MailAccountGui {
	struct _EAccount *account;
	struct _EMAccountPrefs *dialog;
	struct _GladeXML *xml;
	
	/* identity */
	struct _GtkEntry *full_name;
	struct _GtkEntry *email_address;
	struct _GtkEntry *reply_to;
	struct _GtkEntry *organization;
	
	/* signatures */
	struct _GtkOptionMenu *sig_menu;
	guint sig_added_id;
	guint sig_removed_id;
	guint sig_changed_id;
	const char *sig_uid;
	
	/* incoming mail */
	MailAccountGuiService source;
	struct _GtkToggleButton *source_auto_check;
	struct _GtkSpinButton *source_auto_check_min;
	
	/* extra incoming config */
	GHashTable *extra_config;
	
	/* outgoing mail */
	MailAccountGuiService transport;
	
	/* account management */
	struct _GtkEntry *account_name;
	struct _GtkToggleButton *default_account;
	
	/* special folders */
	struct _GtkButton *drafts_folder_button;
	char *drafts_folder_uri;
	struct _GtkButton *sent_folder_button;
	char *sent_folder_uri;
	struct _GtkButton *restore_folders_button;

	/* always cc/bcc */
	struct _GtkToggleButton *always_cc;
	struct _GtkEntry *cc_addrs;
	struct _GtkToggleButton *always_bcc;
	struct _GtkEntry *bcc_addrs;
	
	/* Security */
	struct _GtkEntry *pgp_key;
	struct _GtkToggleButton *pgp_encrypt_to_self;
	struct _GtkToggleButton *pgp_always_sign;
	struct _GtkToggleButton *pgp_no_imip_sign;
	struct _GtkToggleButton *pgp_always_trust;

	struct _GtkToggleButton *smime_sign_default;
	struct _GtkEntry *smime_sign_key;
	struct _GtkButton *smime_sign_key_select;
	struct _GtkButton *smime_sign_key_clear;
	struct _GtkButton *smime_sign_select;
	struct _GtkToggleButton *smime_encrypt_default;
	struct _GtkToggleButton *smime_encrypt_to_self;
	struct _GtkEntry *smime_encrypt_key;
	struct _GtkButton *smime_encrypt_key_select;
	struct _GtkButton *smime_encrypt_key_clear;
} MailAccountGui;


MailAccountGui *mail_account_gui_new (struct _EAccount *account, struct _EMAccountPrefs *dialog);
void mail_account_gui_setup (MailAccountGui *gui, struct _GtkWidget *top);
gboolean mail_account_gui_save (MailAccountGui *gui);
void mail_account_gui_destroy (MailAccountGui *gui);

gboolean mail_account_gui_identity_complete (MailAccountGui *gui, struct _GtkWidget **incomplete);
gboolean mail_account_gui_source_complete (MailAccountGui *gui, struct _GtkWidget **incomplete);
gboolean mail_account_gui_transport_complete (MailAccountGui *gui, struct _GtkWidget **incomplete);
gboolean mail_account_gui_management_complete (MailAccountGui *gui, struct _GtkWidget **incomplete);

void mail_account_gui_build_extra_conf (MailAccountGui *gui, const char *url);

void mail_account_gui_auto_detect_extra_conf (MailAccountGui *gui);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MAIL_ACCOUNT_GUI_H */
