/* Evolution calendar client
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

#ifndef CAL_CLIENT_H
#define CAL_CLIENT_H

#include <glib-object.h>
#include <cal-util/cal-recur.h>
#include <cal-util/cal-util.h>
#include <cal-client/cal-query.h>
#include "cal-client-types.h"

G_BEGIN_DECLS



#define CAL_CLIENT_TYPE            (cal_client_get_type ())
#define CAL_CLIENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAL_CLIENT_TYPE, CalClient))
#define CAL_CLIENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CAL_CLIENT_TYPE, CalClientClass))
#define IS_CAL_CLIENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAL_CLIENT_TYPE))
#define IS_CAL_CLIENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CAL_CLIENT_TYPE))

#define CAL_CLIENT_OPEN_STATUS_ENUM_TYPE     (cal_client_open_status_enum_get_type ())
#define CAL_CLIENT_REMOVE_STATUS_ENUM_TYPE   (cal_client_remove_status_enum_get_type ())
#define CAL_CLIENT_SET_MODE_STATUS_ENUM_TYPE (cal_client_set_mode_status_enum_get_type ())
#define CAL_MODE_ENUM_TYPE                   (cal_mode_enum_get_type ())

typedef struct _CalClientClass CalClientClass;

typedef struct _CalClientPrivate CalClientPrivate;

/* Open status for the cal_opened signal */
typedef enum {
	CAL_CLIENT_OPEN_SUCCESS,
	CAL_CLIENT_OPEN_ERROR,
	CAL_CLIENT_OPEN_NOT_FOUND,
	CAL_CLIENT_OPEN_PERMISSION_DENIED,
	CAL_CLIENT_OPEN_METHOD_NOT_SUPPORTED
} CalClientOpenStatus;

/* Set mode status for the cal_client_set_mode function */
typedef enum {
	CAL_CLIENT_SET_MODE_SUCCESS,
	CAL_CLIENT_SET_MODE_ERROR,
	CAL_CLIENT_SET_MODE_NOT_SUPPORTED
} CalClientSetModeStatus;

/* Get status for the cal_client_get_object() function */
typedef enum {
	CAL_CLIENT_GET_SUCCESS,
	CAL_CLIENT_GET_NOT_FOUND,
	CAL_CLIENT_GET_SYNTAX_ERROR
} CalClientGetStatus;

/* Status for update_object(s) and remove_object */
typedef enum {
	CAL_CLIENT_RESULT_SUCCESS,
	CAL_CLIENT_RESULT_CORBA_ERROR,
	CAL_CLIENT_RESULT_INVALID_OBJECT,
	CAL_CLIENT_RESULT_NOT_FOUND,
	CAL_CLIENT_RESULT_PERMISSION_DENIED
} CalClientResult;

/* Whether the client is not loaded, is being loaded, or is already loaded */
typedef enum {
	CAL_CLIENT_LOAD_NOT_LOADED,
	CAL_CLIENT_LOAD_LOADING,
	CAL_CLIENT_LOAD_LOADED
} CalClientLoadState;

struct _CalClient {
	GObject object;

	/* Private data */
	CalClientPrivate *priv;
};

struct _CalClientClass {
	GObjectClass parent_class;

	/* Notification signals */

	void (* cal_opened) (CalClient *client, CalClientOpenStatus status);
	void (* cal_set_mode) (CalClient *client, CalClientSetModeStatus status, CalMode mode);	

	void (* backend_error) (CalClient *client, const char *message);

	void (* categories_changed) (CalClient *client, GPtrArray *categories);

	void (* forget_password) (CalClient *client, const char *key);

	void (* backend_died) (CalClient *client);
};

typedef gchar * (* CalClientAuthFunc) (CalClient *client,
                                      const gchar *prompt,
                                      const gchar *key,
                                      gpointer user_data);

GType cal_client_get_type (void);

GType cal_client_open_status_enum_get_type (void);
GType cal_client_set_mode_status_enum_get_type (void);
GType cal_mode_enum_get_type (void);

CalClient *cal_client_new (const char *uri, CalObjType type);

void cal_client_set_auth_func (CalClient *client, CalClientAuthFunc func, gpointer data);

