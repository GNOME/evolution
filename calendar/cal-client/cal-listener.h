/* Evolution calendar listener
 *
 * Copyright (C) 2000 Helix Code, Inc.
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Author: Federico Mena-Quintero <federico@ximian.com>
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

typedef struct _CalListenerPrivate CalListenerPrivate;

struct _CalListener {
	BonoboObject object;

	/* Private data */
	CalListenerPrivate *priv;
};

struct _CalListenerClass {
	BonoboObjectClass parent_class;

	/* Notification signals */

	void (* cal_opened) (CalListener *listener,
			     GNOME_Evolution_Calendar_Listener_OpenStatus status,
			     GNOME_Evolution_Calendar_Cal cal);
	void (* obj_updated) (CalListener *listener, const GNOME_Evolution_Calendar_CalObjUID uid);
	void (* obj_removed) (CalListener *listener, const GNOME_Evolution_Calendar_CalObjUID uid);
};

/* Notification functions */
typedef void (* CalListenerCalOpenedFn) (CalListener *listener,
					 GNOME_Evolution_Calendar_Listener_OpenStatus status,
					 GNOME_Evolution_Calendar_Cal cal,
					 gpointer data);

typedef void (* CalListenerObjUpdatedFn) (CalListener *listener,
					  const GNOME_Evolution_Calendar_CalObjUID uid,
					  gpointer data);
typedef void (* CalListenerObjRemovedFn) (CalListener *listener,
					  const GNOME_Evolution_Calendar_CalObjUID uid,
					  gpointer data);


GtkType cal_listener_get_type (void);

CalListener *cal_listener_construct (CalListener *listener,
				     GNOME_Evolution_Calendar_Listener corba_listener,
				     CalListenerCalOpenedFn cal_opened_fn,
				     CalListenerObjUpdatedFn obj_updated_fn,
				     CalListenerObjRemovedFn obj_removed_fn,
				     gpointer fn_data);

GNOME_Evolution_Calendar_Listener cal_listener_corba_object_create (BonoboObject *object);

CalListener *cal_listener_new (CalListenerCalOpenedFn cal_opened_fn,
			       CalListenerObjUpdatedFn obj_updated_fn,
			       CalListenerObjRemovedFn obj_removed_fn,
			       gpointer fn_data);

POA_GNOME_Evolution_Calendar_Listener__epv *cal_listener_get_epv (void);



END_GNOME_DECLS

#endif
