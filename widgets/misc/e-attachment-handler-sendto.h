/*
 * e-attachment-handler-sendto.h
 *
 * Copyright (C) 2009 Matthew Barnes
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 */

#ifndef E_ATTACHMENT_HANDLER_SENDTO_H
#define E_ATTACHMENT_HANDLER_SENDTO_H

#include <misc/e-attachment-handler.h>

/* Standard GObject macros */
#define E_TYPE_ATTACHMENT_HANDLER_SENDTO \
	(e_attachment_handler_sendto_get_type ())
#define E_ATTACHMENT_HANDLER_SENDTO(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_ATTACHMENT_HANDLER_SENDTO, EAttachmentHandlerSendto))
#define E_ATTACHMENT_HANDLER_SENDTO_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_ATTACHMENT_HANDLER_SENDTO, EAttachmentHandlerSendtoClass))
#define E_IS_ATTACHMENT_HANDLER_SENDTO(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_ATTACHMENT_HANDLER_SENDTO))
#define E_IS_ATTACHMENT_HANDLER_SENDTO_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_ATTACHMENT_HANDLER_SENDTO))
#define E_ATTACHMENT_HANDLER_SENDTO_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_ATTACHMENT_HANDLER_SENDTO, EAttachmentHandlerSendtoClass))

G_BEGIN_DECLS

typedef struct _EAttachmentHandlerSendto EAttachmentHandlerSendto;
typedef struct _EAttachmentHandlerSendtoClass EAttachmentHandlerSendtoClass;

struct _EAttachmentHandlerSendto {
	EAttachmentHandler parent;
};

struct _EAttachmentHandlerSendtoClass {
	EAttachmentHandlerClass parent_class;
};

GType		e_attachment_handler_sendto_get_type	(void);

G_END_DECLS

#endif /* E_ATTACHMENT_HANDLER_SENDTO_H */
