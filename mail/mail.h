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

/* FIXME FIXME FIXME this sucks sucks sucks sucks */

/* folder-browser-factory */
void           folder_browser_factory_init         (void);
BonoboControl *folder_browser_factory_new_control  (void);

/* mail-config */
void mail_config_druid (void);

/* mail-format */
void mail_format_mime_message (CamelMimeMessage *mime_message, GtkBox *box);

EMsgComposer *mail_generate_reply (CamelMimeMessage *mime_message,
				   gboolean to_all);

EMsgComposer *mail_generate_forward (CamelMimeMessage *mime_message,
				     gboolean forward_as_attachment,
				     gboolean keep_attachments);

/* mail-identify */
char *mail_identify_mime_part (CamelMimePart *part);

/* mail-ops */
void fetch_mail (GtkWidget *button, gpointer user_data);
void send_msg (GtkWidget *button, gpointer user_data);
void send_to_url (const char *url);
void forward_msg (GtkWidget *button, gpointer user_data);
void reply_to_sender (GtkWidget *button, gpointer user_data);
void reply_to_all (GtkWidget *button, gpointer user_data);
void delete_msg (GtkWidget *button, gpointer user_data);
void expunge_folder (GtkWidget *button, gpointer user_data);

void filter_edit (GtkWidget *button, gpointer user_data);

/* session */
void session_init (void);
extern CamelSession *session;
