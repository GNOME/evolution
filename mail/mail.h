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

/* FIXME FIXME FIXME this sucks sucks sucks sucks */

/* folder-browser-factory */
void           folder_browser_factory_init         (void);
BonoboControl *folder_browser_factory_new_control  (const char *uri);

/* folder-browser */
CamelFolder *mail_uri_to_folder (const char *uri);

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

/* mail-ops */
void fetch_mail (GtkWidget *button, gpointer user_data);
void compose_msg (GtkWidget *button, gpointer user_data);
void send_to_url (const char *url);
void forward_msg (GtkWidget *button, gpointer user_data);
void reply_to_sender (GtkWidget *button, gpointer user_data);
void reply_to_all (GtkWidget *button, gpointer user_data);
void delete_msg (GtkWidget *button, gpointer user_data);
void move_msg (GtkWidget *button, gpointer user_data);
void print_msg (GtkWidget *button, gpointer user_data);

void mark_all_seen (BonoboUIHandler *uih, void *user_data, const char *path);
void edit_message (BonoboUIHandler *uih, void *user_data, const char *path);
void expunge_folder (BonoboUIHandler *uih, void *user_data, const char *path);
void filter_edit (BonoboUIHandler *uih, void *user_data, const char *path);
void vfolder_edit_vfolders (BonoboUIHandler *uih, void *user_data, const char *path);
void providers_config (BonoboUIHandler *uih, void *user_data, const char *path);

/* session */
void session_init (void);
char *mail_request_dialog (const char *prompt, gboolean secret, const char *key);
void forget_passwords (BonoboUIHandler *uih, void *user_data, const char *path);
extern CamelSession *session;
