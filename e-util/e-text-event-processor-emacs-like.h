/*
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
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/* ETextEventProcessorEmacsLike - Turns events on a text widget into commands.
 * Uses an emacs-ish interface. */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_TEXT_EVENT_PROCESSOR_EMACS_LIKE_H
#define E_TEXT_EVENT_PROCESSOR_EMACS_LIKE_H

#include <e-util/e-text-event-processor.h>

/* Standard GObject macros */
#define E_TYPE_TEXT_EVENT_PROCESSOR_EMACS_LIKE \
	(e_text_event_processor_emacs_like_get_type ())
#define E_TEXT_EVENT_PROCESSOR_EMACS_LIKE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_TEXT_EVENT_PROCESSOR_EMACS_LIKE, ETextEventProcessorEmacsLike))
#define E_TEXT_EVENT_PROCESSOR_EMACS_LIKE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_TEXT_EVENT_PROCESSOR_EMACS_LIKE, ETextEventProcessorEmacsLikeClass))
#define E_IS_TEXT_EVENT_PROCESSOR_EMACS_LIKE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_TEXT_EVENT_PROCESSOR_EMACS_LIKE))
#define E_IS_TEXT_EVENT_PROCESSOR_EMACS_LIKE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_TEXT_EVENT_PROCESSOR_EMACS_LIKE))
#define E_TEXT_EVENT_PROCESSOR_EMACS_LIKE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_TEXT_EVENT_PROCESSOR_EMACS_LIKE, ETextEventProcessorEmacsLikeClass))

G_BEGIN_DECLS

typedef struct _ETextEventProcessorEmacsLike       ETextEventProcessorEmacsLike;
typedef struct _ETextEventProcessorEmacsLikeClass  ETextEventProcessorEmacsLikeClass;

struct _ETextEventProcessorEmacsLike {
	ETextEventProcessor parent;

	/* object specific fields */
	guint mouse_down : 1;
};

struct _ETextEventProcessorEmacsLikeClass {
	ETextEventProcessorClass parent_class;
};

GType		e_text_event_processor_emacs_like_get_type
							(void) G_GNUC_CONST;
ETextEventProcessor *
		e_text_event_processor_emacs_like_new	(void);

G_END_DECLS

#endif /* E_TEXT_EVENT_PROCESSOR_EMACS_LIKE_H */
