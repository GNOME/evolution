/*
 * e-attachment-handler.h
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

#ifndef E_ATTACHMENT_HANDLER_H
#define E_ATTACHMENT_HANDLER_H

#include <libebackend/libebackend.h>

#include <e-util/e-attachment-view.h>

/* Standard GObject macros */
#define E_TYPE_ATTACHMENT_HANDLER \
	(e_attachment_handler_get_type ())
#define E_ATTACHMENT_HANDLER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_ATTACHMENT_HANDLER, EAttachmentHandler))
#define E_ATTACHMENT_HANDLER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_ATTACHMENT_HANDLER, EAttachmentHandlerClass))
#define E_IS_ATTACHMENT_HANDLER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_ATTACHMENT_HANDLER))
#define E_IS_ATTACHMENT_HANDLER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_ATTACHMENT_HANDLER))
#define E_ATTACHMENT_HANDLER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_ATTACHMENT_HANDLER, EAttachmentHandlerClass))

G_BEGIN_DECLS

typedef struct _EAttachmentHandler EAttachmentHandler;
typedef struct _EAttachmentHandlerClass EAttachmentHandlerClass;
typedef struct _EAttachmentHandlerPrivate EAttachmentHandlerPrivate;

struct _EAttachmentHandler {
	EExtension parent;
	EAttachmentHandlerPrivate *priv;
};

struct _EAttachmentHandlerClass {
	EExtensionClass parent_class;

	GdkDragAction	(*get_drag_actions)	(EAttachmentHandler *handler);
	const GtkTargetEntry *
			(*get_target_table)	(EAttachmentHandler *handler,
						 guint *n_targets);
};

GType		e_attachment_handler_get_type	(void) G_GNUC_CONST;
EAttachmentView *
		e_attachment_handler_get_view	(EAttachmentHandler *handler);
GdkDragAction	e_attachment_handler_get_drag_actions
						(EAttachmentHandler *handler);
const GtkTargetEntry *
		e_attachment_handler_get_target_table
						(EAttachmentHandler *handler,
						 guint *n_targets);

G_END_DECLS

#endif /* E_ATTACHMENT_HANDLER_H */
