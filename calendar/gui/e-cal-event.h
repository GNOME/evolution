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
 *		David Trowbridge <trowbrds@cs.colorado.edu>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __E_CAL_EVENT_H__
#define __E_CAL_EVENT_H__

#include <glib-object.h>

#include "e-util/e-event.h"
#include "shell/e-shell-module.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif

typedef struct _ECalEvent ECalEvent;
typedef struct _ECalEventClass ECalEventClass;

enum _e_cal_event_target_t {
	E_CAL_EVENT_TARGET_MODULE,
};

/* Flags that describe TARGET_MODULE */
enum {
	E_CAL_EVENT_MODULE_MIGRATION = 1 << 0,
};

typedef struct _ECalEventTargetModule ECalEventTargetModule;

struct _ECalEventTargetModule {
	EEventTarget target;
	EShellModule *shell_module;
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
ECalEventTargetModule* e_cal_event_target_new_module (ECalEvent *ece, EShellModule *shell_module, guint32 flags);

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
