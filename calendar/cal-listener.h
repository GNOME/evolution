/* Evolution calendar listener
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
#include <bonobo/bonobo-object.h>
#include "evolution-calendar.h"

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
	BonoboObject object;

	/* Private data */
	gpointer priv;
};

struct _CalListenerClass {
	BonoboObjectClass parent_class;

	void (* cal_loaded) (CalListener *listener,
			     CalListenerLoadStatus status,
			     Evolution_Calendar_Cal cal);
	void (* obj_added) (CalListener *listener, Evolution_Calendar_CalObj calobj);
	void (* obj_removed) (CalListener *listener, Evolution_Calendar_CalObjUID uid);
	void (* obj_changed) (CalListener *listener, Evolution_Calendar_CalObj calobj);
};

GtkType cal_listener_get_type (void);

CalListener *cal_listener_construct (CalListener *listener,
				     Evolution_Calendar_Listener corba_listener);

Evolution_Calendar_Listener cal_listener_corba_object_create (BonoboObject *object);

CalListener *cal_listener_new (void);

Evolution_Calendar_Cal cal_listener_get_calendar (CalListener *listener);

POA_Evolution_Calendar_Listener__epv *cal_listener_get_epv (void);



END_GNOME_DECLS

#endif
