/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Authors: Michel Zucchi <notzed@ximian.com>
 *
 *  Copyright 2003 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef __ES_EVENT_H__
#define __ES_EVENT_H__

#include <glib-object.h>

#include "e-util/e-event.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

typedef struct _ESEvent ESEvent;
typedef struct _ESEventClass ESEventClass;

/* Current target description */
enum _es_event_target_t {
	ES_EVENT_TARGET_STATE,
};

/* Flags that qualify TARGET_STATE */
enum {
	ES_EVENT_STATE_ONLINE = 1<< 0,
	ES_EVENT_STATE_OFFLINE = 1<< 0,
};

typedef struct _ESEventTargetState ESEventTargetState;

struct _ESEventTargetState {
	EEventTarget target;

	int state;
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

GType es_event_get_type(void);

ESEvent *es_event_peek(void);

ESEventTargetState *es_event_target_new_state(ESEvent *emp, int state);

/* ********************************************************************** */

typedef struct _ESEventHook ESEventHook;
typedef struct _ESEventHookClass ESEventHookClass;

struct _ESEventHook {
	EEventHook hook;
};

struct _ESEventHookClass {
	EEventHookClass hook_class;
};

GType es_event_hook_get_type(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __ES_EVENT_H__ */
