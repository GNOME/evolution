/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * Author : 
 *  Peter Williams <peterw@helixcode.com>
 *
 * Copyright 2000 Helix Code, Inc. (http://www.helixcode.com)
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <camel/camel.h>
#include "mail-threads.h"
#include "evolution-storage.h"	/*EvolutionStorage */
#include "composer/e-msg-composer.h"	/*EMsgComposer */
#include "message-list.h"	/*MessageList */

void mail_do_fetch_mail (const gchar *source_url, gboolean keep_on_server,
			 CamelFolder *destination,
			 gpointer hook_func, gpointer hook_data);
void mail_do_send_mail (const char *xport_uri,
			CamelMimeMessage *message,
			CamelFolder *done_folder,
			const char *done_uid,
			guint32 done_flags, GtkWidget *composer);
void mail_do_expunge_folder (CamelFolder *folder);
void mail_do_transfer_messages (CamelFolder *source, GPtrArray *uids,
				gboolean delete_from_source,
				gchar *dest_uri);
void mail_do_flag_messages (CamelFolder *source, GPtrArray *uids,
			    gboolean invert,
			    guint32 mask, guint32 set);
void mail_do_flag_all_messages (CamelFolder *source, gboolean invert,
				guint32 mask, guint32 set);
void mail_do_scan_subfolders (const gchar *source_uri, EvolutionStorage *storage);
void mail_do_attach_message (CamelFolder *folder, const char *uid,
			     EMsgComposer *composer);
void mail_do_forward_message (CamelMimeMessage *basis, CamelFolder *source,
			      GPtrArray *uids,	/*array of allocated gchar *, will all be freed */
			      EMsgComposer *composer);
void mail_do_load_folder (FolderBrowser *fb, const char *url);
void mail_do_create_folder (const Evolution_ShellComponentListener listener,
			    const char *uri, const char *type);
void mail_do_sync_folder (CamelFolder *folder);
void mail_do_display_message (MessageList *ml, const char *uid,
			      gint (*timeout) (gpointer));
void mail_do_edit_messages (CamelFolder *folder, GPtrArray *uids,
			    GtkSignalFunc signal);
void mail_do_setup_draftbox (void);
void mail_do_setup_outbox (void);
void mail_do_setup_sentbox (void);
void mail_do_view_messages (CamelFolder *folder, GPtrArray *uids,
			    FolderBrowser *fb);

/* This actually lives in message-list.c */
void mail_do_regenerate_messagelist (MessageList *list,
				     const gchar *search);
