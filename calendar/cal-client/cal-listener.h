/* Evolution calendar listener
 *
 * Copyright (C) 2001 Ximian, Inc.
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

#ifndef CAL_LISTENER_H
#define CAL_LISTENER_H

#include <bonobo/bonobo-object.h>
#include "evolution-calendar.h"
#include "cal-client-types.h"

G_BEGIN_DECLS



#define CAL_LISTENER_TYPE            (cal_listener_get_type ())
#define CAL_LISTENER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAL_LISTENER_TYPE, CalListener))
#define CAL_LISTENER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CAL_LISTENER_TYPE,	\
				      CalListenerClass))
#define IS_CAL_LISTENER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAL_LISTENER_TYPE))
#define IS_CAL_LISTENER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CAL_LISTENER_TYPE))

typedef struct CalListenerPrivate CalListenerPrivate;

typedef struct {
	BonoboObject xobject;

	/* Private data */
	CalListenerPrivate *priv;
} CalListener;

typedef struct {
	BonoboObjectClass parent_class;

	POA_GNOME_Evolution_Calendar_Listener__epv epv;

	/* Signals */
	void (*read_only) (CalListener *listener, ECalendarStatus status, gboolean read_only);
	void (*cal_address) (CalListener *listener, ECalendarStatus status, const char *address);
	void (*alarm_address) (CalListener *listener, ECalendarStatus status, const char *address);
	void (*ldap_attribute) (CalListener *listener, ECalendarStatus status, const char *ldap_attribute);
	void (*static_capabilities) (CalListener *listener, ECalendarStatus status, const char *capabilities);

	void (*open) (CalListener *listener, ECalendarStatus status);
	void (*remove) (CalListener *listener, ECalendarStatus status);

	void (*create_object) (CalListener *listener, ECalendarStatus status, const char *id);
	void (*modify_object) (CalListener *listener, ECalendarStatus status);
	void (*remove_object) (CalListener *listener, ECalendarStatus status);

	void (*discard_alarm) (CalListener *listener, ECalendarStatus status);

 	void (*receive_objects) (CalListener *listener, ECalendarStatus status);
 	void (*send_objects) (CalListener *listener, ECalendarStatus status);

	void (*default_object) (CalListener *listener, ECalendarStatus status, const char *object);
	void (*object) (CalListener *listener, ECalendarStatus status, const char *object);
	void (*object_list) (CalListener *listener, ECalendarStatus status, GList **objects);

	void (*get_timezone) (CalListener *listener, ECalendarStatus status, const char *object);
	void (*add_timezone) (CalListener *listener, ECalendarStatus status, const char *tzid);
	void (*set_default_timezone) (CalListener *listener, ECalendarStatus status, const char *tzid);

	void (*get_changes) (CalListener *listener, ECalendarStatus status, GList *changes);
	void (*get_free_busy) (CalListener *listener, ECalendarStatus status, GList *freebusy);
	
	void (*query) (CalListener *listener, ECalendarStatus status, GNOME_Evolution_Calendar_Query query);
} CalListenerClass;

/* Notification functions */
typedef void (* CalListenerCalSetModeFn) (CalListener *listener,
					  GNOME_Evolution_Calendar_Listener_SetModeStatus status,
					  GNOME_Evolution_Calendar_CalMode mode,
					  gpointer data);

typedef void (* CalListenerErrorOccurredFn) (CalListener *listener,
					     const char *message,
					     gpointer data);

typedef void (* CalListenerCategoriesChangedFn) (CalListener *listener,
						 const GNOME_Evolution_Calendar_StringSeq *categories,
						 gpointer data);


GType cal_listener_get_type (void);

CalListener *cal_listener_construct (CalListener *listener,
				     CalListenerCalSetModeFn cal_set_mode_fn,
				     CalListenerErrorOccurredFn error_occurred_fn,
				     CalListenerCategoriesChangedFn categories_changed_fn,
				     gpointer fn_data);

CalListener *cal_listener_new (CalListenerCalSetModeFn cal_set_mode_fn,
			       CalListenerErrorOccurredFn error_occurred_fn,
			       CalListenerCategoriesChangedFn categories_changed_fn,
			       gpointer fn_data);

void cal_listener_stop_notification (CalListener *listener);



G_END_DECLS

#endif
