/*
 * e-attachment.h
 *
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_ATTACHMENT_H
#define E_ATTACHMENT_H

#include <gio/gio.h>
#include <camel/camel-mime-part.h>
#include <camel/camel-mime-message.h>
#include <camel/camel-multipart.h>
#include <widgets/misc/e-file-activity.h>

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
};

GType		e_attachment_get_type		(void);
EAttachment *	e_attachment_new		(void);
EAttachment *	e_attachment_new_for_path	(const gchar *path);
EAttachment *	e_attachment_new_for_uri	(const gchar *uri);
EAttachment *	e_attachment_new_for_message	(CamelMimeMessage *message);
void		e_attachment_add_to_multipart	(EAttachment *attachment,
						 CamelMultipart *multipart,
						 const gchar *default_charset);
const gchar *	e_attachment_get_disposition	(EAttachment *attachment);
void		e_attachment_set_disposition	(EAttachment *attachment,
						 const gchar *disposition);
GFile *		e_attachment_get_file		(EAttachment *attachment);
void		e_attachment_set_file		(EAttachment *attachment,
						 GFile *file);
GFileInfo *	e_attachment_get_file_info	(EAttachment *attachment);
CamelMimePart *	e_attachment_get_mime_part	(EAttachment *attachment);
void		e_attachment_set_mime_part	(EAttachment *attachment,
						 CamelMimePart *mime_part);
const gchar *	e_attachment_get_content_type	(EAttachment *attachment);
const gchar *	e_attachment_get_display_name	(EAttachment *attachment);
const gchar *	e_attachment_get_description	(EAttachment *attachment);
GIcon *		e_attachment_get_icon		(EAttachment *attachment);
const gchar *	e_attachment_get_thumbnail_path	(EAttachment *attachment);
guint64		e_attachment_get_size		(EAttachment *attachment);
gboolean	e_attachment_is_image		(EAttachment *attachment);
gboolean	e_attachment_is_rfc822		(EAttachment *attachment);
void		e_attachment_save_async		(EAttachment *attachment,
						 EFileActivity *file_activity,
						 GFile *destination);

#if 0
void		e_attachment_build_mime_part_async
						(EAttachment *attachment,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
CamelMimePart *	e_attachment_build_mime_part_finish
						(EAttachment *attachment,
						 GAsyncResult *result,
						 GError **error);
#endif

/* For use by EAttachmentStore only. */
void		_e_attachment_set_reference	(EAttachment *attachment,
						 GtkTreeRowReference *reference);

G_END_DECLS

#endif /* E_ATTACHMENT_H */
