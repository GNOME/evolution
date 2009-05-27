/*
 * e-attachment-handler-mail.h
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

#ifndef E_ATTACHMENT_HANDLER_MAIL_H
#define E_ATTACHMENT_HANDLER_MAIL_H

#include <widgets/misc/e-attachment-handler.h>

/* Standard GObject macros */
#define E_TYPE_ATTACHMENT_HANDLER_MAIL \
	(e_attachment_handler_mail_get_type ())
#define E_ATTACHMENT_HANDLER_MAIL(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_ATTACHMENT_HANDLER_MAIL, EAttachmentHandlerMail))
#define E_ATTACHMENT_HANDLER_MAIL_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_ATTACHMENT_HANDLER_MAIL, EAttachmentHandlerMailClass))
#define E_IS_ATTACHMENT_HANDLER_MAIL(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_ATTACHMENT_HANDLER_MAIL))
#define E_IS_ATTACHMENT_HANDLER_MAIL_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_ATTACHMENT_HANDLER_MAIL))
#define E_ATTACHMENT_HANDLER_MAIL_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_ATTACHMENT_HANDLER_MAIL, EAttachmentHandlerMailClass))

G_BEGIN_DECLS

typedef struct _EAttachmentHandlerMail EAttachmentHandlerMail;
typedef struct _EAttachmentHandlerMailClass EAttachmentHandlerMailClass;
typedef struct _EAttachmentHandlerMailPrivate EAttachmentHandlerMailPrivate;

struct _EAttachmentHandlerMail {
	EAttachmentHandler parent;
	EAttachmentHandlerMailPrivate *priv;
};

struct _EAttachmentHandlerMailClass {
	EAttachmentHandlerClass parent_class;
};

GType		e_attachment_handler_mail_get_type	(void);

G_END_DECLS

#endif /* E_ATTACHMENT_HANDLER_MAIL_H */
