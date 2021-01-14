/*
 * e-mail-reader-utils.h
 *
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/* Miscellaneous utility functions used by EMailReader actions. */

#ifndef E_MAIL_READER_UTILS_H
#define E_MAIL_READER_UTILS_H

#include <mail/e-mail-reader.h>

G_BEGIN_DECLS

gboolean	e_mail_reader_confirm_delete	(EMailReader *reader);
void		e_mail_reader_delete_folder	(EMailReader *reader,
						 CamelFolder *folder);
void		e_mail_reader_delete_folder_name
						(EMailReader *reader,
						 CamelStore *store,
						 const gchar *folder_name);
void		e_mail_reader_expunge_folder	(EMailReader *reader,
						 CamelFolder *folder);
void		e_mail_reader_expunge_folder_name
						(EMailReader *reader,
						 CamelStore *store,
						 const gchar *folder_name);
void		e_mail_reader_empty_junk_folder	(EMailReader *reader,
						 CamelFolder *folder);
void		e_mail_reader_empty_junk_folder_name
						(EMailReader *reader,
						 CamelStore *store,
						 const gchar *folder_name);
void		e_mail_reader_refresh_folder	(EMailReader *reader,
						 CamelFolder *folder);
void		e_mail_reader_refresh_folder_name
						(EMailReader *reader,
						 CamelStore *store,
						 const gchar *folder_name);
void		e_mail_reader_unsubscribe_folder_name
						(EMailReader *reader,
						 CamelStore *store,
						 const gchar *folder_name);
guint		e_mail_reader_mark_selected	(EMailReader *reader,
						 guint32 mask,
						 guint32 set);
typedef enum {
	E_IGNORE_THREAD_WHOLE_SET,
	E_IGNORE_THREAD_WHOLE_UNSET,
	E_IGNORE_THREAD_SUBSET_SET,
	E_IGNORE_THREAD_SUBSET_UNSET
} EIgnoreThreadKind;

void		e_mail_reader_mark_selected_ignore_thread
						(EMailReader *reader,
						 EIgnoreThreadKind kind);
guint		e_mail_reader_open_selected	(EMailReader *reader);
void		e_mail_reader_print		(EMailReader *reader,
						 GtkPrintOperationAction action);
void		e_mail_reader_remove_attachments
						(EMailReader *reader);
void		e_mail_reader_remove_duplicates	(EMailReader *reader);
void		e_mail_reader_edit_messages	(EMailReader *reader,
						 CamelFolder *folder,
						 GPtrArray *uids,
						 gboolean replace,
						 gboolean keep_signature);
void		e_mail_reader_forward_messages	(EMailReader *reader,
						 CamelFolder *folder,
						 GPtrArray *uids,
						 EMailForwardStyle style);
void		e_mail_reader_reply_to_message	(EMailReader *reader,
						 CamelMimeMessage *message,
						 EMailReplyType reply_type);
void		e_mail_reader_save_messages	(EMailReader *reader);
void		e_mail_reader_select_next_message
						(EMailReader *reader,
						 gboolean or_else_previous);
void		e_mail_reader_select_previous_message
						(EMailReader *reader,
						 gboolean or_else_next);
void		e_mail_reader_create_filter_from_selected
						(EMailReader *reader,
						 gint filter_type);
void		e_mail_reader_create_vfolder_from_selected
						(EMailReader *reader,
						 gint filter_type);

void		e_mail_reader_parse_message	(EMailReader *reader,
						 CamelFolder *folder,
						 const gchar *message_uid,
						 CamelMimeMessage *message,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
EMailPartList *	e_mail_reader_parse_message_finish
						(EMailReader *reader,
						 GAsyncResult *result,
						 GError **error);
gboolean	e_mail_reader_utils_get_mark_seen_setting
						(EMailReader *reader,
						 gint *out_timeout_interval);
void		e_mail_reader_utils_get_selection_or_message
						(EMailReader *reader,
						 CamelMimeMessage *preloaded_message,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
CamelMimeMessage *
		e_mail_reader_utils_get_selection_or_message_finish
						(EMailReader *reader,
						 GAsyncResult *result,
						 gboolean *out_is_selection,
						 CamelFolder **out_folder,
						 const gchar **out_message_uid, /* free with camel_pstring_free() */
						 EMailPartList **out_part_list,
						 EMailPartValidityFlags *out_orig_validity_pgp_sum,
						 EMailPartValidityFlags *out_orig_validity_smime_sum,
						 GError **error);

G_END_DECLS

#endif /* E_MAIL_READER_UTILS_H */
