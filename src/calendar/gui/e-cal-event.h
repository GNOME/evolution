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
 *		David Trowbridge <trowbrds@cs.colorado.edu>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __E_CAL_EVENT_H__
#define __E_CAL_EVENT_H__

#include <shell/e-shell-backend.h>

G_BEGIN_DECLS

typedef struct _ECalEvent ECalEvent;
typedef struct _ECalEventClass ECalEventClass;

enum _e_cal_event_target_t {
	E_CAL_EVENT_TARGET_BACKEND,
};

/* Flags that describe TARGET_BACKEND */
enum {
	E_CAL_EVENT_MODULE_MIGRATION = 1 << 0,
};

typedef struct _ECalEventTargetBackend ECalEventTargetBackend;

struct _ECalEventTargetBackend {
	EEventTarget target;
	EShellBackend *shell_backend;
};

struct _ECalEvent {
	EEvent event;

	struct _ECalEventPrivate *priv;
};

struct _ECalEventClass {
	EEventClass event_class;
};

GType		e_cal_event_get_type		(void);
ECalEvent *	e_cal_event_peek		(void);
ECalEventTargetBackend *
		e_cal_event_target_new_module	(ECalEvent *ece,
						 EShellBackend *shell_backend,
						 guint32 flags);

G_END_DECLS

#endif /* __E_CAL_EVENT_H__ */
