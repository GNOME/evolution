/*
 * e-mail-session.h
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

#if !defined (__LIBEMAIL_ENGINE_H_INSIDE__) && !defined (LIBEMAIL_ENGINE_COMPILATION)
#error "Only <libemail-engine/libemail-engine.h> should be included directly."
#endif

#ifndef E_MAIL_SESSION_H
#define E_MAIL_SESSION_H

#include <camel/camel.h>
#include <libedataserver/libedataserver.h>
#include <libemail-engine/e-mail-engine-enums.h>
#include <libemail-engine/e-mail-junk-filter.h>
#include <libemail-engine/em-vfolder-context.h>
#include <libemail-engine/mail-folder-cache.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_SESSION \
	(e_mail_session_get_type ())
#define E_MAIL_SESSION(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_SESSION, EMailSession))
#define E_MAIL_SESSION_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_SESSION, EMailSessionClass))
#define E_IS_MAIL_SESSION(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_SESSION))
#define E_IS_MAIL_SESSION_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_SESSION))
#define E_MAIL_SESSION_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_SESSION, EMailSessionClass))

/* Built-in CamelServices */
#define E_MAIL_SESSION_LOCAL_UID   "local"	/* "On This Computer" */
#define E_MAIL_SESSION_VFOLDER_UID "vfolder"	/* "Search Folders" */

G_BEGIN_DECLS

typedef struct _EMailSession EMailSession;
typedef struct _EMailSessionClass EMailSessionClass;
typedef struct _EMailSessionPrivate EMailSessionPrivate;

struct _EMailSession {
	CamelSession parent;
	EMailSessionPrivate *priv;
};

struct _EMailSessionClass {
	CamelSessionClass parent_class;

	EMVFolderContext *
			(*create_vfolder_context)
						(EMailSession *session);
	void		(*flush_outbox)		(EMailSession *session);
	void		(*refresh_service)	(EMailSession *session,
						 CamelService *service);
	void		(*store_added)		(EMailSession *session,
						 CamelStore *store);
	void		(*store_removed)	(EMailSession *session,
						 CamelStore *store);
	void		(*allow_auth_prompt)	(EMailSession *session,
						 ESource *source);
	gchar *		(*get_recipient_certificate)
						(EMailSession *session,
						 guint flags, /* bit-or of CamelRecipientCertificateFlags */
						 const gchar *email_address);
	void		(*archive_folder_changed)
						(EMailSession *session,
						 const gchar *service_uid,
						 const gchar *old_folder_uri,
						 const gchar *new_folder_uri);
	void		(*connect_store)	(EMailSession *session,
						 CamelStore *store);

	/* Padding for future expansion */
	gpointer reserved[8];
};

GType		e_mail_session_get_type		(void);
EMailSession *	e_mail_session_new		(ESourceRegistry *registry);
ESourceRegistry *
		e_mail_session_get_registry	(EMailSession *session);
MailFolderCache *
		e_mail_session_get_folder_cache	(EMailSession *session);
CamelStore *	e_mail_session_get_local_store	(EMailSession *session);
CamelStore *	e_mail_session_get_vfolder_store
						(EMailSession *session);
CamelFolder *	e_mail_session_get_local_folder	(EMailSession *session,
						 EMailLocalFolder type);
const gchar *	e_mail_session_get_local_folder_uri
						(EMailSession *session,
						 EMailLocalFolder type);
GList *		e_mail_session_get_available_junk_filters
						(EMailSession *session);
EMailJunkFilter *
		e_mail_session_get_junk_filter_by_name
						(EMailSession *session,
						 const gchar *filter_name);
CamelFolder *	e_mail_session_get_inbox_sync	(EMailSession *session,
						 const gchar *service_uid,
						 GCancellable *cancellable,
						 GError **error);
void		e_mail_session_get_inbox	(EMailSession *session,
						 const gchar *service_uid,
						 gint io_priority,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
CamelFolder *	e_mail_session_get_inbox_finish	(EMailSession *session,
						 GAsyncResult *result,
						 GError **error);
CamelFolder *	e_mail_session_get_trash_sync	(EMailSession *session,
						 const gchar *service_uid,
						 GCancellable *cancellable,
						 GError **error);
void		e_mail_session_get_trash	(EMailSession *session,
						 const gchar *service_uid,
						 gint io_priority,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
CamelFolder *	e_mail_session_get_trash_finish	(EMailSession *session,
						 GAsyncResult *result,
						 GError **error);
CamelFolder *	e_mail_session_uri_to_folder_sync
						(EMailSession *session,
						 const gchar *folder_uri,
						 CamelStoreGetFolderFlags flags,
						 GCancellable *cancellable,
						 GError **error);
void		e_mail_session_uri_to_folder	(EMailSession *session,
						 const gchar *folder_uri,
						 CamelStoreGetFolderFlags flags,
						 gint io_priority,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
CamelFolder *	e_mail_session_uri_to_folder_finish
						(EMailSession *session,
						 GAsyncResult *result,
						 GError **error);
EMVFolderContext *
		e_mail_session_create_vfolder_context
						(EMailSession *session);

void		e_mail_session_flush_outbox	(EMailSession *session);
void		e_mail_session_schedule_outbox_flush
						(EMailSession *session,
						 gint delay_minutes);
void		e_mail_session_cancel_scheduled_outbox_flush
						(EMailSession *session);
gboolean	e_mail_session_mark_service_used_sync
						(EMailSession *session,
						 CamelService *service,
						 GCancellable *cancellable);
void		e_mail_session_unmark_service_used
						(EMailSession *session,
						 CamelService *service);
void		e_mail_session_emit_allow_auth_prompt
						(EMailSession *session,
						 ESource *source);
gboolean	e_mail_session_is_archive_folder
						(EMailSession *session,
						 const gchar *folder_uri);
void		e_mail_session_emit_connect_store
						(EMailSession *session,
						 CamelStore *store);

/* Useful GBinding transform functions */
gboolean	e_binding_transform_service_to_source
						(GBinding *binding,
						 const GValue *source_value,
						 GValue *target_value,
						 gpointer session);
gboolean	e_binding_transform_source_to_service
						(GBinding *binding,
						 const GValue *source_value,
						 GValue *target_value,
						 gpointer session);

/*** Legacy API ***/

void		mail_session_flush_filter_log	(EMailSession *session);
const gchar *	mail_session_get_data_dir	(void);
const gchar *	mail_session_get_cache_dir	(void);
const gchar *	mail_session_get_config_dir	(void);

G_END_DECLS

#endif /* E_MAIL_SESSION_H */
