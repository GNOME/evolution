/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * Authors: 
 *  Peter Williams <peterw@ximian.com>
 *  Michael Zucchi <notzed@ximian.com>
 *
 * Copyright 2000, 2001 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of version 2 of the GNU General Public 
 * License as published by the Free Software Foundation.
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

#ifndef MAIL_OPS_H
#define MAIL_OPS_H

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include "camel/camel-store.h"
#include "camel/camel-folder.h"
#include "camel/camel-filter-driver.h"
#include "camel/camel-mime-message.h"
#include "camel/camel-operation.h"

#include "e-util/e-msgport.h"
#include "e-util/e-account.h"

void mail_append_mail (CamelFolder *folder, CamelMimeMessage *message, CamelMessageInfo *info,
		       void (*done)(CamelFolder *folder, CamelMimeMessage *msg, CamelMessageInfo *info, int ok,
				    const char *appended_uid, void *data),
		       void *data);

void mail_transfer_messages (CamelFolder *source, GPtrArray *uids,
			     gboolean delete_from_source,
			     const char *dest_uri,
			     guint32 dest_flags,
			     void (*done) (gboolean ok, void *data),
			     void *data);

/* get a single message, asynchronously */
void mail_get_message (CamelFolder *folder, const char *uid,
		       void (*done) (CamelFolder *folder, const char *uid, CamelMimeMessage *msg, void *data),
		       void *data,
		       EThread *thread);

/* get several messages */
void mail_get_messages (CamelFolder *folder, GPtrArray *uids,
			void (*done) (CamelFolder *folder, GPtrArray *uids, GPtrArray *msgs, void *data),
			void *data);

/* same for a folder */
int mail_get_folder (const char *uri, guint32 flags,
		     void (*done) (char *uri, CamelFolder *folder, void *data), void *data,
		     EThread *thread);

/* and for a store */
int mail_get_store (const char *uri, CamelOperation *op,
		    void (*done) (char *uri, CamelStore *store, void *data), void *data);

/* build an attachment */
void mail_build_attachment (CamelFolder *folder, GPtrArray *uids,
			    void (*done)(CamelFolder *folder, GPtrArray *messages,
					 CamelMimePart *part, char *subject, void *data),
			    void *data);

void mail_sync_folder (CamelFolder *folder,
		       void (*done) (CamelFolder *folder, void *data), void *data);

void mail_sync_store(CamelStore *store, int expunge,
		     void (*done) (CamelStore *store, void *data), void *data);

void mail_refresh_folder (CamelFolder *folder,
			  void (*done) (CamelFolder *folder, void *data),
			  void *data);

void mail_expunge_folder (CamelFolder *folder,
			  void (*done) (CamelFolder *folder, void *data),
			  void *data);

void mail_empty_trash (EAccount *account,
		       void (*done) (EAccount *account, void *data),
		       void *data);

/* get folder info asynchronously */
int mail_get_folderinfo (CamelStore *store, CamelOperation *op,
			 void (*done)(CamelStore *store, CamelFolderInfo *info, void *data),
			 void *data);

/* remove an existing folder */
void mail_remove_folder (const char *uri,
			 void (*done) (char *uri, gboolean removed, void *data),
			 void *data);

/* transfer (copy/move) a folder */
void mail_xfer_folder (const char *src_uri, const char *dest_uri, gboolean remove_source,
		       void (*done) (char *src_uri, char *dest_uri, gboolean remove_source,
				     CamelFolder *folder, void *data),
		       void *data);

/* save messages */
int mail_save_messages (CamelFolder *folder, GPtrArray *uids, const char *path,
			void (*done) (CamelFolder *folder, GPtrArray *uids, char *path, void *data),
			void *data);

int mail_save_part (CamelMimePart *part, const char *path,
		    void (*done)(CamelMimePart *part, char *path, int saved, void *data),
		    void *data);

/* yeah so this is messy, but it does a lot, maybe i can consolidate all user_data's to be the one */
void mail_send_queue (CamelFolder *queue, const char *destination,
		      const char *type, CamelOperation *cancel,
		      CamelFilterGetFolderFunc get_folder, void *get_data,
		      CamelFilterStatusFunc *status, void *status_data,
		      void (*done)(char *destination, void *data),
		      void *data);

void mail_fetch_mail (const char *source, int keep,
		      const char *type, CamelOperation *cancel,
		      CamelFilterGetFolderFunc get_folder, void *get_data,
		      CamelFilterStatusFunc *status, void *status_data,
		      void (*done)(char *source, void *data),
		      void *data);

void mail_filter_folder (CamelFolder *source_folder, GPtrArray *uids,
			 const char *type, gboolean notify,
			 CamelOperation *cancel);

/* convenience functions for above */
void mail_filter_on_demand (CamelFolder *folder, GPtrArray *uids);
void mail_filter_junk (CamelFolder *folder, GPtrArray *uids);

/* Work Offline */
void mail_prep_offline(const char *uri, CamelOperation *cancel,
		       void (*done)(const char *, void *data),
		       void *data);
int mail_store_set_offline(CamelStore *store, gboolean offline,
			   void (*done)(CamelStore *, void *data),
			   void *data);

/* filter driver execute shell command async callback */
void mail_execute_shell_command (CamelFilterDriver *driver, int argc, char **argv, void *data);

int mail_check_service(const char *url, CamelProviderType type,
		       void (*done)(const char *url, CamelProviderType type, GList *authtypes, void *data), void *data);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MAIL_OPS_H */
