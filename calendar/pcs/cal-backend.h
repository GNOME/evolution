/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Evolution calendar - generic backend class
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

#ifndef CAL_BACKEND_H
#define CAL_BACKEND_H

#include <libgnome/gnome-defs.h>
#include <cal-util/cal-util.h>
#include <cal-util/cal-component.h>
#include "pcs/evolution-calendar.h"
#include "pcs/cal-common.h"
#include "pcs/cal.h"
#include "pcs/query.h"

BEGIN_GNOME_DECLS



#define CAL_BACKEND_TYPE            (cal_backend_get_type ())
#define CAL_BACKEND(obj)            (GTK_CHECK_CAST ((obj), CAL_BACKEND_TYPE, CalBackend))
#define CAL_BACKEND_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), CAL_BACKEND_TYPE,		\
				     CalBackendClass))
#define IS_CAL_BACKEND(obj)         (GTK_CHECK_TYPE ((obj), CAL_BACKEND_TYPE))
#define IS_CAL_BACKEND_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), CAL_BACKEND_TYPE))

/* Open status values */
typedef enum {
	CAL_BACKEND_OPEN_SUCCESS,	/* Loading OK */
	CAL_BACKEND_OPEN_ERROR,		/* We need better error reporting in libversit */
	CAL_BACKEND_OPEN_NOT_FOUND,
	CAL_BACKEND_OPEN_PERMISSION_DENIED
} CalBackendOpenStatus;

/* Update and Remove result values */
typedef enum {
	CAL_BACKEND_RESULT_SUCCESS,
	CAL_BACKEND_RESULT_INVALID_OBJECT,
	CAL_BACKEND_RESULT_NOT_FOUND,
	CAL_BACKEND_RESULT_PERMISSION_DENIED
} CalBackendResult;

/* Send result values */
typedef enum {
	CAL_BACKEND_SEND_SUCCESS,
	CAL_BACKEND_SEND_INVALID_OBJECT,
	CAL_BACKEND_SEND_BUSY,
	CAL_BACKEND_SEND_PERMISSION_DENIED,
} CalBackendSendResult;

/* Result codes for ::get_alarms_in_range() */
typedef enum {
	CAL_BACKEND_GET_ALARMS_SUCCESS,
	CAL_BACKEND_GET_ALARMS_NOT_FOUND,
	CAL_BACKEND_GET_ALARMS_INVALID_RANGE
} CalBackendGetAlarmsForObjectResult;

struct _CalBackend {
	GtkObject object;
	GList *clients;
};

struct _CalBackendClass {
	GtkObjectClass parent_class;

	/* Notification signals */
	void (* last_client_gone) (CalBackend *backend);
	void (* cal_added) (CalBackend *backend, Cal *cal);

	void (* opened) (CalBackend *backend, CalBackendOpenStatus status);
	void (* obj_updated) (CalBackend *backend, const char *uid);
	void (* obj_removed) (CalBackend *backend, const char *uid);

	/* Virtual methods */
	const char *(* get_uri) (CalBackend *backend);

	const char *(* get_cal_address) (CalBackend *backend);
	const char *(* get_alarm_email_address) (CalBackend *backend);
	const char *(* get_ldap_attribute) (CalBackend *backend);
	
	const char *(* get_static_capabilities) (CalBackend *backend);
	
	CalBackendOpenStatus (* open) (CalBackend *backend, const char *uristr,
				       gboolean only_if_exists);

	gboolean (* is_loaded) (CalBackend *backend);
	gboolean (* is_read_only) (CalBackend *backend);

	Query *(* get_query) (CalBackend *backend,
			      GNOME_Evolution_Calendar_QueryListener ql,
			      const char *sexp);

	/* Mode relate virtual methods */
	CalMode (* get_mode) (CalBackend *backend);
	void    (* set_mode) (CalBackend *backend, CalMode mode);	

	/* General object acquirement and information related virtual methods */
	int (* get_n_objects) (CalBackend *backend, CalObjType type);
	char *(* get_default_object) (CalBackend *backend, CalObjType type);
	char *(* get_object) (CalBackend *backend, const char *uid);
	CalComponent *(* get_object_component) (CalBackend *backend, const char *uid);
	char *(* get_timezone_object) (CalBackend *backend, const char *tzid);
	GList *(* get_uids) (CalBackend *backend, CalObjType type);

	GList *(* get_objects_in_range) (CalBackend *backend, CalObjType type,
					 time_t start, time_t end);
	GList *(* get_free_busy) (CalBackend *backend, GList *users, time_t start, time_t end);

