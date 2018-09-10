/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Peter Williams <peterw@ximian.com>
 *		Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__LIBEMAIL_ENGINE_H_INSIDE__) && !defined (LIBEMAIL_ENGINE_COMPILATION)
#error "Only <libemail-engine/libemail-engine.h> should be included directly."
#endif

#ifndef MAIL_OPS_H
#define MAIL_OPS_H

G_BEGIN_DECLS

#include <camel/camel.h>
#include <libemail-engine/e-mail-session.h>
#include <libemail-engine/mail-mt.h>

/* Used to "tag" messages as being edited */
#define MAIL_USER_KEY_EDITING	"mail-user-key-editing"

void		mail_transfer_messages		(EMailSession *session,
						 CamelFolder *source,
						 GPtrArray *uids,
						 gboolean delete_from_source,
						 const gchar *dest_uri,
						 guint32 dest_flags,
						 void (*done) (gboolean ok, gpointer data),
						 gpointer data);

void mail_sync_folder (CamelFolder *folder,
		       gboolean test_for_expunge,
		       void (*done) (CamelFolder *folder, gpointer data), gpointer data);

void mail_sync_store (CamelStore *store, gint expunge,
		     void (*done) (CamelStore *store, gpointer data), gpointer data);

void		mail_empty_trash		(CamelStore *store);

/* yeah so this is messy, but it does a lot, maybe i can consolidate all user_data's to be the one */
void		mail_send_queue			(EMailSession *session,
						 CamelFolder *queue,
						 CamelTransport *transport,
						 const gchar *type,
						 gboolean immediately,
						 GCancellable *cancellable,
						 CamelFilterGetFolderFunc get_folder,
						 gpointer get_data,
						 CamelFilterStatusFunc status,
						 gpointer status_data,
						  /* Return %TRUE, when the error had been reported to the user */
						 gboolean (* done)(gpointer data, const GError *error, const GPtrArray *failed_uids),
						 gpointer data);

typedef void	(*MailProviderFetchLockFunc)	(const gchar *source);
typedef void	(*MailProviderFetchUnlockFunc)	(const gchar *source);
typedef CamelFolder *
		(*MailProviderFetchInboxFunc)	(const gchar *source,
						 GCancellable *cancellable,
						 GError **error);

void		mail_fetch_mail			(CamelStore *store,
						 const gchar *type,
						 MailProviderFetchLockFunc lock_func,
						 MailProviderFetchUnlockFunc unlock_func,
						 MailProviderFetchInboxFunc fetch_inbox_func,
						 GCancellable *cancellable,
						 CamelFilterGetFolderFunc get_folder,
						 gpointer get_data,
						 CamelFilterStatusFunc status,
						 gpointer status_data,
						 void (*done)(gpointer data),
						 gpointer data);

void		mail_filter_folder		(EMailSession *session,
						 CamelFolder *source_folder,
						 GPtrArray *uids,
						 const gchar *type,
						 gboolean notify);

void		mail_process_folder_changes	(CamelFolder *folder,
						 CamelFolderChangeInfo *changes,
						 void (*process) (CamelFolder *folder,
								  CamelFolderChangeInfo *changes,
								  GCancellable *cancellable,
								  GError **error,
								  gpointer user_data),
						 void (* done) (gpointer user_data),
						 gpointer user_data);

/* filter driver execute shell command async callback */
void mail_execute_shell_command (CamelFilterDriver *driver, gint argc, gchar **argv, gpointer data);

G_END_DECLS

#endif /* MAIL_OPS_H */
