/*
 * e-cal-attachment-handler.h
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

#ifndef E_CAL_ATTACHMENT_HANDLER_H
#define E_CAL_ATTACHMENT_HANDLER_H

#include <e-util/e-util.h>

/* Standard GObject macros */
#define E_TYPE_CAL_ATTACHMENT_HANDLER \
	(e_cal_attachment_handler_get_type ())
#define E_CAL_ATTACHMENT_HANDLER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CAL_ATTACHMENT_HANDLER, ECalAttachmentHandler))
#define E_CAL_ATTACHMENT_HANDLER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CAL_ATTACHMENT_HANDLER, ECalAttachmentHandlerClass))
#define E_IS_CAL_ATTACHMENT_HANDLER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CAL_ATTACHMENT_HANDLER))
#define E_IS_CAL_ATTACHMENT_HANDLER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CAL_ATTACHMENT_HANDLER))
#define E_CAL_ATTACHMENT_HANDLER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CAL_ATTACHMENT_HANDLER, ECalAttachmentHandlerClass))

G_BEGIN_DECLS

typedef struct _ECalAttachmentHandler ECalAttachmentHandler;
typedef struct _ECalAttachmentHandlerClass ECalAttachmentHandlerClass;
typedef struct _ECalAttachmentHandlerPrivate ECalAttachmentHandlerPrivate;

struct _ECalAttachmentHandler {
	EAttachmentHandler parent;
	ECalAttachmentHandlerPrivate *priv;
};

struct _ECalAttachmentHandlerClass {
	EAttachmentHandlerClass parent_class;
};

GType		e_cal_attachment_handler_get_type	(void);
void		e_cal_attachment_handler_type_register	(GTypeModule *type_module);

G_END_DECLS

#endif /* E_CAL_ATTACHMENT_HANDLER_H */
