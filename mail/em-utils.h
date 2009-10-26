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

#include <mail/em-format.h>

G_BEGIN_DECLS

gboolean em_utils_prompt_user(GtkWindow *parent, const gchar *promptkey, const gchar *tag, const gchar *arg0, ...);

GPtrArray *em_utils_uids_copy (GPtrArray *uids);
void em_utils_uids_free (GPtrArray *uids);

gboolean em_utils_configure_account (GtkWidget *parent);
gboolean em_utils_check_user_can_send_mail (GtkWidget *parent);

void em_utils_edit_filters (GtkWidget *parent);
void em_filename_make_safe (gchar *string);
void em_utils_edit_vfolders (GtkWidget *parent);

void em_utils_save_part(GtkWidget *parent, const gchar *prompt, CamelMimePart *part);
gboolean em_utils_save_part_to_file(GtkWidget *parent, const gchar *filename, CamelMimePart *part);
void em_utils_save_messages (GtkWidget *parent, CamelFolder *folder, GPtrArray *uids);

void em_utils_add_address(GtkWidget *parent, const gchar *email);
void em_utils_add_vcard(GtkWidget *parent, const gchar *vcard);

void em_utils_flag_for_followup (GtkWidget *parent, CamelFolder *folder, GPtrArray *uids);
void em_utils_flag_for_followup_clear (GtkWidget *parent, CamelFolder *folder, GPtrArray *uids);
void em_utils_flag_for_followup_completed (GtkWidget *parent, CamelFolder *folder, GPtrArray *uids);

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
void em_utils_save_parts (GtkWidget *parent, const gchar *prompt, GSList * parts);

gboolean em_utils_folder_is_drafts(CamelFolder *folder, const gchar *uri);
gboolean em_utils_folder_is_templates(CamelFolder *folder, const gchar *uri);
gboolean em_utils_folder_is_sent(CamelFolder *folder, const gchar *uri);
gboolean em_utils_folder_is_outbox(CamelFolder *folder, const gchar *uri);

void em_utils_adjustment_page(GtkAdjustment *adj, gboolean down);

gchar *em_utils_get_proxy_uri (const gchar *uri);

/* FIXME: should this have an override charset? */
gchar *em_utils_part_to_html(CamelMimePart *part, gssize *len, EMFormat *source);
gchar *em_utils_message_to_html(CamelMimeMessage *msg, const gchar *credits, guint32 flags, gssize *len, EMFormat *source, const gchar *append);

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

const gchar *em_utils_snoop_type(CamelMimePart *part);

/* clears flag 'get_password_canceled' at every known accounts, so if needed, get_password will show dialog */
void em_utils_clear_get_password_canceled_accounts_flag (void);

void emu_remove_from_mail_cache (const GSList *addresses);
void emu_remove_from_mail_cache_1 (const gchar *address);
void emu_free_mail_cache (void);

G_END_DECLS

#endif /* __EM_UTILS_H__ */
