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

#include <gtk/gtkobject.h>
#include <bonobo/bonobo-xobject.h>
#include "evolution-calendar.h"

G_BEGIN_DECLS



#define CAL_LISTENER_TYPE            (cal_listener_get_type ())
#define CAL_LISTENER(obj)            (GTK_CHECK_CAST ((obj), CAL_LISTENER_TYPE, CalListener))
#define CAL_LISTENER_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), CAL_LISTENER_TYPE,	\
				      CalListenerClass))
#define IS_CAL_LISTENER(obj)         (GTK_CHECK_TYPE ((obj), CAL_LISTENER_TYPE))
#define IS_CAL_LISTENER_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), CAL_LISTENER_TYPE))

typedef struct CalListenerPrivate CalListenerPrivate;

typedef struct {
	BonoboXObject xobject;

	/* Private data */
	CalListenerPrivate *priv;
} CalListener;

typedef struct {
	BonoboXObjectClass parent_class;

	POA_GNOME_Evolution_Calendar_Listener__epv epv;
} CalListenerClass;

/* Notification functions */
typedef void (* CalListenerCalOpenedFn) (CalListener *listener,
					 GNOME_Evolution_Calendar_Listener_OpenStatus status,
					 GNOME_Evolution_Calendar_Cal cal,
					 gpointer data);

typedef void (* CalListenerCalSetModeFn) (CalListener *listener,
					  GNOME_Evolution_Calendar_Listener_SetModeStatus status,
					  GNOME_Evolution_Calendar_CalMode mode,
					  gpointer data);

typedef void (* CalListenerObjUpdatedFn) (CalListener *listener,
					  const GNOME_Evolution_Calendar_CalObjUID uid,
					  gpointer data);
typedef void (* CalListenerObjRemovedFn) (CalListener *listener,
					  const GNOME_Evolution_Calendar_CalObjUID uid,
					  gpointer data);

typedef void (* CalListenerErrorOccurredFn) (CalListener *listener,
					     const char *message,
					     gpointer data);

typedef void (* CalListenerCategoriesChangedFn) (CalListener *listener,
						 const GNOME_Evolution_Calendar_StringSeq *categories,
						 gpointer data);


GtkType cal_listener_get_type (void);

CalListener *cal_listener_construct (CalListener *listener,
				     CalListenerCalOpenedFn cal_opened_fn,
				     CalListenerCalSetModeFn cal_set_mode_fn,
				     CalListenerObjUpdatedFn obj_updated_fn,
				     CalListenerObjRemovedFn obj_removed_fn,
				     CalListenerErrorOccurredFn error_occurred_fn,
				     CalListenerCategoriesChangedFn categories_changed_fn,
				     gpointer fn_data);

CalListener *cal_listener_new (CalListenerCalOpenedFn cal_opened_fn,
			       CalListenerCalSetModeFn cal_set_mode_fn,
			       CalListenerObjUpdatedFn obj_updated_fn,
			       CalListenerObjRemovedFn obj_removed_fn,
			       CalListenerErrorOccurredFn error_occurred_fn,
			       CalListenerCategoriesChangedFn categories_changed_fn,
			       gpointer fn_data);

void cal_listener_stop_notification (CalListener *listener);



G_END_DECLS

#endif
