/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  Copyright 2000, Helix Code, Inc. (http://www.helixcode.com)
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
 */

#include <gtkhtml/gtkhtml.h>
#include "camel/camel.h"
#include "composer/e-msg-composer.h"
#include "mail-config.h"
#include "mail-config-gui.h"
#include "folder-browser.h"

/* FIXME FIXME FIXME this sucks sucks sucks sucks */

/* folder-browser-factory */
void           folder_browser_factory_init         (void);
BonoboControl *folder_browser_factory_new_control  (const char *uri);

/* mail-config */
void mail_config_druid (void);

/* mail-crypto */
char *mail_crypto_openpgp_decrypt (const char *ciphertext,
				   const char *passphrase,
				   CamelException *ex);

char *mail_crypto_openpgp_encrypt (const char *plaintext,
				   const GPtrArray *recipients,
				   const char *passphrase,
				   gboolean sign,
				   CamelException *ex);
/* FIXME: add encryption & signing functions */

/* mail-format */
void mail_format_mime_message (CamelMimeMessage *mime_message,
			       GtkHTML *html, GtkHTMLStream *stream,
			       CamelMimeMessage *root_message);

EMsgComposer *mail_generate_reply (CamelMimeMessage *mime_message,
				   gboolean to_all);

char *mail_get_message_body (CamelDataWrapper *data, gboolean want_plain,
			     gboolean *is_html);

/* mail-identify */
char *mail_identify_mime_part (CamelMimePart *part);

/* mail-callbacks */
void fetch_mail (GtkWidget *widget, gpointer user_data);
void compose_msg (GtkWidget *widget, gpointer user_data);
void send_to_url (const char *url);
void forward_msg (GtkWidget *widget, gpointer user_data);
void reply_to_sender (GtkWidget *widget, gpointer user_data);
void reply_to_all (GtkWidget *widget, gpointer user_data);
void delete_msg (GtkWidget *widget, gpointer user_data);
void move_msg (GtkWidget *widget, gpointer user_data);
void print_msg (GtkWidget *widget, gpointer user_data);
void edit_msg (GtkWidget *widget, gpointer user_data);
void view_msg (GtkWidget *widget, gpointer user_data);

void mark_all_seen (BonoboUIHandler *uih, void *user_data, const char *path);
void edit_message (BonoboUIHandler *uih, void *user_data, const char *path);
void view_message (BonoboUIHandler *uih, void *user_data, const char *path);
void expunge_folder (BonoboUIHandler *uih, void *user_data, const char *path);
void filter_edit (BonoboUIHandler *uih, void *user_data, const char *path);
void vfolder_edit_vfolders (BonoboUIHandler *uih, void *user_data, const char *path);
void providers_config (BonoboUIHandler *uih, void *user_data, const char *path);

void configure_folder(BonoboUIHandler *uih, void *user_data, const char *path);

void mail_reply (CamelFolder *folder, CamelMimeMessage *msg, const char *uid, gboolean to_all);
void composer_send_cb (EMsgComposer *composer, gpointer data);
void mail_print_msg (MailDisplay *md);

/* mail view */
GtkWidget *mail_view_create (CamelFolder *source, const char *uid, CamelMimeMessage *msg);

/* session */
void session_init (void);
char *mail_request_dialog (const char *prompt, gboolean secret, const char *key);
void forget_passwords (BonoboUIHandler *uih, void *user_data, const char *path);
extern CamelSession *session;
