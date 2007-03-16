/*
 * Authors: David Trowbridge <trowbrds@cs.colorado.edu>
 *
 * Copyright (C) 2005 Novell, Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#ifndef __E_CAL_EVENT_H__
#define __E_CAL_EVENT_H__

#include <glib-object.h>

#include "e-util/e-event.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif

typedef struct _ECalEvent ECalEvent;
typedef struct _ECalEventClass ECalEventClass;

enum _e_cal_event_target_t {
	E_CAL_EVENT_TARGET_COMPONENT,
};

/* Flags that describe TARGET_COMPONENT */
enum {
	E_CAL_EVENT_COMPONENT_MIGRATION = 1 << 0,
};

typedef struct _ECalEventTargetComponent ECalEventTargetComponent;

struct _ECalEventTargetComponent {
	EEventTarget target;

	struct _CalendarComponent *component;
};

struct _ECalEvent {
	EEvent event;

	struct _ECalEventPrivate *priv;
};

struct _ECalEventClass {
	EEventClass event_class;
};

GType                     e_cal_event_get_type (void);
ECalEvent*                e_cal_event_peek (void);
ECalEventTargetComponent* e_cal_event_target_new_component (ECalEvent *ece, struct _CalendarComponent *component, guint32 flags);

/* ********************************************************************** */

typedef struct _ECalEventHook ECalEventHook;
typedef struct _ECalEventHookClass ECalEventHookClass;

struct _ECalEventHook {
	EEventHook hook;
};

struct _ECalEventHookClass {
	EEventHookClass hook_class;
};

GType e_cal_event_hook_get_type (void);

#ifdef __cplusplus
}
#endif

#endif /* __E_CAL_EVENT_H__ */
