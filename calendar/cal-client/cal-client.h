/* Evolution calendar client
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

#ifndef CAL_CLIENT_H
#define CAL_CLIENT_H

#include <libgnome/gnome-defs.h>
#include <gtk/gtkobject.h>
#include <cal-util/cal-recur.h>
#include <cal-util/cal-util.h>

BEGIN_GNOME_DECLS



#define CAL_CLIENT_TYPE            (cal_client_get_type ())
#define CAL_CLIENT(obj)            (GTK_CHECK_CAST ((obj), CAL_CLIENT_TYPE, CalClient))
#define CAL_CLIENT_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), CAL_CLIENT_TYPE, CalClientClass))
#define IS_CAL_CLIENT(obj)         (GTK_CHECK_TYPE ((obj), CAL_CLIENT_TYPE))
#define IS_CAL_CLIENT_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), CAL_CLIENT_TYPE))

typedef struct _CalClient CalClient;
typedef struct _CalClientClass CalClientClass;

typedef struct _CalClientPrivate CalClientPrivate;

/* Load status for the cal_loaded signal */
typedef enum {
	CAL_CLIENT_LOAD_SUCCESS,
	CAL_CLIENT_LOAD_ERROR,
	CAL_CLIENT_LOAD_IN_USE,
	CAL_CLIENT_LOAD_METHOD_NOT_SUPPORTED
} CalClientLoadStatus;

typedef enum {
	CAL_CLIENT_GET_SUCCESS,
	CAL_CLIENT_GET_NOT_FOUND,
	CAL_CLIENT_GET_SYNTAX_ERROR
} CalClientGetStatus;


struct _CalClient {
	GtkObject object;

	/* Private data */
	CalClientPrivate *priv;
};

struct _CalClientClass {
	GtkObjectClass parent_class;

	/* Notification signals */

	void (* cal_loaded) (CalClient *client, CalClientLoadStatus status);

	void (* obj_updated) (CalClient *client, const char *uid);
	void (* obj_removed) (CalClient *client, const char *uid);
};

GtkType cal_client_get_type (void);

CalClient *cal_client_construct (CalClient *client);

CalClient *cal_client_new (void);

gboolean cal_client_load_calendar (CalClient *client, const char *str_uri);
gboolean cal_client_create_calendar (CalClient *client, const char *str_uri);

gboolean cal_client_is_loaded (CalClient *client);

int cal_client_get_n_objects (CalClient *client, CalObjType type);

CalClientGetStatus cal_client_get_object (CalClient *client,
					  const char *uid,
					  CalComponent **comp);

CalClientGetStatus cal_client_get_uid_by_pilot_id (CalClient *client,
						   unsigned long pilot_id,
						   char **uid);

void cal_client_update_pilot_id (CalClient *client, char *uid,
				 unsigned long pilot_id,
				 unsigned long pilot_status);

GList *cal_client_get_uids (CalClient *client, CalObjType type);

GList *cal_client_get_objects_in_range (CalClient *client, CalObjType type,
					time_t start, time_t end);

void cal_client_generate_instances (CalClient *client, CalObjType type,
				    time_t start, time_t end,
				    CalRecurInstanceFn cb, gpointer cb_data);

GList *cal_client_get_alarms_in_range (CalClient *client, time_t start, time_t end);

gboolean cal_client_get_alarms_for_object (CalClient *client, const char *uid,
					   time_t start, time_t end,
					   GList **alarms);

gboolean cal_client_update_object (CalClient *client, CalComponent *comp);

gboolean cal_client_remove_object (CalClient *client, const char *uid);



END_GNOME_DECLS

#endif
