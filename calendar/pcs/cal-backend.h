/* Evolution calendar backend
 *
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Author: Federico Mena-Quintero <federico@helixcode.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#ifndef CAL_BACKEND_H
#define CAL_BACKEND_H

#include <libgnome/gnome-defs.h>
#include <libgnomevfs/gnome-vfs.h>
#include <cal-util/cal-util.h>
#include "evolution-calendar.h"
#include "cal-common.h"
#include "cal.h"

BEGIN_GNOME_DECLS



#define CAL_BACKEND_TYPE            (cal_backend_get_type ())
#define CAL_BACKEND(obj)            (GTK_CHECK_CAST ((obj), CAL_BACKEND_TYPE, CalBackend))
#define CAL_BACKEND_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), CAL_BACKEND_TYPE,		\
				     CalBackendClass))
#define IS_CAL_BACKEND(obj)         (GTK_CHECK_TYPE ((obj), CAL_BACKEND_TYPE))
#define IS_CAL_BACKEND_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), CAL_BACKEND_TYPE))

/* Load status values */
typedef enum {
	CAL_BACKEND_LOAD_SUCCESS,	/* Loading OK */
	CAL_BACKEND_LOAD_ERROR		/* We need better error reporting in libversit */
} CalBackendLoadStatus;

struct _CalBackend {
	GtkObject object;

	/* Private data */
	gpointer priv;
};

struct _CalBackendClass {
	GtkObjectClass parent_class;
};

typedef enum {
	CAL_VCAL,
	CAL_ICAL
} CalendarFormat;

GtkType cal_backend_get_type (void);

CalBackend *cal_backend_new (void);

GnomeVFSURI *cal_backend_get_uri (CalBackend *backend);

void cal_backend_add_cal (CalBackend *backend, Cal *cal);
void cal_backend_remove_cal (CalBackend *backend, Cal *cal);

CalBackendLoadStatus cal_backend_load (CalBackend *backend, GnomeVFSURI *uri);

void cal_backend_create (CalBackend *backend, GnomeVFSURI *uri);

char *cal_backend_get_object (CalBackend *backend, const char *uid);

GList *cal_backend_get_uids (CalBackend *backend, CalObjType type);

GList *cal_backend_get_events_in_range (CalBackend *backend, time_t start, time_t end);

gboolean cal_backend_update_object (CalBackend *backend, const char *uid, const char *calobj);

gboolean cal_backend_remove_object (CalBackend *backend, const char *uid);



END_GNOME_DECLS

#endif
