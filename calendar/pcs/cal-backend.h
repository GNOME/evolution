/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Evolution calendar - generic backend class
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

#ifndef CAL_BACKEND_H
#define CAL_BACKEND_H

#include <libgnome/gnome-defs.h>
#include <libgnomevfs/gnome-vfs.h>
#include <cal-util/cal-util.h>
#include <cal-util/cal-component.h>
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

/* Open status values */
typedef enum {
	CAL_BACKEND_OPEN_SUCCESS,	/* Loading OK */
	CAL_BACKEND_OPEN_ERROR,		/* We need better error reporting in libversit */
	CAL_BACKEND_OPEN_NOT_FOUND
} CalBackendOpenStatus;

/* Result codes for ::get_alarms_in_range() */
typedef enum {
	CAL_BACKEND_GET_ALARMS_SUCCESS,
	CAL_BACKEND_GET_ALARMS_NOT_FOUND,
	CAL_BACKEND_GET_ALARMS_INVALID_RANGE
} CalBackendGetAlarmsForObjectResult;

struct _CalBackend {
	GtkObject object;
};

struct _CalBackendClass {
	GtkObjectClass parent_class;

	/* Notification signals */
	void (* last_client_gone) (CalBackend *backend);

	void (* opened) (CalBackend *backend, CalBackendOpenStatus status);
	void (* obj_updated) (CalBackend *backend, const char *uid);
	void (* obj_removed) (CalBackend *backend, const char *uid);

	/* Virtual methods */
	GnomeVFSURI *(* get_uri) (CalBackend *backend);
	void (* add_cal) (CalBackend *backend, Cal *cal);

	CalBackendOpenStatus (* open) (CalBackend *backend, GnomeVFSURI *uri,
				       gboolean only_if_exists);

	gboolean (* is_loaded) (CalBackend *backend);

	/* General object acquirement and information related virtual methods */
	int (* get_n_objects) (CalBackend *backend, CalObjType type);
	char *(* get_object) (CalBackend *backend, const char *uid);
	CalObjType(* get_type_by_uid) (CalBackend *backend, const char *uid);
	GList *(* get_uids) (CalBackend *backend, CalObjType type);

	GList *(* get_objects_in_range) (CalBackend *backend, CalObjType type,
					 time_t start, time_t end);
	char *(* get_free_busy) (CalBackend *backend, time_t start, time_t end);

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
	gboolean (* update_object) (CalBackend *backend, const char *uid, const char *calobj);
	gboolean (* remove_object) (CalBackend *backend, const char *uid);
};

GtkType cal_backend_get_type (void);

GnomeVFSURI *cal_backend_get_uri (CalBackend *backend);

void cal_backend_add_cal (CalBackend *backend, Cal *cal);

CalBackendOpenStatus cal_backend_open (CalBackend *backend, GnomeVFSURI *uri,
				       gboolean only_if_exists);

gboolean cal_backend_is_loaded (CalBackend *backend);

int cal_backend_get_n_objects (CalBackend *backend, CalObjType type);

char *cal_backend_get_object (CalBackend *backend, const char *uid);

GList *cal_backend_get_uids (CalBackend *backend, CalObjType type);

GList *cal_backend_get_objects_in_range (CalBackend *backend, CalObjType type,
					 time_t start, time_t end);

char *cal_backend_get_free_busy (CalBackend *backend, time_t start, time_t end);

GNOME_Evolution_Calendar_CalObjChangeSeq * cal_backend_get_changes (
	CalBackend *backend, CalObjType type, const char *change_id);

GNOME_Evolution_Calendar_CalComponentAlarmsSeq *cal_backend_get_alarms_in_range (
	CalBackend *backend, time_t start, time_t end, gboolean *valid_range);

GNOME_Evolution_Calendar_CalComponentAlarms *cal_backend_get_alarms_for_object (
	CalBackend *backend, const char *uid,
	time_t start, time_t end,
	CalBackendGetAlarmsForObjectResult *result);


gboolean cal_backend_update_object (CalBackend *backend, const char *uid, const char *calobj);

gboolean cal_backend_remove_object (CalBackend *backend, const char *uid);

void cal_backend_last_client_gone (CalBackend *backend);
void cal_backend_opened (CalBackend *backend, CalBackendOpenStatus status);
void cal_backend_obj_updated (CalBackend *backend, const char *uid);
void cal_backend_obj_removed (CalBackend *backend, const char *uid);



END_GNOME_DECLS

#endif
