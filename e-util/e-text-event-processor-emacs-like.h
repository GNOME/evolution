/*
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
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __E_TEXT_EVENT_PROCESSOR_EMACS_LIKE_H__
#define __E_TEXT_EVENT_PROCESSOR_EMACS_LIKE_H__

#include <e-util/e-text-event-processor.h>

G_BEGIN_DECLS

/* ETextEventProcessorEmacsLike - Turns events on a text widget into commands.  Uses an emacs-ish interface.
 *
 */

#define E_TEXT_EVENT_PROCESSOR_EMACS_LIKE_TYPE			(e_text_event_processor_emacs_like_get_type ())
#define E_TEXT_EVENT_PROCESSOR_EMACS_LIKE(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TEXT_EVENT_PROCESSOR_EMACS_LIKE_TYPE, ETextEventProcessorEmacsLike))
#define E_TEXT_EVENT_PROCESSOR_EMACS_LIKE_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), E_TEXT_EVENT_PROCESSOR_EMACS_LIKE_TYPE, ETextEventProcessorEmacsLikeClass))
#define E_IS_TEXT_EVENT_PROCESSOR_EMACS_LIKE(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TEXT_EVENT_PROCESSOR_EMACS_LIKE_TYPE))
#define E_IS_TEXT_EVENT_PROCESSOR_EMACS_LIKE_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE ((obj), E_TEXT_EVENT_PROCESSOR_EMACS_LIKE_TYPE))

typedef struct _ETextEventProcessorEmacsLike       ETextEventProcessorEmacsLike;
typedef struct _ETextEventProcessorEmacsLikeClass  ETextEventProcessorEmacsLikeClass;

struct _ETextEventProcessorEmacsLike
{
	ETextEventProcessor parent;

	/* object specific fields */
	guint mouse_down : 1;
};

struct _ETextEventProcessorEmacsLikeClass
{
	ETextEventProcessorClass parent_class;
};

GType      e_text_event_processor_emacs_like_get_type (void);
ETextEventProcessor *e_text_event_processor_emacs_like_new (void);

G_END_DECLS

#endif /* __E_TEXT_EVENT_PROCESSOR_EMACS_LIKE_H__ */
