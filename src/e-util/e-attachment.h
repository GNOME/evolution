/*
 * e-attachment.h
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_ATTACHMENT_H
#define E_ATTACHMENT_H

#include <gtk/gtk.h>
#include <camel/camel.h>

/* Standard GObject macros */
#define E_TYPE_ATTACHMENT \
	(e_attachment_get_type ())
#define E_ATTACHMENT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_ATTACHMENT, EAttachment))
#define E_ATTACHMENT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_ATTACHMENT, EAttachmentClass))
#define E_IS_ATTACHMENT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_ATTACHMENT))
#define E_IS_ATTACHMENT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_ATTACHMENT))
#define E_ATTACHMENT_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_ATTACHMENT, EAttachmentClass))

G_BEGIN_DECLS

typedef struct _EAttachment EAttachment;
typedef struct _EAttachmentClass EAttachmentClass;
typedef struct _EAttachmentPrivate EAttachmentPrivate;

struct _EAttachment {
	GObject parent;
	EAttachmentPrivate *priv;
};

struct _EAttachmentClass {
	GObjectClass parent_class;

	/* Signals */
	void	(*update_file_info)		(EAttachment *attachment,
						 const gchar *caption,
						 const gchar *content_type,
						 const gchar *description,
						 gint64 size);
	void	(*update_icon)			(EAttachment *attachment,
						 GIcon *icon);
	void	(*update_progress)		(EAttachment *attachment,
						 gboolean loading,
						 gboolean saving,
						 gint percent);
	void	(*load_failed)			(EAttachment *attachment);
};

GType		e_attachment_get_type		(void) G_GNUC_CONST;
EAttachment *	e_attachment_new		(void);
EAttachment *	e_attachment_new_for_path	(const gchar *path);
EAttachment *	e_attachment_new_for_uri	(const gchar *uri);
EAttachment *	e_attachment_new_for_message	(CamelMimeMessage *message);
void		e_attachment_add_to_multipart	(EAttachment *attachment,
						 CamelMultipart *multipart,
						 const gchar *default_charset);
void		e_attachment_cancel		(EAttachment *attachment);
gboolean	e_attachment_is_mail_note	(EAttachment *attachment);
gboolean	e_attachment_is_uri		(EAttachment *attachment);
gboolean	e_attachment_get_can_show	(EAttachment *attachment);
void		e_attachment_set_can_show	(EAttachment *attachment,
						 gboolean can_show);
const gchar *	e_attachment_get_disposition	(EAttachment *attachment);
gchar *		e_attachment_dup_disposition	(EAttachment *attachment);
void		e_attachment_set_disposition	(EAttachment *attachment,
						 const gchar *disposition);
GFile *		e_attachment_ref_file		(EAttachment *attachment);
void		e_attachment_set_file		(EAttachment *attachment,
						 GFile *file);
GFileInfo *	e_attachment_ref_file_info	(EAttachment *attachment);
void		e_attachment_set_file_info	(EAttachment *attachment,
						 GFileInfo *file_info);
gchar *		e_attachment_dup_mime_type	(EAttachment *attachment);
GIcon *		e_attachment_ref_icon		(EAttachment *attachment);
gboolean	e_attachment_get_loading	(EAttachment *attachment);
CamelMimePart *	e_attachment_ref_mime_part	(EAttachment *attachment);
void		e_attachment_set_mime_part	(EAttachment *attachment,
						 CamelMimePart *mime_part);
gint		e_attachment_get_percent	(EAttachment *attachment);
gboolean	e_attachment_get_saving		(EAttachment *attachment);
gboolean	e_attachment_get_initially_shown(EAttachment *attachment);
void		e_attachment_set_initially_shown(EAttachment *attachment,
						 gboolean initially_shown);
gboolean	e_attachment_get_save_self	(EAttachment *attachment);
void		e_attachment_set_save_self	(EAttachment *attachment,
						 gboolean save_self);
gboolean	e_attachment_get_save_extracted	(EAttachment *attachment);
void		e_attachment_set_save_extracted	(EAttachment *attachment,
						 gboolean save_extracted);
CamelCipherValidityEncrypt
		e_attachment_get_encrypted	(EAttachment *attachment);
void		e_attachment_set_encrypted	(EAttachment *attachment,
						 CamelCipherValidityEncrypt encrypted);
CamelCipherValiditySign
		e_attachment_get_signed		(EAttachment *attachment);
void		e_attachment_set_signed		(EAttachment *attachment,
						 CamelCipherValiditySign signed_);
gchar *		e_attachment_dup_description	(EAttachment *attachment);
gchar *		e_attachment_dup_thumbnail_path	(EAttachment *attachment);
gboolean	e_attachment_is_rfc822		(EAttachment *attachment);
GList *		e_attachment_list_apps		(EAttachment *attachment);
GAppInfo *	e_attachment_ref_default_app	(EAttachment *attachment);
void		e_attachment_update_store_columns
						(EAttachment *attachment);
gboolean	e_attachment_check_file_changed	(EAttachment *attachment,
						 gboolean *out_file_exists,
						 GCancellable *cancellable);
void		e_attachment_set_may_reload	(EAttachment *attachment,
						 gboolean may_reload);
gboolean	e_attachment_get_may_reload	(EAttachment *attachment);
void		e_attachment_set_is_possible	(EAttachment *attachment,
						 gboolean is_possible);
gboolean	e_attachment_get_is_possible	(EAttachment *attachment);

/* Asynchronous Operations */
void		e_attachment_load_async		(EAttachment *attachment,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
gboolean	e_attachment_load_finish	(EAttachment *attachment,
						 GAsyncResult *result,
						 GError **error);
gboolean	e_attachment_load		(EAttachment *attachment,
						 GError **error);
void		e_attachment_open_async		(EAttachment *attachment,
						 GAppInfo *app_info,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
gboolean	e_attachment_open_finish	(EAttachment *attachment,
						 GAsyncResult *result,
						 GError **error);
gboolean	e_attachment_open		(EAttachment *attachment,
						 GAppInfo *app_info,
						 GError **error);
void		e_attachment_save_async		(EAttachment *attachment,
						 GFile *destination,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
GFile *		e_attachment_save_finish	(EAttachment *attachment,
						 GAsyncResult *result,
						 GError **error);
gboolean	e_attachment_save		(EAttachment *attachment,
						 GFile *in_destination,
						 GFile **out_destination,
						 GError **error);

/* Handy GAsyncReadyCallback Functions */
void		e_attachment_load_handle_error	(EAttachment *attachment,
						 GAsyncResult *result,
						 GtkWindow *parent);
void		e_attachment_open_handle_error	(EAttachment *attachment,
						 GAsyncResult *result,
						 GtkWindow *parent);
void		e_attachment_save_handle_error	(EAttachment *attachment,
						 GAsyncResult *result,
						 GtkWindow *parent);

G_END_DECLS

#endif /* E_ATTACHMENT_H */
