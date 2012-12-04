/*
 * e-mail-reader-utils.h
 *
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/* Miscellaneous utility functions used by EMailReader actions. */

#ifndef E_MAIL_READER_UTILS_H
#define E_MAIL_READER_UTILS_H

#include <mail/e-mail-reader.h>

G_BEGIN_DECLS

typedef struct _EMailReaderHeader EMailReaderHeader;

struct _EMailReaderHeader {
	gchar *name;
	guint enabled : 1;
	guint is_default : 1;
};

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
guint		e_mail_reader_open_selected	(EMailReader *reader);
void		e_mail_reader_print		(EMailReader *reader,
						 GtkPrintOperationAction action);
void		e_mail_reader_remove_attachments
						(EMailReader *reader);
void		e_mail_reader_remove_duplicates	(EMailReader *reader);
void		e_mail_reader_reply_to_message	(EMailReader *reader,
						 CamelMimeMessage *message,
						 EMailReplyType reply_type);
void		e_mail_reader_save_messages	(EMailReader *reader);
void		e_mail_reader_select_next_message
						(EMailReader *reader,
						 gboolean or_else_previous);
void		e_mail_reader_create_filter_from_selected
						(EMailReader *reader,
						 gint filter_type);
void		e_mail_reader_create_vfolder_from_selected
						(EMailReader *reader,
						 gint filter_type);

EMailReaderHeader *
		e_mail_reader_header_from_xml	(const gchar *xml);
gchar *		e_mail_reader_header_to_xml	(EMailReaderHeader *header);
void		e_mail_reader_header_free	(EMailReaderHeader *header);

void		e_mail_reader_parse_message
						(EMailReader *reader,
						 CamelFolder *folder,
						 const gchar *message_uid,
						 CamelMimeMessage *message,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
EMailPartList *	e_mail_reader_parse_message_finish
						(EMailReader *reader,
						 GAsyncResult *result);

G_END_DECLS

#endif /* E_MAIL_READER_UTILS_H */
