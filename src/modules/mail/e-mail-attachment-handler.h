/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_MAIL_ATTACHMENT_HANDLER_H
#define E_MAIL_ATTACHMENT_HANDLER_H

#include <e-util/e-util.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_ATTACHMENT_HANDLER \
	(e_mail_attachment_handler_get_type ())
#define E_MAIL_ATTACHMENT_HANDLER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_ATTACHMENT_HANDLER, EMailAttachmentHandler))
#define E_MAIL_ATTACHMENT_HANDLER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_ATTACHMENT_HANDLER, EMailAttachmentHandlerClass))
#define E_IS_MAIL_ATTACHMENT_HANDLER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_ATTACHMENT_HANDLER))
#define E_IS_MAIL_ATTACHMENT_HANDLER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_ATTACHMENT_HANDLER))
#define E_MAIL_ATTACHMENT_HANDLER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_ATTACHMENT_HANDLER, EMailAttachmentHandlerClass))

G_BEGIN_DECLS

typedef struct _EMailAttachmentHandler EMailAttachmentHandler;
typedef struct _EMailAttachmentHandlerClass EMailAttachmentHandlerClass;
typedef struct _EMailAttachmentHandlerPrivate EMailAttachmentHandlerPrivate;

struct _EMailAttachmentHandler {
	EAttachmentHandler parent;
	EMailAttachmentHandlerPrivate *priv;
};

struct _EMailAttachmentHandlerClass {
	EAttachmentHandlerClass parent_class;
};

GType		e_mail_attachment_handler_get_type	(void);
void		e_mail_attachment_handler_type_register	(GTypeModule *type_module);

G_END_DECLS

#endif /* E_MAIL_ATTACHMENT_HANDLER_H */
