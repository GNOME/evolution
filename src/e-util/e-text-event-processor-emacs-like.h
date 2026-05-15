/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Chris Lahey <clahey@ximian.com>
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
