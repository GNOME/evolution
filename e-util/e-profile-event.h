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
 *		Michel Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __E_PROFILE_EVENT_H__
#define __E_PROFILE_EVENT_H__

#include <glib-object.h>
#include <sys/time.h>

#include "e-util/e-event.h"

G_BEGIN_DECLS

typedef struct _EProfileEvent EProfileEvent;
typedef struct _EProfileEventClass EProfileEventClass;

/* Current target description */
enum _e_profile_event_target_t {
	E_PROFILE_EVENT_TARGET
};

/* Flags that qualify a target (UNIMPLEMENTED) */
enum {
	E_PROFILE_EVENT_START = 1<< 0,
	E_PROFILE_EVENT_END = 1<< 1,
	E_PROFILE_EVENT_CANCEL = 1<< 2
};

typedef struct _EProfileEventTarget EProfileEventTarget;

struct _EProfileEventTarget {
	EEventTarget target;

	struct timeval tv;
	gchar *id;		/* id of event */
	gchar *uid;		/* uid of event (folder/message, etc) */
};

typedef struct _EEventItem EProfileEventItem;

/* The object */
struct _EProfileEvent {
	EEvent popup;

	struct _EProfileEventPrivate *priv;
};

struct _EProfileEventClass {
	EEventClass popup_class;
};

GType e_profile_event_get_type(void);

EProfileEvent *e_profile_event_peek(void);

EProfileEventTarget *e_profile_event_target_new(EProfileEvent *emp, const gchar *id, const gchar *uid, guint32 flags);

/* we don't want ANY rubbish code lying around if we have profiling off */
#ifdef ENABLE_PROFILING
void e_profile_event_emit(const gchar *id, const gchar *uid, guint32 flags);
#else
#define e_profile_event_emit(a, b, c)
#endif

/* ********************************************************************** */

typedef struct _EProfileEventHook EProfileEventHook;
typedef struct _EProfileEventHookClass EProfileEventHookClass;

struct _EProfileEventHook {
	EEventHook hook;
};

struct _EProfileEventHookClass {
	EEventHookClass hook_class;
};

GType e_profile_event_hook_get_type(void);

G_END_DECLS

#endif /* __E_PROFILE_EVENT_H__ */
