/* GNOME calendar listener
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

#ifndef CAL_LISTENER_H
#define CAL_LISTENER_H

#include <libgnome/gnome-defs.h>
#include <bonobo/gnome-object.h>
#include "gnome-calendar.h"

BEGIN_GNOME_DECLS



#define CAL_LISTENER_TYPE            (cal_listener_get_type ())
#define CAL_LISTENER(obj)            (GTK_CHECK_CAST ((obj), CAL_LISTENER_TYPE, CalListener))
#define CAL_LISTENER_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), CAL_LISTENER_TYPE,	\
				      CalListenerClass))
#define IS_CAL_LISTENER(obj)         (GTK_CHECK_TYPE ((obj), CAL_LISTENER_TYPE))
#define IS_CAL_LISTENER_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), CAL_LISTENER_TYPE))

typedef struct _CalListener CalListener;
typedef struct _CalListenerClass CalListenerClass;

/* Load status for the cal_loaded signal.  We need better error reporting. */
typedef enum {
	CAL_LISTENER_LOAD_SUCCESS,
	CAL_LISTENER_LOAD_ERROR
} CalListenerLoadStatus;

struct _CalListener {
	GnomeObject object;

	/* Private data */
	gpointer priv;
};

struct _CalListenerClass {
	GnomeObjectClass parent_class;

	void (* cal_loaded) (CalListener *listener,
			     CalListenerLoadStatus status,
			     GNOME_Calendar_Cal cal);
	void (* obj_added) (CalListener *listener, GNOME_Calendar_CalObj calobj);
	void (* obj_removed) (CalListener *listener, GNOME_Calendar_CalObjUID uid);
	void (* obj_changed) (CalListener *listener, GNOME_Calendar_CalObj calobj);
};

GtkType cal_listener_get_type (void);

CalListener *cal_listener_construct (CalListener *listener, GNOME_Calendar_Listener corba_listener);
GNOME_Calendar_Listener cal_listener_corba_object_create (GnomeObject *object);

CalListener *cal_listener_new (void);

GNOME_Calendar_Cal cal_listener_get_calendar (CalListener *listener);

POA_GNOME_Calendar_Listener__epv *cal_listener_get_epv (void);



END_GNOME_DECLS

#endif
