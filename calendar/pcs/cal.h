/* Evolution calendar client interface object
 *
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@ximian.com>
 *          Rodrigo Moya <rodrigo@ximian.com>
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

#ifndef CAL_H
#define CAL_H

#include <bonobo/bonobo-object.h>
#include "pcs/evolution-calendar.h"
#include "pcs/cal-common.h"
#include "pcs/query.h"

G_BEGIN_DECLS



#define CAL_TYPE            (cal_get_type ())
#define CAL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAL_TYPE, Cal))
#define CAL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CAL_TYPE, CalClass))
#define IS_CAL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAL_TYPE))
#define IS_CAL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CAL_TYPE))

typedef struct _CalPrivate CalPrivate;

struct _Cal {
	BonoboObject object;

	/* Private data */
	CalPrivate *priv;
};

struct _CalClass {
	BonoboObjectClass parent_class;

	POA_GNOME_Evolution_Calendar_Cal__epv epv;
};

GType cal_get_type (void);

Cal *cal_construct (Cal *cal,
		    CalBackend *backend,
		    GNOME_Evolution_Calendar_Listener listener);

Cal *cal_new (CalBackend *backend, const char *uri, GNOME_Evolution_Calendar_Listener listener);

CalBackend *cal_get_backend (Cal *cal);
GNOME_Evolution_Calendar_Listener cal_get_listener (Cal *cal);

void cal_notify_read_only (Cal *cal, GNOME_Evolution_Calendar_CallStatus status, gboolean read_only);
void cal_notify_cal_address (Cal *cal, GNOME_Evolution_Calendar_CallStatus status, const char *address);
void cal_notify_alarm_email_address (Cal *cal, GNOME_Evolution_Calendar_CallStatus status, const char *address);
void cal_notify_ldap_attribute (Cal *cal, GNOME_Evolution_Calendar_CallStatus status, const char *attribute);
void cal_notify_static_capabilities (Cal *cal, GNOME_Evolution_Calendar_CallStatus status, const char *capabilities);

void cal_notify_open (Cal *cal, GNOME_Evolution_Calendar_CallStatus status);
void cal_notify_remove (Cal *cal, GNOME_Evolution_Calendar_CallStatus status);

void cal_notify_object_created (Cal *cal, GNOME_Evolution_Calendar_CallStatus status,
				const char *uid, const char *object);
void cal_notify_object_modified (Cal *cal, GNOME_Evolution_Calendar_CallStatus status, 
				 const char *old_object, const char *object);
void cal_notify_object_removed (Cal *cal, GNOME_Evolution_Calendar_CallStatus status, 
				const char *uid, const char *object);
void cal_notify_alarm_discarded (Cal *cal, GNOME_Evolution_Calendar_CallStatus status);

void cal_notify_objects_received (Cal *cal, GNOME_Evolution_Calendar_CallStatus status);
void cal_notify_objects_sent (Cal *cal, GNOME_Evolution_Calendar_CallStatus status);

void cal_notify_default_object (Cal *cal, GNOME_Evolution_Calendar_CallStatus status, char *object);
void cal_notify_object (Cal *cal, GNOME_Evolution_Calendar_CallStatus status, char *object);
void cal_notify_object_list (Cal *cal, GNOME_Evolution_Calendar_CallStatus status, GList *objects);

void cal_notify_query (Cal *cal, GNOME_Evolution_Calendar_CallStatus status, Query *query);

void cal_notify_timezone_requested (Cal *cal, GNOME_Evolution_Calendar_CallStatus status, const char *object);
void cal_notify_timezone_added (Cal *cal, GNOME_Evolution_Calendar_CallStatus status, const char *tzid);
void cal_notify_default_timezone_set (Cal *cal, GNOME_Evolution_Calendar_CallStatus status);

void cal_notify_changes (Cal *cal, GNOME_Evolution_Calendar_CallStatus status, GList *adds, GList *modifies, GList *deletes);
void cal_notify_free_busy (Cal *cal, GNOME_Evolution_Calendar_CallStatus status, GList *freebusy);

void cal_notify_mode (Cal *cal, 
		      GNOME_Evolution_Calendar_Listener_SetModeStatus status, 
		      GNOME_Evolution_Calendar_CalMode mode);
void cal_notify_error (Cal *cal, const char *message);

void cal_notify_categories_changed (Cal *cal, GNOME_Evolution_Calendar_StringSeq *categories);



G_END_DECLS

#endif
