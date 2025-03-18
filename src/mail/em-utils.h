/*
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

#ifndef __EM_UTILS_H__
#define __EM_UTILS_H__

#include <gtk/gtk.h>
#include <sys/types.h>
#include <camel/camel.h>

#include <libemail-engine/libemail-engine.h>

#include <mail/e-mail-browser.h>
#include <mail/e-mail-reader.h>
#include <mail/em-folder-tree.h>

G_BEGIN_DECLS

struct _EMailPartList;

gboolean em_utils_ask_open_many (GtkWindow *parent, gint how_many);

void		em_utils_edit_filters		(EMailSession *session,
						 EAlertSink *alert_sink,
						 GtkWindow *parent_window);
void em_utils_edit_vfolders (GtkWidget *parent);

void em_utils_flag_for_followup (EMailReader *reader, CamelFolder *folder, GPtrArray *uids);
void em_utils_flag_for_followup_clear (GtkWindow *parent, CamelFolder *folder, GPtrArray *uids);
void em_utils_flag_for_followup_completed (GtkWindow *parent, CamelFolder *folder, GPtrArray *uids);

/* This stuff that follows probably doesn't belong here, then again, the stuff above probably belongs elsewhere */

void em_utils_selection_set_mailbox (GtkSelectionData *data, CamelFolder *folder, GPtrArray *uids);
void em_utils_selection_get_message (GtkSelectionData *data, CamelFolder *folder);
void em_utils_selection_set_uidlist (GtkSelectionData *data, CamelFolder *folder, GPtrArray *uids);
void em_utils_selection_get_uidlist (GtkSelectionData *data, EMailSession *session, CamelFolder *dest, gint move, GCancellable *cancellable, GError **error);
void em_utils_selection_set_urilist (GdkDragContext *context, GtkSelectionData *data, CamelFolder *folder, GPtrArray *uids);
void em_utils_selection_get_urilist (GtkSelectionData *data, CamelFolder *folder);

/* Return TRUE to continue, FALSE to stop further processing */
typedef gboolean (* EMUtilsUIDListFunc)		(CamelFolder *folder,
						 const GPtrArray *uids,
						 gpointer user_data,
						 GCancellable *cancellable,
						 GError **error);

void	em_utils_selection_uidlist_foreach_sync	(GtkSelectionData *selection_data,
						 EMailSession *session,
						 EMUtilsUIDListFunc func,
						 gpointer user_data,
						 GCancellable *cancellable,
						 GError **error);

/* FIXME: should this have an override charset? */
gchar *		em_utils_message_to_html_ex	(CamelSession *session,
						 CamelMimeMessage *message,
						 const gchar *credits,
						 guint32 flags,
						 EMailPartList *part_list,
						 const gchar *prepend,
						 const gchar *append,
						 EMailPartValidityFlags *validity_found,
						 EMailPartList **out_part_list);

void		em_utils_empty_trash		(GtkWidget *parent,
						 EMailSession *session);

void emu_restore_folder_tree_state (EMFolderTree *folder_tree);

gboolean	em_utils_is_re_in_subject	(const gchar *subject,
						 gint *skip_len,
						 const gchar * const *use_prefixes_strv,
						 const gchar * const *use_separators_strv);

gchar *		em_utils_get_archive_folder_uri_from_folder
						(CamelFolder *folder,
						 EMailBackend *mail_backend,
						 GPtrArray *uids,
						 gboolean deep_uids_check);

gboolean	em_utils_process_autoarchive_sync
						(EMailBackend *mail_backend,
						 CamelFolder *folder,
						 const gchar *folder_uri,
						 GCancellable *cancellable,
						 GError **error);

gchar *		em_utils_build_export_basename	(CamelFolder *folder,
						 const gchar *uid,
						 const gchar *extension);
gchar *		em_utils_account_path_to_folder_uri
						(CamelSession *session,
						 const gchar *account_path); /* On This Computer/Inbox/Subfolder... */
EMailBrowser *	em_utils_find_message_window	(EMailFormatterMode display_mode,
						 CamelFolder *folder,
						 const gchar *message_uid);
gboolean	em_utils_import_pgp_key		(GtkWindow *parent,
						 CamelSession *session,
						 const guint8 *keydata,
						 gsize keydata_size,
						 GError **error);
void		em_utils_print_part_list	(EMailPartList *part_list,
						 EMailDisplay *mail_display,
						 GtkPrintOperationAction print_action,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
gboolean	em_utils_print_part_list_finish	(GObject *source_object,
						 GAsyncResult *result,
						 GError **error);
gchar *		em_utils_select_folder_for_copy_move_message
						(GtkWindow *parent,
						 gboolean is_move,
						 CamelFolder *folder); /* optional */

G_END_DECLS

#endif /* __EM_UTILS_H__ */
