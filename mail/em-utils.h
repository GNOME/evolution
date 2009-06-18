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
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __EM_UTILS_H__
#define __EM_UTILS_H__

#include <glib.h>

G_BEGIN_DECLS

#include <sys/types.h>

struct _GtkWidget;
struct _GtkWindow;
struct _CamelFolder;
struct _CamelInternetAddress;
struct _CamelStream;
struct _CamelMimeMessage;
struct _CamelMimePart;
struct _GtkSelectionData;
struct _GtkAdjustment;
struct _CamelException;
struct _EMFormat;

gboolean em_utils_prompt_user(struct _GtkWindow *parent, const gchar *promptkey, const gchar *tag, const gchar *arg0, ...);

GPtrArray *em_utils_uids_copy (GPtrArray *uids);
void em_utils_uids_free (GPtrArray *uids);

gboolean em_utils_configure_account (struct _GtkWidget *parent);
gboolean em_utils_check_user_can_send_mail (struct _GtkWidget *parent);

void em_utils_edit_filters (struct _GtkWidget *parent);
void em_filename_make_safe (gchar *string);
void em_utils_edit_vfolders (struct _GtkWidget *parent);

void em_utils_save_part(struct _GtkWidget *parent, const gchar *prompt, struct _CamelMimePart *part);
gboolean em_utils_save_part_to_file(struct _GtkWidget *parent, const gchar *filename, struct _CamelMimePart *part);
void em_utils_save_messages (struct _GtkWidget *parent, struct _CamelFolder *folder, GPtrArray *uids);

void em_utils_add_address(struct _GtkWidget *parent, const gchar *email);
void em_utils_add_vcard(struct _GtkWidget *parent, const gchar *vcard);

void em_utils_flag_for_followup (struct _GtkWidget *parent, struct _CamelFolder *folder, GPtrArray *uids);
void em_utils_flag_for_followup_clear (struct _GtkWidget *parent, struct _CamelFolder *folder, GPtrArray *uids);
void em_utils_flag_for_followup_completed (struct _GtkWidget *parent, struct _CamelFolder *folder, GPtrArray *uids);

/* This stuff that follows probably doesn't belong here, then again, the stuff above probably belongs elsewhere */

void em_utils_selection_set_mailbox(struct _GtkSelectionData *data, struct _CamelFolder *folder, GPtrArray *uids);
void em_utils_selection_get_mailbox(struct _GtkSelectionData *data, struct _CamelFolder *folder);
void em_utils_selection_get_message(struct _GtkSelectionData *data, struct _CamelFolder *folder);
/* FIXME: be nice if these also worked on struct _CamelFolder's, no easy way to get uri from folder yet tho */
void em_utils_selection_set_uidlist(struct _GtkSelectionData *data, const gchar *uri, GPtrArray *uids);
void em_utils_selection_get_uidlist(struct _GtkSelectionData *data, struct _CamelFolder *dest, gint move, struct _CamelException *ex);
void em_utils_selection_set_urilist(struct _GtkSelectionData *data, struct _CamelFolder *folder, GPtrArray *uids);
void em_utils_selection_get_urilist(struct _GtkSelectionData *data, struct _CamelFolder *folder);

gchar *em_utils_temp_save_part(struct _GtkWidget *parent, struct _CamelMimePart *part, gboolean mode);
void em_utils_save_parts (struct _GtkWidget *parent, const gchar *prompt, GSList * parts);

gboolean em_utils_folder_is_drafts(struct _CamelFolder *folder, const gchar *uri);
gboolean em_utils_folder_is_templates(struct _CamelFolder *folder, const gchar *uri);
gboolean em_utils_folder_is_sent(struct _CamelFolder *folder, const gchar *uri);
gboolean em_utils_folder_is_outbox(struct _CamelFolder *folder, const gchar *uri);

void em_utils_adjustment_page(struct _GtkAdjustment *adj, gboolean down);

gchar *em_utils_get_proxy_uri (const gchar *uri);

/* FIXME: should this have an override charset? */
gchar *em_utils_part_to_html(struct _CamelMimePart *part, gssize *len, struct _EMFormat *source);
gchar *em_utils_message_to_html(struct _CamelMimeMessage *msg, const gchar *credits, guint32 flags, gssize *len, struct _EMFormat *source, const gchar *append);

void em_utils_expunge_folder (struct _GtkWidget *parent, struct _CamelFolder *folder);
void em_utils_empty_trash (struct _GtkWidget *parent);

/* returns the folder name portion of an URI */
gchar *em_utils_folder_name_from_uri (const gchar *uri);

/* internal/camel uri translation */
gchar *em_uri_from_camel (const gchar *curi);
gchar *em_uri_to_camel (const gchar *euri);

/* Run errors silently on the status bar */
void em_utils_show_error_silent (struct _GtkWidget *widget);
void em_utils_show_info_silent (struct _GtkWidget *widget);

/* is this address in the addressbook?  caches results */
gboolean em_utils_in_addressbook (struct _CamelInternetAddress *addr, gboolean local_only);
struct _CamelMimePart *em_utils_contact_photo (struct _CamelInternetAddress *addr, gboolean local);

const gchar *em_utils_snoop_type(struct _CamelMimePart *part);

/* clears flag 'get_password_canceled' at every known accounts, so if needed, get_password will show dialog */
void em_utils_clear_get_password_canceled_accounts_flag (void);

G_END_DECLS

#endif /* __EM_UTILS_H__ */
