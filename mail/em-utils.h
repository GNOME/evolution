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


#ifndef __EM_UTILS_H__
#define __EM_UTILS_H__

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

struct _GtkWidget;
struct _GtkWindow;
struct _CamelFolder;
struct _CamelInternetAddress;
struct _CamelStream;
struct _CamelMimeMessage;
struct _CamelMimePart;
struct _GtkSelectionData;
struct _GtkAdjustment;
struct _EMsgComposer;

gboolean em_utils_prompt_user(struct _GtkWindow *parent, const char *promptkey, const char *tag, const char *arg0, ...);

GPtrArray *em_utils_uids_copy (GPtrArray *uids);
void em_utils_uids_free (GPtrArray *uids);

gboolean em_utils_configure_account (struct _GtkWidget *parent);
gboolean em_utils_check_user_can_send_mail (struct _GtkWidget *parent);

void em_utils_edit_filters (struct _GtkWidget *parent);
void em_utils_edit_vfolders (struct _GtkWidget *parent);

void em_utils_composer_send_cb(struct _EMsgComposer *composer, gpointer user_data);
void em_utils_composer_save_draft_cb(struct _EMsgComposer *composer, int quit, gpointer user_data);

void em_utils_compose_new_message (void);

/* FIXME: mailto?  url?  should make up its mind what its called.  imho use 'uri' */
void em_utils_compose_new_message_with_mailto (const char *url);
void em_utils_post_to_folder (struct _CamelFolder *folder);
void em_utils_post_to_url (const char *url);

void em_utils_edit_message (struct _CamelMimeMessage *message);
void em_utils_edit_messages (struct _CamelFolder *folder, GPtrArray *uids);

void em_utils_forward_attached (struct _CamelFolder *folder, GPtrArray *uids);
void em_utils_forward_inline (struct _CamelFolder *folder, GPtrArray *uids);
void em_utils_forward_quoted (struct _CamelFolder *folder, GPtrArray *uids);

void em_utils_forward_message (struct _CamelMimeMessage *msg);
void em_utils_forward_messages (struct _CamelFolder *folder, GPtrArray *uids);

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

void em_utils_save_part(struct _GtkWidget *parent, const char *prompt, struct _CamelMimePart *part);
gboolean em_utils_save_part_to_file(struct _GtkWidget *parent, const char *filename, struct _CamelMimePart *part);
void em_utils_save_messages (struct _GtkWidget *parent, struct _CamelFolder *folder, GPtrArray *uids);

void em_utils_add_address(struct _GtkWidget *parent, const char *email);

void em_utils_flag_for_followup (struct _GtkWidget *parent, struct _CamelFolder *folder, GPtrArray *uids);
void em_utils_flag_for_followup_clear (struct _GtkWidget *parent, struct _CamelFolder *folder, GPtrArray *uids);
void em_utils_flag_for_followup_completed (struct _GtkWidget *parent, struct _CamelFolder *folder, GPtrArray *uids);

/* This stuff that follows probably doesn't belong here, then again, the stuff above probably belongs elsewhere */

void em_utils_selection_set_mailbox(struct _GtkSelectionData *data, struct _CamelFolder *folder, GPtrArray *uids);
void em_utils_selection_get_mailbox(struct _GtkSelectionData *data, struct _CamelFolder *folder);
/* FIXME: be nice if these also worked on struct _CamelFolder's, no easy way to get uri from folder yet tho */
void em_utils_selection_set_uidlist(struct _GtkSelectionData *data, const char *uri, GPtrArray *uids);
int  em_utils_selection_get_uidlist(struct _GtkSelectionData *data, char **uri, GPtrArray **uidsp);
void em_utils_selection_set_urilist(struct _GtkSelectionData *data, struct _CamelFolder *folder, GPtrArray *uids);

char *em_utils_temp_save_part(struct _GtkWidget *parent, struct _CamelMimePart *part);

gboolean em_utils_folder_is_drafts(struct _CamelFolder *folder, const char *uri);
gboolean em_utils_folder_is_sent(struct _CamelFolder *folder, const char *uri);
gboolean em_utils_folder_is_outbox(struct _CamelFolder *folder, const char *uri);

void em_utils_adjustment_page(struct _GtkAdjustment *adj, gboolean down);

char *em_utils_get_proxy_uri(void);

/* FIXME: should this have an override charset? */
char *em_utils_part_to_html(struct _CamelMimePart *part);
char *em_utils_message_to_html(struct _CamelMimeMessage *msg, const char *credits, guint32 flags);

void em_utils_expunge_folder (struct _GtkWidget *parent, struct _CamelFolder *folder);
void em_utils_empty_trash (struct _GtkWidget *parent);

/* returns the folder name portion of an URI */
char *em_utils_folder_name_from_uri (const char *uri);

/* internal/camel uri translation */
char *em_uri_from_camel (const char *curi);
char *em_uri_to_camel (const char *euri);

/* is this address in the addressbook?  caches results */
gboolean em_utils_in_addressbook(struct _CamelInternetAddress *addr);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __EM_UTILS_H__ */
