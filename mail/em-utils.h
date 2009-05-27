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

#include <gtk/gtk.h>
#include <sys/types.h>
#include <camel/camel-exception.h>
#include <camel/camel-folder.h>
#include <camel/camel-internet-address.h>
#include <camel/camel-mime-message.h>
#include <camel/camel-mime-part.h>
#include <camel/camel-stream.h>

G_BEGIN_DECLS

struct _EMFormat;

gboolean em_utils_prompt_user(GtkWindow *parent, const gchar *promptkey, const gchar *tag, const gchar *arg0, ...);

GPtrArray *em_utils_uids_copy (GPtrArray *uids);
void em_utils_uids_free (GPtrArray *uids);

gboolean em_utils_configure_account (GtkWindow *parent);
gboolean em_utils_check_user_can_send_mail (GtkWindow *parent);

void em_utils_edit_filters (GtkWidget *parent);
void em_filename_make_safe (gchar *string);
void em_utils_edit_vfolders (GtkWidget *parent);

void em_utils_save_part(GtkWindow *parent, const gchar *prompt, CamelMimePart *part);
gboolean em_utils_save_part_to_file(GtkWindow *parent, const gchar *filename, CamelMimePart *part);
void em_utils_save_messages (GtkWindow *parent, CamelFolder *folder, GPtrArray *uids);

void em_utils_add_address(GtkWindow *parent, const gchar *email);
void em_utils_add_vcard(GtkWindow *parent, const gchar *vcard);

void em_utils_flag_for_followup (GtkWindow *parent, CamelFolder *folder, GPtrArray *uids);
void em_utils_flag_for_followup_clear (GtkWindow *parent, CamelFolder *folder, GPtrArray *uids);
void em_utils_flag_for_followup_completed (GtkWindow *parent, CamelFolder *folder, GPtrArray *uids);

/* This stuff that follows probably doesn't belong here, then again, the stuff above probably belongs elsewhere */

void em_utils_selection_set_mailbox(GtkSelectionData *data, CamelFolder *folder, GPtrArray *uids);
void em_utils_selection_get_mailbox(GtkSelectionData *data, CamelFolder *folder);
void em_utils_selection_get_message(GtkSelectionData *data, CamelFolder *folder);
/* FIXME: be nice if these also worked on CamelFolder's, no easy way to get uri from folder yet tho */
void em_utils_selection_set_uidlist(GtkSelectionData *data, const gchar *uri, GPtrArray *uids);
void em_utils_selection_get_uidlist(GtkSelectionData *data, CamelFolder *dest, gint move, CamelException *ex);
void em_utils_selection_set_urilist(GtkSelectionData *data, CamelFolder *folder, GPtrArray *uids);
void em_utils_selection_get_urilist(GtkSelectionData *data, CamelFolder *folder);

gchar *em_utils_temp_save_part(GtkWidget *parent, CamelMimePart *part, gboolean mode);
void em_utils_save_parts (GtkWindow *parent, const gchar *prompt, GSList * parts);

gboolean em_utils_folder_is_drafts(CamelFolder *folder, const gchar *uri);
gboolean em_utils_folder_is_templates(CamelFolder *folder, const gchar *uri);
gboolean em_utils_folder_is_sent(CamelFolder *folder, const gchar *uri);
gboolean em_utils_folder_is_outbox(CamelFolder *folder, const gchar *uri);

void em_utils_adjustment_page(GtkAdjustment *adj, gboolean down);

gchar *em_utils_get_proxy_uri (const gchar *uri);

/* FIXME: should this have an override charset? */
gchar *em_utils_message_to_html(CamelMimeMessage *msg, const gchar *credits, guint32 flags, ssize_t *len, struct _EMFormat *source, const gchar *append);

void em_utils_expunge_folder (GtkWidget *parent, CamelFolder *folder);
void em_utils_empty_trash (GtkWidget *parent);

/* returns the folder name portion of an URI */
gchar *em_utils_folder_name_from_uri (const gchar *uri);

/* internal/camel uri translation */
gchar *em_uri_from_camel (const gchar *curi);
gchar *em_uri_to_camel (const gchar *euri);

/* Run errors silently on the status bar */
void em_utils_show_error_silent (GtkWidget *widget);
void em_utils_show_info_silent (GtkWidget *widget);

/* is this address in the addressbook?  caches results */
gboolean em_utils_in_addressbook (CamelInternetAddress *addr, gboolean local_only);
CamelMimePart *em_utils_contact_photo (CamelInternetAddress *addr, gboolean local);

/* clears flag 'get_password_canceled' at every known accounts, so if needed, get_password will show dialog */
void em_utils_clear_get_password_canceled_accounts_flag (void);

/* Unescapes &amp; back to a real & in URIs */
gchar *em_utils_url_unescape_amp (const gchar *url);

G_END_DECLS

#endif /* __EM_UTILS_H__ */
