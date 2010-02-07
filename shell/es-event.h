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

#ifndef __ES_EVENT_H__
#define __ES_EVENT_H__

#include <glib-object.h>

#include "e-util/e-event.h"

G_BEGIN_DECLS

typedef struct _ESEvent ESEvent;
typedef struct _ESEventClass ESEventClass;

/* Current target description */
enum _es_event_target_t {
	ES_EVENT_TARGET_STATE,
	ES_EVENT_TARGET_UPGRADE,
	ES_EVENT_TARGET_SHELL,
	ES_EVENT_TARGET_COMPONENT
};

/* Flags that qualify TARGET_STATE */
enum {
	ES_EVENT_STATE_ONLINE = 1<<0,
	ES_EVENT_STATE_OFFLINE = 1<<1
};

typedef struct _ESEventTargetState ESEventTargetState;
typedef struct _ESEventTargetUpgrade ESEventTargetUpgrade;
typedef struct _ESEventTargetShell ESEventTargetShell;
typedef struct _ESEventTargetComponent ESEventTargetComponent;

struct _ESEventTargetShell {
	EEventTarget target;
};

struct _ESEventTargetState {
	EEventTarget target;

	gint state;
};

struct _ESEventTargetUpgrade {
	EEventTarget target;

	gint major;
	gint minor;
	gint revision;
};

struct _ESEventTargetComponent {
	EEventTarget target;

	const gchar *id;
};

typedef struct _EEventItem ESEventItem;

/* The object */
struct _ESEvent {
	EEvent event;

	struct _ESEventPrivate *priv;
};

struct _ESEventClass {
	EEventClass event_class;
};

GType		es_event_get_type		(void);
ESEvent *	es_event_peek			(void);
ESEventTargetShell *
		es_event_target_new		(ESEvent *eme);
ESEventTargetState *
		es_event_target_new_state	(ESEvent *emp,
						 gint state);
ESEventTargetUpgrade *
		es_event_target_new_upgrade	(ESEvent *event,
						 gint major,
						 gint minor,
						 gint revision);
ESEventTargetComponent *
		es_event_target_new_component	(ESEvent *event,
						 const gchar *id);

/* ********************************************************************** */

typedef struct _ESEventHook ESEventHook;
typedef struct _ESEventHookClass ESEventHookClass;

struct _ESEventHook {
	EEventHook hook;
};

struct _ESEventHookClass {
	EEventHookClass hook_class;
};

GType		es_event_hook_get_type		(void);

G_END_DECLS

#endif /* __ES_EVENT_H__ */
