/*
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
 * Authors:
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __EM_COMPOSER_UTILS_H__
#define __EM_COMPOSER_UTILS_H__

#include <em-format/em-format.h>
#include <composer/e-msg-composer.h>

G_BEGIN_DECLS

void		em_utils_compose_new_message	(EShell *shell,
						 const gchar *from_uri);
EMsgComposer *	em_utils_compose_new_message_with_mailto
						(EShell *shell,
						 const gchar *url,
						 const gchar *from_uri);
GtkWidget *	em_utils_edit_message		(EShell *shell,
						 CamelMimeMessage *message,
						 CamelFolder *folder);
void		em_utils_edit_messages		(EShell *shell,
						 CamelFolder *folder,
						 GPtrArray *uids,
						 gboolean replace);
void		em_utils_forward_attached	(EShell *shell,
						 CamelFolder *folder,
						 GPtrArray *uids,
						 const gchar *from_uri);
void		em_utils_forward_inline		(EShell *shell,
						 CamelFolder *folder,
						 GPtrArray *uids,
						 const gchar *from_uri);
void		em_utils_forward_quoted		(EShell *shell,
						 CamelFolder *folder,
						 GPtrArray *uids,
						 const gchar *from_uri);
EMsgComposer *	em_utils_forward_message	(EShell *shell,
						 CamelMimeMessage *msg,
						 const gchar *from_uri);
void		em_utils_forward_messages	(EShell *shell,
						 CamelFolder *folder,
						 GPtrArray *uids,
						 const gchar *from_uri);
void		em_utils_redirect_message	(EShell *shell,
						 CamelMimeMessage *message);
void		em_utils_redirect_message_by_uid(EShell *shell,
						 CamelFolder *folder,
						 const gchar *uid);
void		em_utils_forward_message_raw	(CamelFolder *folder,
						 CamelMimeMessage *message,
						 const gchar *address,
						 GError **error);
void		em_utils_handle_receipt		(CamelFolder *folder,
						 const gchar *uid,
						 CamelMimeMessage *msg);
void		em_utils_send_receipt		(CamelFolder *folder,
						 CamelMimeMessage *message);

enum {
	REPLY_MODE_SENDER, /* Reply-To?:From */
	REPLY_MODE_FROM,
	REPLY_MODE_ALL,
	REPLY_MODE_LIST
};

gchar *em_utils_construct_composer_text (CamelMimeMessage *message, EMFormat *source);
gboolean em_utils_is_munged_list_message (CamelMimeMessage *message);
void em_utils_get_reply_sender (CamelMimeMessage *message, CamelInternetAddress *to, CamelNNTPAddress *postto);
void em_utils_get_reply_all (CamelMimeMessage *message, CamelInternetAddress *to, CamelInternetAddress *cc, CamelNNTPAddress *postto);
EMsgComposer * em_utils_reply_to_message (EShell *shell, CamelFolder *, const gchar *uid, CamelMimeMessage *message, gint mode, EMFormat *source);
EDestination ** em_utils_camel_address_to_destination (CamelInternetAddress *iaddr);

void em_configure_new_composer (struct _EMsgComposer *composer);

G_END_DECLS

#endif /* __EM_COMPOSER_UTILS_H__ */
