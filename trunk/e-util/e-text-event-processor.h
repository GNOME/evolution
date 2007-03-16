/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-text-event-processor.h
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef __E_TEXT_EVENT_PROCESSOR_H__
#define __E_TEXT_EVENT_PROCESSOR_H__

#include <glib.h>
#include <gtk/gtkobject.h>
#include <e-util/e-text-event-processor-types.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

/* ETextEventProcessor - Turns events on a text widget into commands.
 *
 */

#define E_TEXT_EVENT_PROCESSOR_TYPE		(e_text_event_processor_get_type ())
#define E_TEXT_EVENT_PROCESSOR(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TEXT_EVENT_PROCESSOR_TYPE, ETextEventProcessor))
#define E_TEXT_EVENT_PROCESSOR_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), E_TEXT_EVENT_PROCESSOR_TYPE, ETextEventProcessorClass))
#define E_IS_TEXT_EVENT_PROCESSOR(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TEXT_EVENT_PROCESSOR_TYPE))
#define E_IS_TEXT_EVENT_PROCESSOR_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((obj), E_TEXT_EVENT_PROCESSOR_TYPE))
#define E_TEXT_EVENT_PROCESSOR_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), E_TEXT_EVENT_PROCESSOR_TYPE, ETextEventProcessorClass))
typedef struct _ETextEventProcessor       ETextEventProcessor;
typedef struct _ETextEventProcessorClass  ETextEventProcessorClass;

struct _ETextEventProcessor
{
	GObject parent;

	/* object specific fields */
	guint allow_newlines : 1;
};

struct _ETextEventProcessorClass
{
	GtkObjectClass parent_class;

	/* signals */
	void (* command) (ETextEventProcessor *tep, ETextEventProcessorCommand *command);

	/* virtual functions */
	gint (* event) (ETextEventProcessor *tep, ETextEventProcessorEvent *event);
};


GType      e_text_event_processor_get_type (void);
gint       e_text_event_processor_handle_event (ETextEventProcessor *tep, ETextEventProcessorEvent *event);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __E_TEXT_EVENT_PROCESSOR_H__ */
