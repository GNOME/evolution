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
#include <filter/filter-driver.h>
#include "mail-threads.h"
#include "evolution-storage.h"	/*EvolutionStorage */
#include "composer/e-msg-composer.h"	/*EMsgComposer */
#include "message-list.h"	/*MessageList */
#include "mail-mt.h"

void mail_do_fetch_mail (const gchar *source_url, gboolean keep_on_server,
			 CamelFolder *destination,
			 gpointer hook_func, gpointer hook_data);

void mail_do_filter_ondemand (CamelFolder *source, GPtrArray *uids);

void mail_do_send_mail (const char *xport_uri,
			CamelMimeMessage *message,
			CamelFolder *done_folder,
			const char *done_uid,
			guint32 done_flags, GtkWidget *composer);
void mail_do_send_queue (CamelFolder *folder_queue,
			 const char *xport_uri);
void mail_do_append_mail (CamelFolder *folder,
			  CamelMimeMessage *message,
			  CamelMessageInfo *info);
void mail_do_transfer_messages (CamelFolder *source, GPtrArray *uids,
				gboolean delete_from_source,
				gchar *dest_uri);
void mail_do_flag_messages (CamelFolder *source, GPtrArray *uids,
			    gboolean invert,
			    guint32 mask, guint32 set);
void mail_do_flag_all_messages (CamelFolder *source, gboolean invert,
				guint32 mask, guint32 set);
void mail_do_scan_subfolders (CamelStore *store, EvolutionStorage *storage);
void mail_do_create_folder (const GNOME_Evolution_ShellComponentListener listener,
			    const char *uri, const char *type);
void mail_do_setup_trash (const char *name, const char *store_uri, CamelFolder **folder);

/* get a single message, asynchronously */
void mail_get_message(CamelFolder *folder, const char *uid,
		      void (*done) (CamelFolder *folder, char *uid, CamelMimeMessage *msg, void *data), void *data,
		      EThread *thread);

/* get several messages */
void mail_get_messages(CamelFolder *folder, GPtrArray *uids,
		       void (*done) (CamelFolder *folder, GPtrArray *uids, GPtrArray *msgs, void *data), void *data);

/* same for a folder */
int mail_get_folder(const char *uri,
		    void (*done) (char *uri, CamelFolder *folder, void *data), void *data);

/* build an attachment */
void mail_build_attachment(CamelFolder *folder, GPtrArray *uids,
			   void (*done)(CamelFolder *folder, GPtrArray *messages, CamelMimePart *part, char *subject, void *data), void *data);

void mail_sync_folder(CamelFolder *folder,
		      void (*done) (CamelFolder *folder, void *data), void *data);
void mail_expunge_folder(CamelFolder *folder,
			 void (*done) (CamelFolder *folder, void *data), void *data);

/* get folder info asynchronously */
int mail_get_folderinfo(CamelStore *store,
			void (*done)(CamelStore *store, CamelFolderInfo *info, void *data), void *data);

/* create a new mail folder */
void mail_create_folder(const char *uri,
			void (*done) (char *uri, CamelFolder *folder, void *data), void *data);

/* save messages */
int mail_save_messages(CamelFolder *folder, GPtrArray *uids, const char *path,
		       void (*done) (CamelFolder *folder, GPtrArray *uids, char *path, void *data), void *data);

int mail_send_mail(const char *uri, CamelMimeMessage *message,
		   void (*done) (char *uri, CamelMimeMessage *message, gboolean sent, void *data), void *data);

/* scan subfolders and add them to the storage, synchronous */
/* FIXME: Move this to component-factory.c */
void mail_scan_subfolders(CamelStore *store, EvolutionStorage *storage);

