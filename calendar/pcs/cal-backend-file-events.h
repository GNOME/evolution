/* Evolution calendar - iCalendar file backend
 *
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Author: Federico Mena-Quintero <federico@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef CAL_BACKEND_FILE_EVENTS_H
#define CAL_BACKEND_FILE_EVENTS_H

#include "cal-backend-file.h"

G_BEGIN_DECLS



#define CAL_BACKEND_FILE_EVENTS_TYPE            (cal_backend_file_events_get_type ())
#define CAL_BACKEND_FILE_EVENTS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAL_BACKEND_FILE_EVENTS_TYPE,		\
					  CalBackendFileEvents))
#define CAL_BACKEND_FILE_EVENTS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CAL_BACKEND_FILE_EVENTS_TYPE,	\
					  CalBackendFileEventsClass))
#define IS_CAL_BACKEND_FILE_EVENTS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAL_BACKEND_FILE_EVENTS_TYPE))
#define IS_CAL_BACKEND_FILE_EVENTS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CAL_BACKEND_FILE_EVENTS_TYPE))

typedef struct _CalBackendFileEvents CalBackendFileEvents;
typedef struct _CalBackendFileEventsClass CalBackendFileEventsClass;

typedef struct _CalBackendFileEventsPrivate CalBackendFileEventsPrivate;

struct _CalBackendFileEvents {
	CalBackendFile backend;

	/* Private data */
	CalBackendFileEventsPrivate *priv;
};

struct _CalBackendFileEventsClass {
	CalBackendFileClass parent_class;
};

GType cal_backend_file_events_get_type (void);



G_END_DECLS

#endif
