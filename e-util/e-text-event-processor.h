/*
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
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/* ETextEventProcessor - Turns events on a text widget into commands. */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_TEXT_EVENT_PROCESSOR_H
#define E_TEXT_EVENT_PROCESSOR_H

#include <gtk/gtk.h>
#include <e-util/e-text-event-processor-types.h>

/* Standard GObject macros */
#define E_TYPE_TEXT_EVENT_PROCESSOR \
	(e_text_event_processor_get_type ())
#define E_TEXT_EVENT_PROCESSOR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_TEXT_EVENT_PROCESSOR, ETextEventProcessor))
#define E_TEXT_EVENT_PROCESSOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_TEXT_EVENT_PROCESSOR, ETextEventProcessorClass))
#define E_IS_TEXT_EVENT_PROCESSOR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_TEXT_EVENT_PROCESSOR))
#define E_IS_TEXT_EVENT_PROCESSOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((obj), E_TYPE_TEXT_EVENT_PROCESSOR))
#define E_TEXT_EVENT_PROCESSOR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_TEXT_EVENT_PROCESSOR, ETextEventProcessorClass))

G_BEGIN_DECLS

typedef struct _ETextEventProcessor ETextEventProcessor;
typedef struct _ETextEventProcessorClass  ETextEventProcessorClass;

struct _ETextEventProcessor {
	GObject parent;

	/* object specific fields */
	guint allow_newlines : 1;
};

struct _ETextEventProcessorClass {
	GObjectClass parent_class;

	/* signals */
	void		(*command)	(ETextEventProcessor *tep,
					 ETextEventProcessorCommand *command);

	/* virtual functions */
	gint		(*event)	(ETextEventProcessor *tep,
					 ETextEventProcessorEvent *event);
};

GType		e_text_event_processor_get_type
					(void) G_GNUC_CONST;
gint		e_text_event_processor_handle_event
					(ETextEventProcessor *tep,
					 ETextEventProcessorEvent *event);

G_END_DECLS

#endif /* E_TEXT_EVENT_PROCESSOR_H */
