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
#include <camel/camel.h>
#include <libedataserver/e-proxy.h>

#include <mail/e-mail-reader.h>
#include <mail/e-mail-session.h>
#include <mail/em-folder-tree.h>

G_BEGIN_DECLS

struct _EMFormat;

gboolean em_utils_ask_open_many (GtkWindow *parent, gint how_many);
gboolean em_utils_prompt_user (GtkWindow *parent, const gchar *promptkey, const gchar *tag, ...);

GPtrArray *em_utils_uids_copy (GPtrArray *uids);
void em_utils_uids_free (GPtrArray *uids);

void em_utils_edit_filters (GtkWidget *parent, EMailBackend *backend);
void em_filename_make_safe (gchar *string);
void em_utils_edit_vfolders (GtkWidget *parent);

void em_utils_flag_for_followup (EMailReader *reader, CamelFolder *folder, GPtrArray *uids);
void em_utils_flag_for_followup_clear (GtkWindow *parent, CamelFolder *folder, GPtrArray *uids);
void em_utils_flag_for_followup_completed (GtkWindow *parent, CamelFolder *folder, GPtrArray *uids);

/* This stuff that follows probably doesn't belong here, then again, the stuff above probably belongs elsewhere */

void em_utils_selection_set_mailbox (GtkSelectionData *data, CamelFolder *folder, GPtrArray *uids);
void em_utils_selection_get_mailbox (GtkSelectionData *data, CamelFolder *folder);
void em_utils_selection_get_message (GtkSelectionData *data, CamelFolder *folder);
void em_utils_selection_set_uidlist (GtkSelectionData *data, CamelFolder *folder, GPtrArray *uids);
void em_utils_selection_get_uidlist (GtkSelectionData *data, EMailSession *session, CamelFolder *dest, gint move, GCancellable *cancellable, GError **error);
void em_utils_selection_set_urilist (GtkSelectionData *data, CamelFolder *folder, GPtrArray *uids);
void em_utils_selection_get_urilist (GtkSelectionData *data, CamelFolder *folder);

gboolean	em_utils_folder_is_drafts	(CamelFolder *folder);
gboolean	em_utils_folder_is_templates	(CamelFolder *folder);
gboolean	em_utils_folder_is_sent		(CamelFolder *folder);
gboolean	em_utils_folder_is_outbox	(CamelFolder *folder);

EProxy *	em_utils_get_proxy		(void);

/* FIXME: should this have an override charset? */
gchar *em_utils_message_to_html (CamelMimeMessage *msg, const gchar *credits, guint32 flags, struct _EMFormat *source, const gchar *append, guint32 *validity_found);

void em_utils_expunge_folder (GtkWidget *parent, EMailBackend *backend, CamelFolder *folder);
void em_utils_empty_trash (GtkWidget *parent, EMailBackend *backend);

/* is this address in the addressbook?  caches results */
gboolean em_utils_in_addressbook (CamelInternetAddress *addr, gboolean local_only);
CamelMimePart *em_utils_contact_photo (CamelInternetAddress *addr, gboolean local);

/* clears flag 'get_password_canceled' at every known accounts, so if needed, get_password will show dialog */
void em_utils_clear_get_password_canceled_accounts_flag (void);

/* Unescapes &amp; back to a real & in URIs */
gchar *em_utils_url_unescape_amp (const gchar *url);

GHashTable * em_utils_generate_account_hash (void);
struct _EAccount *em_utils_guess_account (CamelMimeMessage *message, CamelFolder *folder);
struct _EAccount *em_utils_guess_account_with_recipients (CamelMimeMessage *message, CamelFolder *folder);

void emu_remove_from_mail_cache (const GSList *addresses);
void emu_remove_from_mail_cache_1 (const gchar *address);
void emu_free_mail_cache (void);

void emu_restore_folder_tree_state (EMFolderTree *folder_tree);

gboolean em_utils_is_local_delivery_mbox_file (CamelURL *url);

gboolean em_utils_connect_service_sync (CamelService *service, GCancellable *cancellable, GError **error);
gboolean em_utils_disconnect_service_sync (CamelService *service, gboolean clean, GCancellable *cancellable, GError **error);

void	em_utils_save_accounts_sort_order (EMailBackend *backend, const GSList *account_uids);
GSList *em_utils_load_accounts_sort_order (EMailBackend *backend);
guint	em_utils_get_account_sort_order   (EMailBackend *backend, const gchar *account_uid);

G_END_DECLS

#endif /* __EM_UTILS_H__ */
