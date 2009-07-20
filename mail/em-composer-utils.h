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

#include <glib.h>
#include <camel/camel-nntp-address.h>

#include <mail/em-format.h>
#include <composer/e-msg-composer.h>

G_BEGIN_DECLS

void em_composer_utils_setup_callbacks (EMsgComposer *composer, CamelFolder *folder, const gchar *uid,
					guint32 flags, guint32 set, CamelFolder *drafts, const gchar *drafts_uid);

#define em_composer_utils_setup_default_callbacks(composer) em_composer_utils_setup_callbacks (composer, NULL, NULL, 0, 0, NULL, NULL)

void em_utils_composer_send_cb(EMsgComposer *composer, gpointer user_data);
void em_utils_composer_save_draft_cb(EMsgComposer *composer, gpointer user_data);

void em_utils_compose_new_message (const gchar *fromuri);
EMsgComposer * em_utils_compose_lite_new_message (const gchar *fromuri);

/* FIXME: mailto?  url?  should make up its mind what its called.  imho use 'uri' */
EMsgComposer * em_utils_compose_new_message_with_mailto (const gchar *url, const gchar *fromuri);

GtkWidget * em_utils_edit_message (CamelMimeMessage *message, CamelFolder *folder);
void em_utils_edit_messages (CamelFolder *folder, GPtrArray *uids, gboolean replace);

void em_utils_forward_attached (CamelFolder *folder, GPtrArray *uids, const gchar *fromuri);
void em_utils_forward_inline (CamelFolder *folder, GPtrArray *uids, const gchar *fromuri);
void em_utils_forward_quoted (CamelFolder *folder, GPtrArray *uids, const gchar *fromuri);

struct _EMsgComposer * em_utils_forward_message (CamelMimeMessage *msg, const gchar *fromuri);
void em_utils_forward_messages (CamelFolder *folder, GPtrArray *uids, const gchar *fromuri);

void em_utils_redirect_message (CamelMimeMessage *message);
void em_utils_redirect_message_by_uid (CamelFolder *folder, const gchar *uid);

void em_utils_forward_message_raw (CamelFolder *folder, CamelMimeMessage *message, const gchar *address, CamelException *ex);

void em_utils_handle_receipt (CamelFolder *folder, const gchar *uid, CamelMimeMessage *msg);
void em_utils_send_receipt   (CamelFolder *folder, CamelMimeMessage *message);

enum {
	REPLY_MODE_SENDER,
	REPLY_MODE_ALL,
	REPLY_MODE_LIST
};

gchar *em_utils_construct_composer_text (CamelMimeMessage *message, EMFormat *source);
void em_utils_get_reply_sender (CamelMimeMessage *message, CamelInternetAddress *to, CamelNNTPAddress *postto);
void em_utils_get_reply_all (CamelMimeMessage *message, CamelInternetAddress *to, CamelInternetAddress *cc, CamelNNTPAddress *postto);
EMsgComposer * em_utils_reply_to_message (CamelFolder *, const gchar *uid, CamelMimeMessage *message, gint mode, EMFormat *source);
EDestination ** em_utils_camel_address_to_destination (CamelInternetAddress *iaddr);

G_END_DECLS

#endif /* __EM_COMPOSER_UTILS_H__ */
