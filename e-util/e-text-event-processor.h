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
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __E_TEXT_EVENT_PROCESSOR_H__
#define __E_TEXT_EVENT_PROCESSOR_H__

#include <gtk/gtk.h>
#include <e-util/e-text-event-processor-types.h>

G_BEGIN_DECLS

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

G_END_DECLS

#endif /* __E_TEXT_EVENT_PROCESSOR_H__ */
