/*
 * e-mail-session-utils.h
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
 */

#if !defined (__LIBEMAIL_ENGINE_H_INSIDE__) && !defined (LIBEMAIL_ENGINE_COMPILATION)
#error "Only <libemail-engine/libemail-engine.h> should be included directly."
#endif

#ifndef E_MAIL_SESSION_UTILS_H
#define E_MAIL_SESSION_UTILS_H

/* High-level operations with Evolution-specific policies. */

#include <libemail-engine/e-mail-session.h>

#define E_MAIL_ERROR (e_mail_error_quark ())

G_BEGIN_DECLS

typedef enum {
	E_MAIL_ERROR_POST_PROCESSING
} EMailError;

GQuark		e_mail_error_quark		(void) G_GNUC_CONST;
gboolean	e_mail_session_append_to_local_folder_sync
						(EMailSession *session,
						 EMailLocalFolder local_id,
						 CamelMimeMessage *message,
						 CamelMessageInfo *info,
						 gchar **appended_uid,
						 GCancellable *cancellable,
						 GError **error);
void		e_mail_session_append_to_local_folder
						(EMailSession *session,
						 EMailLocalFolder local_id,
						 CamelMimeMessage *message,
						 CamelMessageInfo *info,
						 gint io_priority,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
gboolean	e_mail_session_append_to_local_folder_finish
						(EMailSession *session,
						 GAsyncResult *result,
						 gchar **appended_uid,
						 GError **error);
gboolean	e_mail_session_handle_draft_headers_sync
						(EMailSession *session,
						 CamelMimeMessage *message,
						 GCancellable *cancellable,
						 GError **error);
void		e_mail_session_handle_draft_headers
						(EMailSession *session,
						 CamelMimeMessage *message,
						 gint io_priority,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
gboolean	e_mail_session_handle_draft_headers_finish
						(EMailSession *session,
						 GAsyncResult *result,
						 GError **error);
gboolean	e_mail_session_handle_source_headers_sync
						(EMailSession *session,
						 CamelMimeMessage *message,
						 GCancellable *cancellable,
						 GError **error);
void		e_mail_session_handle_source_headers
						(EMailSession *session,
						 CamelMimeMessage *message,
						 gint io_priority,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
gboolean	e_mail_session_handle_source_headers_finish
						(EMailSession *session,
						 GAsyncResult *result,
						 GError **error);
void		e_mail_session_send_to		(EMailSession *session,
						 CamelMimeMessage *message,
						 gint io_priority,
						 GCancellable *cancellable,
						 CamelFilterGetFolderFunc get_folder_func,
						 gpointer get_folder_data,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
gboolean	e_mail_session_send_to_finish	(EMailSession *session,
						 GAsyncResult *result,
						 GError **error);
CamelFolder *	e_mail_session_get_fcc_for_message_sync
						(EMailSession *session,
						 CamelMimeMessage *message,
						 gboolean *out_use_sent_folder,
						 GCancellable *cancellable,
						 GError **error);
void		e_mail_session_get_fcc_for_message
						(EMailSession *session,
						 CamelMimeMessage *message,
						 gint io_priority,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
CamelFolder *	e_mail_session_get_fcc_for_message_finish
						(EMailSession *session,
						 GAsyncResult *result,
						 gboolean *out_use_sent_folder,
						 GError **error);
CamelService *	e_mail_session_ref_transport	(EMailSession *session,
						 const gchar *transport_uid);
CamelService *	e_mail_session_ref_default_transport
						(EMailSession *session);
CamelService *	e_mail_session_ref_transport_for_message
						(EMailSession *session,
						 CamelMimeMessage *message);

G_END_DECLS

#endif /* E_MAIL_SESSION_UTILS_H */
