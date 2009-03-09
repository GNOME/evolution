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
 *		Ettore Perazzoli <ettore@ximian.com>
 *		Srinivasa Ragavan <sragavan@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_ATTACHMENT_H
#define E_ATTACHMENT_H

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <glade/glade-xml.h>
#include <camel/camel-mime-part.h>
#include <camel/camel-exception.h>
#include <camel/camel-cipher-context.h>

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
	((obj), E_TYPE_ATTACHMENT))
#define E_ATTACHMENT_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_ATTACHMENT, EAttachmentClass))

G_BEGIN_DECLS

typedef struct _EAttachment EAttachment;
typedef struct _EAttachmentClass EAttachmentClass;
typedef struct _EAttachmentPrivate EAttachmentPrivate;

struct _EAttachment {
	GObject parent;

	gboolean guessed_type;
	gulong size;

	GCancellable *cancellable;

	gboolean is_available_local;
	int percentage;
	int index;
	char *store_uri;

	/* Status of signed/encrypted attachments */
	camel_cipher_validity_sign_t sign;
	camel_cipher_validity_encrypt_t encrypt;

	EAttachmentPrivate *priv;
};

struct _EAttachmentClass {
	GObjectClass parent_class;

	void		(*changed)		(EAttachment *attachment);
	void		(*update)		(EAttachment *attachment,
						 gchar *message);
};

GType		e_attachment_get_type		(void);
EAttachment *	e_attachment_new		(const gchar *filename,
						 const gchar *disposition,
						 CamelException *ex);
EAttachment *	e_attachment_new_remote_file	(GtkWindow *error_dlg_parent,
						 const gchar *url,
						 const gchar *disposition,
						 const gchar *path,
						 CamelException *ex);
void		e_attachment_build_remote_file	(const gchar *filename,
						 EAttachment *attachment,
						 CamelException *ex);
EAttachment *	e_attachment_new_from_mime_part	(CamelMimePart *part);
void		e_attachment_edit		(EAttachment *attachment,
						 GtkWindow *parent);
const gchar *	e_attachment_get_description	(EAttachment *attachment);
void		e_attachment_set_description	(EAttachment *attachment,
						 const gchar *description);
const gchar *	e_attachment_get_disposition	(EAttachment *attachment);
void		e_attachment_set_disposition	(EAttachment *attachment,
						 const gchar *disposition);
const gchar *	e_attachment_get_filename	(EAttachment *attachment);
void		e_attachment_set_filename	(EAttachment *attachment,
						 const gchar *filename);
CamelMimePart *	e_attachment_get_mime_part	(EAttachment *attachment);
const gchar *	e_attachment_get_mime_type	(EAttachment *attachment);
GdkPixbuf *	e_attachment_get_thumbnail	(EAttachment *attachment);
void		e_attachment_set_thumbnail	(EAttachment *attachment,
						 GdkPixbuf *pixbuf);
gboolean	e_attachment_is_image		(EAttachment *attachment);
gboolean	e_attachment_is_inline		(EAttachment *attachment);

G_END_DECLS

#endif /* E_ATTACHMENT_H */
