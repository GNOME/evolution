/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_ATTACHMENT_HANDLER_IMAGE_H
#define E_ATTACHMENT_HANDLER_IMAGE_H

#include <e-util/e-attachment-handler.h>

/* Standard GObject macros */
#define E_TYPE_ATTACHMENT_HANDLER_IMAGE \
	(e_attachment_handler_image_get_type ())
#define E_ATTACHMENT_HANDLER_IMAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_ATTACHMENT_HANDLER_IMAGE, EAttachmentHandlerImage))
#define E_ATTACHMENT_HANDLER_IMAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_ATTACHMENT_HANDLER_IMAGE, EAttachmentHandlerImageClass))
#define E_IS_ATTACHMENT_HANDLER_IMAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_ATTACHMENT_HANDLER_IMAGE))
#define E_IS_ATTACHMENT_HANDLER_IMAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_ATTACHMENT_HANDLER_IMAGE))
#define E_ATTACHMENT_HANDLER_IMAGE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_ATTACHMENT_HANDLER_IMAGE, EAttachmentHandlerImageClass))

G_BEGIN_DECLS

typedef struct _EAttachmentHandlerImage EAttachmentHandlerImage;
typedef struct _EAttachmentHandlerImageClass EAttachmentHandlerImageClass;
typedef struct _EAttachmentHandlerImagePrivate EAttachmentHandlerImagePrivate;

struct _EAttachmentHandlerImage {
	EAttachmentHandler parent;
	EAttachmentHandlerImagePrivate *priv;
};

struct _EAttachmentHandlerImageClass {
	EAttachmentHandlerClass parent_class;
};

GType		e_attachment_handler_image_get_type	(void) G_GNUC_CONST;

G_END_DECLS

#endif /* E_ATTACHMENT_HANDLER_IMAGE_H */
