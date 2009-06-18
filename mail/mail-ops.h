/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Peter Williams <peterw@ximian.com>
 *		Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef MAIL_OPS_H
#define MAIL_OPS_H

G_BEGIN_DECLS

#include "mail-mt.h"

#include "camel/camel-store.h"
#include "camel/camel-folder.h"
#include "camel/camel-filter-driver.h"
#include "camel/camel-mime-message.h"
#include "camel/camel-operation.h"

#include "libedataserver/e-account.h"

void mail_append_mail (CamelFolder *folder, CamelMimeMessage *message, CamelMessageInfo *info,
		       void (*done)(CamelFolder *folder, CamelMimeMessage *msg, CamelMessageInfo *info, gint ok,
				    const gchar *appended_uid, gpointer data),
		       gpointer data);

void mail_transfer_messages (CamelFolder *source, GPtrArray *uids,
			     gboolean delete_from_source,
			     const gchar *dest_uri,
			     guint32 dest_flags,
			     void (*done) (gboolean ok, gpointer data),
			     gpointer data);

/* get a single message, asynchronously */
void mail_get_message (CamelFolder *folder, const gchar *uid,
		       void (*done) (CamelFolder *folder, const gchar *uid, CamelMimeMessage *msg, gpointer data),
		       gpointer data,
		       MailMsgDispatchFunc dispatch);

CamelOperation *
mail_get_messagex(CamelFolder *folder, const gchar *uid,
		  void (*done) (CamelFolder *folder, const gchar *uid, CamelMimeMessage *msg, gpointer data, CamelException *),
		  gpointer data, MailMsgDispatchFunc dispatch);

/* get several messages */
void mail_get_messages (CamelFolder *folder, GPtrArray *uids,
			void (*done) (CamelFolder *folder, GPtrArray *uids, GPtrArray *msgs, gpointer data),
			gpointer data);

/* same for a folder */
gint mail_get_folder (const gchar *uri, guint32 flags,
		     void (*done) (gchar *uri, CamelFolder *folder, gpointer data), gpointer data,
		     MailMsgDispatchFunc dispatch);

/* get quota information for a folder */
gint mail_get_folder_quota (CamelFolder *folder,
		 void (*done)(CamelFolder *folder, CamelFolderQuotaInfo *quota, gpointer data),
		 gpointer data, MailMsgDispatchFunc dispatch);

/* and for a store */
gint mail_get_store (const gchar *uri, CamelOperation *op,
		    void (*done) (gchar *uri, CamelStore *store, gpointer data), gpointer data);

/* build an attachment */
void mail_build_attachment (CamelFolder *folder, GPtrArray *uids,
			    void (*done)(CamelFolder *folder, GPtrArray *messages,
					 CamelMimePart *part, gchar *subject, gpointer data),
			    gpointer data);

void mail_sync_folder (CamelFolder *folder,
		       void (*done) (CamelFolder *folder, gpointer data), gpointer data);

void mail_sync_store(CamelStore *store, gint expunge,
		     void (*done) (CamelStore *store, gpointer data), gpointer data);

void mail_refresh_folder (CamelFolder *folder,
			  void (*done) (CamelFolder *folder, gpointer data),
			  gpointer data);

void mail_expunge_folder (CamelFolder *folder,
			  void (*done) (CamelFolder *folder, gpointer data),
			  gpointer data);

void mail_empty_trash (EAccount *account,
		       void (*done) (EAccount *account, gpointer data),
		       gpointer data);

/* get folder info asynchronously */
gint mail_get_folderinfo (CamelStore *store, CamelOperation *op,
			 gboolean (*done)(CamelStore *store, CamelFolderInfo *info, gpointer data),
			 gpointer data);

/* remove an existing folder */
void mail_remove_folder (CamelFolder *folder,
			 void (*done) (CamelFolder *folder, gboolean removed, CamelException *ex, gpointer data),
			 gpointer data);

/* transfer (copy/move) a folder */
void mail_xfer_folder (const gchar *src_uri, const gchar *dest_uri, gboolean remove_source,
		       void (*done) (gchar *src_uri, gchar *dest_uri, gboolean remove_source,
				     CamelFolder *folder, gpointer data),
		       gpointer data);

/* save messages */
gint mail_save_messages (CamelFolder *folder, GPtrArray *uids, const gchar *path,
			void (*done) (CamelFolder *folder, GPtrArray *uids, gchar *path, gpointer data),
			gpointer data);

gint mail_save_part (CamelMimePart *part, const gchar *path,
		    void (*done)(CamelMimePart *part, gchar *path, gint saved, gpointer data),
		    gpointer data, gboolean readonly);

/* yeah so this is messy, but it does a lot, maybe i can consolidate all user_data's to be the one */
void mail_send_queue (CamelFolder *queue, const gchar *destination,
		      const gchar *type, CamelOperation *cancel,
		      CamelFilterGetFolderFunc get_folder, gpointer get_data,
		      CamelFilterStatusFunc *status, gpointer status_data,
		      void (*done)(const gchar *destination, gpointer data),
		      gpointer data);

void mail_fetch_mail (const gchar *source, gint keep,
		      const gchar *type, CamelOperation *cancel,
		      CamelFilterGetFolderFunc get_folder, gpointer get_data,
		      CamelFilterStatusFunc *status, gpointer status_data,
		      void (*done)(const gchar *source, gpointer data),
		      gpointer data);

void mail_filter_folder (CamelFolder *source_folder, GPtrArray *uids,
			 const gchar *type, gboolean notify,
			 CamelOperation *cancel);

/* convenience functions for above */
void mail_filter_on_demand (CamelFolder *folder, GPtrArray *uids);
void mail_filter_junk (CamelFolder *folder, GPtrArray *uids);

/* Work Offline */
void mail_prep_offline(const gchar *uri, CamelOperation *cancel,
		       void (*done)(const gchar *, gpointer data),
		       gpointer data);
gint mail_store_set_offline(CamelStore *store, gboolean offline,
			   void (*done)(CamelStore *, gpointer data),
			   gpointer data);
gint mail_store_prepare_offline (CamelStore *store);

/* filter driver execute shell command async callback */
void mail_execute_shell_command (CamelFilterDriver *driver, gint argc, gchar **argv, gpointer data);

gint mail_check_service(const gchar *url, CamelProviderType type,
		       void (*done)(const gchar *url, CamelProviderType type, GList *authtypes, gpointer data), gpointer data);

G_END_DECLS

#endif /* MAIL_OPS_H */
