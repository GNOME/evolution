/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@helixcode.com>
 *
 *  Copyright 2000 Helix Code, Inc. (www.helixcode.com)
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

#ifndef MAIL_CALLBACKS_H
#define MAIL_CALLBACKS_H

#include <gnome.h>
#include <camel/camel.h>
#include "composer/e-msg-composer.h"
#include "mail-types.h"
#include "evolution-storage.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

void enumerate_msg (MessageList *ml, const char *uid, gpointer data);

void fetch_mail            (GtkWidget *widget, gpointer user_data);
void send_queued_mail      (GtkWidget *widget, gpointer user_data);
void send_receive_mail     (GtkWidget *widget, gpointer user_data);

void compose_msg           (GtkWidget *widget, gpointer user_data);
void send_to_url           (const char *url);

enum {
	FORWARD_INLINE,
	FORWARD_QUOTED,
	FORWARD_ATTACHED
};

void forward_messages      (CamelFolder *folder, GPtrArray *uids, int flag);
void forward_inlined       (GtkWidget *widget, gpointer user_data);
void forward_quoted        (GtkWidget *widget, gpointer user_data);
void forward_attached      (GtkWidget *widget, gpointer user_data);

void reply_to_sender       (GtkWidget *widget, gpointer user_data);
void reply_to_all          (GtkWidget *widget, gpointer user_data);
void delete_msg            (GtkWidget *widget, gpointer user_data);
void undelete_msg          (GtkWidget *widget, gpointer user_data);
void move_msg              (GtkWidget *widget, gpointer user_data);
void copy_msg              (GtkWidget *widget, gpointer user_data);
void apply_filters         (GtkWidget *widget, gpointer user_data);
void print_msg             (GtkWidget *widget, gpointer user_data);
void print_preview_msg     (GtkWidget *widget, gpointer user_data);
void edit_msg              (GtkWidget *widget, gpointer user_data);
void open_msg              (GtkWidget *widget, gpointer user_data);
void save_msg              (GtkWidget *widget, gpointer user_data);
void view_msg              (GtkWidget *widget, gpointer user_data);
void view_source           (GtkWidget *widget, gpointer user_data);
void next_msg              (GtkWidget *widget, gpointer user_data);
void previous_msg          (GtkWidget *widget, gpointer user_data);

void select_all            (BonoboUIComponent *uih, void *user_data, const char *path);
void invert_selection      (BonoboUIComponent *uih, void *user_data, const char *path);
void mark_as_seen          (BonoboUIComponent *uih, void *user_data, const char *path);
void mark_all_as_seen      (BonoboUIComponent *uih, void *user_data, const char *path);
void mark_as_unseen        (BonoboUIComponent *uih, void *user_data, const char *path);
void edit_message          (BonoboUIComponent *uih, void *user_data, const char *path);
void open_message          (BonoboUIComponent *uih, void *user_data, const char *path);
void expunge_folder        (BonoboUIComponent *uih, void *user_data, const char *path);
void filter_edit           (BonoboUIComponent *uih, void *user_data, const char *path);
void vfolder_edit_vfolders (BonoboUIComponent *uih, void *user_data, const char *path);
void providers_config      (BonoboUIComponent *uih, void *user_data, const char *path);
void manage_subscriptions  (BonoboUIComponent *uih, void *user_data, const char *path);

void configure_folder      (BonoboUIComponent *uih, void *user_data, const char *path);

void stop_threads          (BonoboUIComponent *uih, void *user_data, const char *path);

void mail_reply            (CamelFolder *folder, CamelMimeMessage *msg, const char *uid, gboolean to_all);
void composer_send_cb      (EMsgComposer *composer, gpointer data);
void composer_postpone_cb  (EMsgComposer *composer, gpointer data);

void forward_messages	   (CamelFolder *folder, GPtrArray *uids, gboolean inline);

void mail_print_preview_msg (MailDisplay *md);
void mail_print_msg         (MailDisplay *md);


/* CamelStore callbacks */
void folder_created (CamelStore *store, CamelFolderInfo *fi);
void folder_deleted (CamelStore *store, CamelFolderInfo *fi);

void mail_storage_create_folder (EvolutionStorage *storage, CamelStore *store, CamelFolderInfo *fi);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ! MAIL_CALLBACKS_H */
