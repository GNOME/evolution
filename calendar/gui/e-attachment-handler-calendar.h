/*
 * e-attachment-handler-calendar.h
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

#ifndef E_ATTACHMENT_HANDLER_CALENDAR_H
#define E_ATTACHMENT_HANDLER_CALENDAR_H

#include <misc/e-attachment-handler.h>

/* Standard GObject macros */
#define E_TYPE_ATTACHMENT_HANDLER_CALENDAR \
	(e_attachment_handler_calendar_get_type ())
#define E_ATTACHMENT_HANDLER_CALENDAR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_ATTACHMENT_HANDLER_CALENDAR, EAttachmentHandlerCalendar))
#define E_ATTACHMENT_HANDLER_CALENDAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_ATTACHMENT_HANDLER_CALENDAR, EAttachmentHandlerCalendarClass))
#define E_IS_ATTACHMENT_HANDLER_CALENDAR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_ATTACHMENT_HANDLER_CALENDAR))
#define E_IS_ATTACHMENT_HANDLER_CALENDAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_ATTACHMENT_HANDLER_CALENDAR))
#define E_ATTACHMENT_HANDLER_CALENDAR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_ATTACHMENT_HANDLER_CALENDAR, EAttachmentHandlerCalendarClass))

G_BEGIN_DECLS

typedef struct _EAttachmentHandlerCalendar EAttachmentHandlerCalendar;
typedef struct _EAttachmentHandlerCalendarClass EAttachmentHandlerCalendarClass;
typedef struct _EAttachmentHandlerCalendarPrivate EAttachmentHandlerCalendarPrivate;

struct _EAttachmentHandlerCalendar {
	EAttachmentHandler parent;
	EAttachmentHandlerCalendarPrivate *priv;
};

struct _EAttachmentHandlerCalendarClass {
	EAttachmentHandlerClass parent_class;
};

GType		e_attachment_handler_calendar_get_type	(void);

G_END_DECLS

#endif /* E_ATTACHMENT_HANDLER_CALENDAR_H */
