/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2003 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef __EM_COMPOSER_UTILS_H__
#define __EM_COMPOSER_UTILS_H__

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

struct _CamelFolder;
struct _CamelMimeMessage;
struct _EMsgComposer;

void em_composer_utils_setup_callbacks (struct _EMsgComposer *composer, struct _CamelFolder *folder, const char *uid,
					guint32 flags, guint32 set, struct _CamelFolder *drafts, const char *drafts_uid);

#define em_composer_utils_setup_default_callbacks(composer) em_composer_utils_setup_callbacks (composer, NULL, NULL, 0, 0, NULL, NULL)

void em_utils_composer_send_cb(struct _EMsgComposer *composer, gpointer user_data);
void em_utils_composer_save_draft_cb(struct _EMsgComposer *composer, int quit, gpointer user_data);

void em_utils_compose_new_message (const char *fromuri);

/* FIXME: mailto?  url?  should make up its mind what its called.  imho use 'uri' */
void em_utils_compose_new_message_with_mailto (const char *url, const char *fromuri);
void em_utils_post_to_folder (struct _CamelFolder *folder);
void em_utils_post_to_url (const char *url);

void em_utils_edit_message (struct _CamelMimeMessage *message);
void em_utils_edit_messages (struct _CamelFolder *folder, GPtrArray *uids);

void em_utils_forward_attached (struct _CamelFolder *folder, GPtrArray *uids, const char *fromuri);
void em_utils_forward_inline (struct _CamelFolder *folder, GPtrArray *uids, const char *fromuri);
void em_utils_forward_quoted (struct _CamelFolder *folder, GPtrArray *uids, const char *fromuri);

void em_utils_forward_message (struct _CamelMimeMessage *msg, const char *fromuri);
void em_utils_forward_messages (struct _CamelFolder *folder, GPtrArray *uids, const char *fromuri);

void em_utils_redirect_message (struct _CamelMimeMessage *message);
void em_utils_redirect_message_by_uid (struct _CamelFolder *folder, const char *uid);

enum {
	REPLY_MODE_SENDER,
	REPLY_MODE_ALL,
	REPLY_MODE_LIST
};

void em_utils_reply_to_message (struct _CamelMimeMessage *message, int mode);
void em_utils_reply_to_message_by_uid (struct _CamelFolder *folder, const char *uid, int mode);

void em_utils_post_reply_to_message_by_uid (struct _CamelFolder *folder, const char *uid);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __EM_COMPOSER_UTILS_H__ */