gboolean cal_client_open (CalClient *client, gboolean only_if_exists, GError **error);
void cal_client_open_async (CalClient *client, gboolean only_if_exists);
gboolean cal_client_remove_calendar (CalClient *client, GError **error);

GList *cal_client_uri_list (CalClient *client, CalMode mode);

CalClientLoadState cal_client_get_load_state (CalClient *client);

const char *cal_client_get_uri (CalClient *client);

gboolean cal_client_is_read_only (CalClient *client, gboolean *read_only, GError **error);
gboolean cal_client_get_cal_address (CalClient *client, char **cal_address, GError **error);
gboolean cal_client_get_alarm_email_address (CalClient *client, char **alarm_address, GError **error);
gboolean cal_client_get_ldap_attribute (CalClient *client, char **ldap_attribute, GError **error);

gboolean cal_client_get_one_alarm_only (CalClient *client);
gboolean cal_client_get_organizer_must_attend (CalClient *client);
gboolean cal_client_get_save_schedules (CalClient *client);
gboolean cal_client_get_static_capability (CalClient *client, const char *cap);

gboolean cal_client_set_mode (CalClient *client, CalMode mode);

gboolean cal_client_get_default_object (CalClient *client,
					icalcomponent **icalcomp, GError **error);

gboolean cal_client_get_object (CalClient *client,
				const char *uid,
				const char *rid,
				icalcomponent **icalcomp,
				GError **error);

gboolean cal_client_get_changes (CalClient *client, CalObjType type, const char *change_id, GList **changes, GError **error);

gboolean cal_client_get_object_list (CalClient *client, const char *query, GList **objects, GError **error);
gboolean cal_client_get_object_list_as_comp (CalClient *client, const char *query, GList **objects, GError **error);
void cal_client_free_object_list (GList *objects);

gboolean cal_client_get_free_busy (CalClient *client, GList *users, time_t start, time_t end, 
				   GList **freebusy, GError **error);

void cal_client_generate_instances (CalClient *client, CalObjType type,
				    time_t start, time_t end,
				    CalRecurInstanceFn cb, gpointer cb_data);

GSList *cal_client_get_alarms_in_range (CalClient *client, time_t start, time_t end);

void cal_client_free_alarms (GSList *comp_alarms);

gboolean cal_client_get_alarms_for_object (CalClient *client, const char *uid,
					   time_t start, time_t end,
					   CalComponentAlarms **alarms);

gboolean cal_client_create_object (CalClient *client, icalcomponent *icalcomp, char **uid, GError **error);
gboolean cal_client_modify_object (CalClient *client, icalcomponent *icalcomp, CalObjModType mod, GError **error);
gboolean cal_client_remove_object (CalClient *client, const char *uid, GError **error);
gboolean cal_client_remove_object_with_mod (CalClient *client, const char *uid, const char *rid, CalObjModType mod, GError **error);

gboolean cal_client_discard_alarm (CalClient *client, CalComponent *comp, const char *auid, GError **error);

gboolean cal_client_receive_objects (CalClient *client, icalcomponent *icalcomp, GError **error);
gboolean cal_client_send_objects (CalClient *client, icalcomponent *icalcomp, GError **error);

gboolean cal_client_get_timezone (CalClient *client, const char *tzid, icaltimezone **zone, GError **error);
gboolean cal_client_add_timezone (CalClient *client, icaltimezone *izone, GError **error);
/* Sets the default timezone to use to resolve DATE and floating DATE-TIME
   values. This will typically be from the user's timezone setting. Call this
   before using any other functions. It will pass the default timezone on to
   the server. Returns TRUE on success. */
gboolean cal_client_set_default_timezone (CalClient *client, icaltimezone *zone, GError **error);

gboolean cal_client_get_query (CalClient *client, const char *sexp, CalQuery **query, GError **error);

/* Resolves TZIDs for the recurrence generator. */
icaltimezone *cal_client_resolve_tzid_cb (const char *tzid, gpointer data);

/* Returns a complete VCALENDAR for a VEVENT/VTODO including all VTIMEZONEs
   used by the component. It also includes a 'METHOD:PUBLISH' property. */
char* cal_client_get_component_as_string (CalClient *client, icalcomponent *icalcomp);

const char * cal_client_get_error_message (ECalendarStatus status);



G_END_DECLS

#endif
