/*
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
 * Authors:
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef EM_COMPOSER_UTILS_H
#define EM_COMPOSER_UTILS_H

#include <em-format/e-mail-part.h>
#include <em-format/e-mail-part-list.h>

#include <composer/e-msg-composer.h>

#include <mail/e-mail-backend.h>
#include <mail/e-mail-enums.h>

G_BEGIN_DECLS

void		em_utils_compose_new_message	(EMsgComposer *composer,
						 CamelFolder *folder);
void		em_utils_compose_new_message_with_mailto
						(EShell *shell,
						 const gchar *mailto,
						 CamelFolder *folder);
void		em_utils_edit_message		(EMsgComposer *composer,
						 CamelFolder *folder,
						 CamelMimeMessage *message,
						 const gchar *message_uid,
						 gboolean keep_signature);
void		em_utils_forward_message	(EMsgComposer *composer,
						 CamelMimeMessage *message,
						 EMailForwardStyle style,
						 CamelFolder *folder,
						 const gchar *uid);
void		em_utils_forward_attachment	(EMsgComposer *composer,
						 CamelMimePart *part,
						 const gchar *subject,
						 CamelFolder *folder,
						 GPtrArray *uids);
void		em_utils_redirect_message	(EMsgComposer *composer,
						 CamelMimeMessage *message);
gchar *		em_utils_construct_composer_text
						(CamelSession *session,
						 CamelMimeMessage *message,
						 EMailPartList *source_formatter);
gboolean	em_utils_is_munged_list_message	(CamelMimeMessage *message);
void		em_utils_get_reply_sender	(CamelMimeMessage *message,
						 CamelInternetAddress *to,
						 CamelNNTPAddress *postto);
void		em_utils_get_reply_all		(ESourceRegistry *registry,
						 CamelMimeMessage *message,
						 CamelInternetAddress *to,
						 CamelInternetAddress *cc,
						 CamelNNTPAddress *postto);
void		em_utils_reply_to_message	(EMsgComposer *composer,
						 CamelMimeMessage *message,
						 CamelFolder *folder,
						 const gchar *message_uid,
						 EMailReplyType type,
						 EMailReplyStyle style,
						 EMailPartList *source,
						 CamelInternetAddress *address);
EDestination **	em_utils_camel_address_to_destination
						(CamelInternetAddress *iaddr);
void		em_configure_new_composer	(EMsgComposer *composer,
						 EMailSession *session);
void		em_utils_get_real_folder_uri_and_message_uid
						(CamelFolder *folder,
						 const gchar *uid,
						 gchar **folder_uri,
						 gchar **message_uid);
ESource *	em_utils_check_send_account_override
						(EShell *shell,
						 CamelMimeMessage *message,
						 CamelFolder *folder);
void		em_utils_apply_send_account_override_to_composer
						(EMsgComposer *composer,
						 CamelFolder *folder);

G_END_DECLS

#endif /* EM_COMPOSER_UTILS_H */