	/* Change related virtual methods */
	GNOME_Evolution_Calendar_CalObjChangeSeq * (* get_changes) (
		CalBackend *backend, CalObjType type, const char *change_id);

	/* Alarm related virtual methods */
	GNOME_Evolution_Calendar_CalComponentAlarmsSeq *(* get_alarms_in_range) (
		CalBackend *backend, time_t start, time_t end);
	GNOME_Evolution_Calendar_CalComponentAlarms *(* get_alarms_for_object) (
		CalBackend *backend, const char *uid,
		time_t start, time_t end, gboolean *object_found);

	/* Object manipulation virtual methods */
	CalBackendResult (* update_objects) (CalBackend *backend, const char *calobj, CalObjModType mod);
	CalBackendResult (* remove_object) (CalBackend *backend, const char *uid, CalObjModType mod);

	CalBackendSendResult (* send_object) (CalBackend *backend, const char *calobj, char **new_calobj,
					      GNOME_Evolution_Calendar_UserList **user_list,
					      char error_msg[256]);

	/* Timezone related virtual methods */
	icaltimezone *(* get_timezone) (CalBackend *backend, const char *tzid);
	icaltimezone *(* get_default_timezone) (CalBackend *backend);
	gboolean (* set_default_timezone) (CalBackend *backend, const char *tzid);
};

GtkType cal_backend_get_type (void);

const char *cal_backend_get_uri (CalBackend *backend);

const char *cal_backend_get_cal_address (CalBackend *backend);
const char *cal_backend_get_alarm_email_address (CalBackend *backend);
const char *cal_backend_get_ldap_attribute (CalBackend *backend);

const char *cal_backend_get_static_capabilities (CalBackend *backend);

void cal_backend_add_cal (CalBackend *backend, Cal *cal);

CalBackendOpenStatus cal_backend_open (CalBackend *backend, const char *uristr,
				       gboolean only_if_exists);

gboolean cal_backend_is_loaded (CalBackend *backend);

gboolean cal_backend_is_read_only (CalBackend *backend);

Query *cal_backend_get_query (CalBackend *backend,
			      GNOME_Evolution_Calendar_QueryListener ql,
			      const char *sexp);

CalMode cal_backend_get_mode (CalBackend *backend);
void cal_backend_set_mode (CalBackend *backend, CalMode mode);

int cal_backend_get_n_objects (CalBackend *backend, CalObjType type);

char *cal_backend_get_default_object (CalBackend *backend, CalObjType type);

char *cal_backend_get_object (CalBackend *backend, const char *uid);

CalComponent *cal_backend_get_object_component (CalBackend *backend, const char *uid);

gboolean cal_backend_set_default_timezone (CalBackend *backend, const char *tzid);

char *cal_backend_get_timezone_object (CalBackend *backend, const char *tzid);

CalObjType cal_backend_get_type_by_uid (CalBackend *backend, const char *uid);

GList *cal_backend_get_uids (CalBackend *backend, CalObjType type);

GList *cal_backend_get_objects_in_range (CalBackend *backend, CalObjType type,
					 time_t start, time_t end);

GList *cal_backend_get_free_busy (CalBackend *backend, GList *users, time_t start, time_t end);

GNOME_Evolution_Calendar_CalObjChangeSeq * cal_backend_get_changes (
	CalBackend *backend, CalObjType type, const char *change_id);

GNOME_Evolution_Calendar_CalComponentAlarmsSeq *cal_backend_get_alarms_in_range (
	CalBackend *backend, time_t start, time_t end, gboolean *valid_range);

GNOME_Evolution_Calendar_CalComponentAlarms *cal_backend_get_alarms_for_object (
	CalBackend *backend, const char *uid,
	time_t start, time_t end,
	CalBackendGetAlarmsForObjectResult *result);


CalBackendResult cal_backend_update_objects (CalBackend *backend, const char *calobj, CalObjModType mod);

CalBackendResult cal_backend_remove_object (CalBackend *backend, const char *uid, CalObjModType mod);

CalBackendSendResult cal_backend_send_object (CalBackend *backend, const char *calobj, char **new_calobj,
					      GNOME_Evolution_Calendar_UserList **user_list, 
					      char error_msg[256]);

icaltimezone* cal_backend_get_timezone (CalBackend *backend, const char *tzid);
icaltimezone* cal_backend_get_default_timezone (CalBackend *backend);

void cal_backend_last_client_gone (CalBackend *backend);
void cal_backend_opened (CalBackend *backend, CalBackendOpenStatus status);
void cal_backend_obj_updated (CalBackend *backend, const char *uid);
void cal_backend_obj_removed (CalBackend *backend, const char *uid);



END_GNOME_DECLS

#endif
