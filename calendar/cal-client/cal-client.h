/* Evolution calendar client
 *
 * Copyright (C) 2001 Ximian, Inc.
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

#ifndef CAL_CLIENT_H
#define CAL_CLIENT_H

#include <libgnome/gnome-defs.h>
#include <gtk/gtkobject.h>
#include <cal-util/cal-recur.h>
#include <cal-util/cal-util.h>
#include <cal-client/cal-query.h>

BEGIN_GNOME_DECLS



#define CAL_CLIENT_TYPE            (cal_client_get_type ())
#define CAL_CLIENT(obj)            (GTK_CHECK_CAST ((obj), CAL_CLIENT_TYPE, CalClient))
#define CAL_CLIENT_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), CAL_CLIENT_TYPE, CalClientClass))
#define IS_CAL_CLIENT(obj)         (GTK_CHECK_TYPE ((obj), CAL_CLIENT_TYPE))
#define IS_CAL_CLIENT_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), CAL_CLIENT_TYPE))

typedef struct _CalClient CalClient;
typedef struct _CalClientClass CalClientClass;

typedef struct _CalClientPrivate CalClientPrivate;

/* Open status for the cal_opened signal */
typedef enum {
	CAL_CLIENT_OPEN_SUCCESS,
	CAL_CLIENT_OPEN_ERROR,
	CAL_CLIENT_OPEN_NOT_FOUND,
	CAL_CLIENT_OPEN_METHOD_NOT_SUPPORTED
} CalClientOpenStatus;

/* Get status for the cal_client_get_object() function */
typedef enum {
	CAL_CLIENT_GET_SUCCESS,
	CAL_CLIENT_GET_NOT_FOUND,
	CAL_CLIENT_GET_SYNTAX_ERROR
} CalClientGetStatus;

/* Whether the client is not loaded, is being loaded, or is already loaded */
typedef enum {
	CAL_CLIENT_LOAD_NOT_LOADED,
	CAL_CLIENT_LOAD_LOADING,
	CAL_CLIENT_LOAD_LOADED
} CalClientLoadState;

struct _CalClient {
	GtkObject object;

	/* Private data */
	CalClientPrivate *priv;
};

struct _CalClientClass {
	GtkObjectClass parent_class;

	/* Notification signals */

	void (* cal_opened) (CalClient *client, CalClientOpenStatus status);

	void (* obj_updated) (CalClient *client, const char *uid);
	void (* obj_removed) (CalClient *client, const char *uid);

	void (* forget_password) (CalClient *client, const char *key);
};

typedef gchar * (* CalClientAuthFunc) (CalClient *client,
                                      const gchar *prompt,
                                      const gchar *key,
                                      gpointer user_data);

GtkType cal_client_get_type (void);

CalClient *cal_client_construct (CalClient *client);

CalClient *cal_client_new (void);

void cal_client_set_auth_func (CalClient *client, CalClientAuthFunc func, gpointer data);

gboolean cal_client_open_calendar (CalClient *client, const char *str_uri, gboolean only_if_exists);

CalClientLoadState cal_client_get_load_state (CalClient *client);

const char *cal_client_get_uri (CalClient *client);

int cal_client_get_n_objects (CalClient *client, CalObjType type);

CalClientGetStatus cal_client_get_object (CalClient *client,
					  const char *uid,
					  CalComponent **comp);

GList *cal_client_get_uids (CalClient *client, CalObjType type);
GList *cal_client_get_changes (CalClient *client, CalObjType type, const char *change_id);

GList *cal_client_get_objects_in_range (CalClient *client, CalObjType type,
					time_t start, time_t end);

GList *cal_client_get_free_busy (CalClient *client, time_t start, time_t end);

void cal_client_generate_instances (CalClient *client, CalObjType type,
				    time_t start, time_t end,
				    CalRecurInstanceFn cb, gpointer cb_data);

GSList *cal_client_get_alarms_in_range (CalClient *client, time_t start, time_t end);

void cal_client_free_alarms (GSList *comp_alarms);

gboolean cal_client_get_alarms_for_object (CalClient *client, const char *uid,
					   time_t start, time_t end,
					   CalComponentAlarms **alarms);

gboolean cal_client_update_object (CalClient *client, CalComponent *comp);

gboolean cal_client_remove_object (CalClient *client, const char *uid);

CalQuery *cal_client_get_query (CalClient *client, const char *sexp);



END_GNOME_DECLS

#endif
