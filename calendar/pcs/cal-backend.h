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

#include <e-util/e-list.h>
#include <cal-util/cal-util.h>
#include <cal-util/cal-component.h>
#include "pcs/evolution-calendar.h"
#include "pcs/cal-common.h"
#include "pcs/cal.h"
#include "pcs/query.h"

G_BEGIN_DECLS



#define CAL_BACKEND_TYPE            (cal_backend_get_type ())
#define CAL_BACKEND(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAL_BACKEND_TYPE, CalBackend))
#define CAL_BACKEND_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CAL_BACKEND_TYPE,		\
				     CalBackendClass))
#define IS_CAL_BACKEND(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAL_BACKEND_TYPE))
#define IS_CAL_BACKEND_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CAL_BACKEND_TYPE))

typedef struct _CalBackendPrivate CalBackendPrivate;

struct _CalBackend {
	GObject object;

	CalBackendPrivate *priv;
};

struct _CalBackendClass {
	GObjectClass parent_class;

	/* Notification signals */
	void (* last_client_gone) (CalBackend *backend);
	void (* cal_added) (CalBackend *backend, Cal *cal);

	gboolean (* is_loaded) (CalBackend *backend);

	/* FIXME What to pass back here */
	void (* opened) (CalBackend *backend, int status);
	void (* removed) (CalBackend *backend, int status);
	void (* obj_updated) (CalBackend *backend, const char *uid);

	/* Virtual methods */
	void (* is_read_only) (CalBackend *backend, Cal *cal);
	void (* get_cal_address) (CalBackend *backend, Cal *cal);
	void (* get_alarm_email_address) (CalBackend *backend, Cal *cal);
	void (* get_ldap_attribute) (CalBackend *backend, Cal *cal);
	void (* get_static_capabilities) (CalBackend *backend, Cal *cal);
	
	void (* open) (CalBackend *backend, Cal *cal, gboolean only_if_exists);
	void (* remove) (CalBackend *backend, Cal *cal);

	/* Object related virtual methods */
	void (* create_object) (CalBackend *backend, Cal *cal, const char *calobj);
	void (* modify_object) (CalBackend *backend, Cal *cal, const char *calobj, CalObjModType mod);
	void (* remove_object) (CalBackend *backend, Cal *cal, const char *uid, const char *rid, CalObjModType mod);

	void (* discard_alarm) (CalBackend *backend, Cal *cal, const char *uid, const char *auid);

	void (* receive_objects) (CalBackend *backend, Cal *cal, const char *calobj);
	void (* send_objects) (CalBackend *backend, Cal *cal, const char *calobj);

	void (* get_default_object) (CalBackend *backend, Cal *cal);
	void (* get_object) (CalBackend *backend, Cal *cal, const char *uid, const char *rid);	
	void (* get_object_list) (CalBackend *backend, Cal *cal, const char *sexp);

	/* Timezone related virtual methods */
	void (* get_timezone) (CalBackend *backend, Cal *cal, const char *tzid);
	void (* add_timezone) (CalBackend *backend, Cal *cal, const char *object);
	void (* set_default_timezone) (CalBackend *backend, Cal *cal, const char *tzid);

	void (* start_query) (CalBackend *backend, Query *query);

	/* Mode relate virtual methods */
	CalMode (* get_mode) (CalBackend *backend);
	void    (* set_mode) (CalBackend *backend, CalMode mode);

	void (* get_free_busy) (CalBackend *backend, Cal *cal, GList *users, time_t start, time_t end);
	void (* get_changes) (CalBackend *backend, Cal *cal, CalObjType type, const char *change_id);

	/* Internal methods for use only in the pcs */
	icaltimezone *(* internal_get_default_timezone) (CalBackend *backend);
	icaltimezone *(* internal_get_timezone) (CalBackend *backend, const char *tzid);
};

GType cal_backend_get_type (void);

const char *cal_backend_get_uri (CalBackend *backend);
icalcomponent_kind cal_backend_get_kind (CalBackend *backend);

void cal_backend_add_client (CalBackend *backend, Cal *cal);
void cal_backend_remove_client (CalBackend *backend, Cal *cal);

void cal_backend_add_query (CalBackend *backend, Query *query);
EList *cal_backend_get_queries (CalBackend *backend);

void cal_backend_is_read_only (CalBackend *backend, Cal *cal);
void cal_backend_get_cal_address (CalBackend *backend, Cal *cal);
void cal_backend_get_alarm_email_address (CalBackend *backend, Cal *cal);
void cal_backend_get_ldap_attribute (CalBackend *backend, Cal *cal);
void cal_backend_get_static_capabilities (CalBackend *backend, Cal *cal);

void cal_backend_open (CalBackend *backend, Cal *cal, gboolean only_if_exists);
void cal_backend_remove (CalBackend *backend, Cal *cal);

void cal_backend_create_object (CalBackend *backend, Cal *cal, const char *calobj);
void cal_backend_modify_object (CalBackend *backend, Cal *cal, const char *calobj, CalObjModType mod);
void cal_backend_remove_object (CalBackend *backend, Cal *cal, const char *uid, const char *rid, CalObjModType mod);

void cal_backend_discard_alarm (CalBackend *backend, Cal *cal, const char *uid, const char *auid);

void cal_backend_receive_objects (CalBackend *backend, Cal *cal, const char *calobj);
void cal_backend_send_objects (CalBackend *backend, Cal *cal, const char *calobj);

void cal_backend_get_default_object (CalBackend *backend, Cal *cal);
void cal_backend_get_object (CalBackend *backend, Cal *cal, const char *uid, const char *rid);
void cal_backend_get_object_list (CalBackend *backend, Cal *cal, const char *sexp);

gboolean cal_backend_is_loaded (CalBackend *backend);

void cal_backend_start_query (CalBackend *backend, Query *query);

CalMode cal_backend_get_mode (CalBackend *backend);
void cal_backend_set_mode (CalBackend *backend, CalMode mode);

void cal_backend_get_timezone (CalBackend *backend, Cal *cal, const char *tzid);
void cal_backend_add_timezone (CalBackend *backend, Cal *cal, const char *object);
void cal_backend_set_default_timezone (CalBackend *backend, Cal *cal, const char *tzid);

void cal_backend_get_changes (CalBackend *backend, Cal *cal, CalObjType type, const char *change_id);
void cal_backend_get_free_busy (CalBackend *backend, Cal *cal, GList *users, time_t start, time_t end);

icaltimezone* cal_backend_internal_get_default_timezone (CalBackend *backend);
icaltimezone* cal_backend_internal_get_timezone (CalBackend *backend, const char *tzid);

void cal_backend_last_client_gone (CalBackend *backend);

void cal_backend_notify_mode      (CalBackend *backend,
				   GNOME_Evolution_Calendar_Listener_SetModeStatus status, 
				   GNOME_Evolution_Calendar_CalMode mode);
void cal_backend_notify_error     (CalBackend *backend, const char *message);
void cal_backend_ref_categories   (CalBackend *backend, GSList *categories);
void cal_backend_unref_categories (CalBackend *backend, GSList *categories);



G_END_DECLS

#endif
