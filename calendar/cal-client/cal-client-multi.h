/* Evolution calendar client
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Author: Rodrigo Moya <rodrigo@ximian.com>
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

#ifndef CAL_CLIENT_MULTI_H
#define CAL_CLIENT_MULTI_H

#include <cal-client/cal-client.h>

G_BEGIN_DECLS

#define CAL_CLIENT_MULTI_TYPE            (cal_client_multi_get_type ())
#define CAL_CLIENT_MULTI(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAL_CLIENT_MULTI_TYPE, CalClientMulti))
#define CAL_CLIENT_MULTI_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CAL_CLIENT_MULTI_TYPE, CalClientMultiClass))
#define IS_CAL_CLIENT_MULTI(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAL_CLIENT_MULTI_TYPE))
#define IS_CAL_CLIENT_MULTI_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CAL_CLIENT_MULTI_TYPE))

typedef struct _CalClientMulti        CalClientMulti;
typedef struct _CalClientMultiClass   CalClientMultiClass;
typedef struct _CalClientMultiPrivate CalClientMultiPrivate;

struct _CalClientMulti {
	GObject object;

	/* Private data */
	CalClientMultiPrivate *priv;
};

struct _CalClientMultiClass {
	GObjectClass parent_class;

	/* notification signals */
	void (* cal_opened) (CalClientMulti *multi, CalClient *client, CalClientOpenStatus status);

	void (* obj_updated) (CalClientMulti *multi, CalClient *client, const char *uid);
	void (* obj_removed) (CalClientMulti *multi, CalClient *client, const char *uid);

	void (* categories_changed) (CalClientMulti *multi, CalClient *client, GPtrArray *categories);

	void (* forget_password) (CalClientMulti *multi, CalClient *client, const char *key);
};

GType              cal_client_multi_get_type (void);

CalClientMulti    *cal_client_multi_new (void);

void               cal_client_multi_add_client (CalClientMulti *multi, CalClient *client);
void               cal_client_multi_set_auth_func (CalClientMulti *multi,
						   CalClientAuthFunc func,
						   gpointer user_data);

CalClient         *cal_client_multi_open_calendar (CalClientMulti *multi,
						   const char *str_uri,
						   gboolean only_if_exists);
CalClient         *cal_client_multi_get_client_for_uri (CalClientMulti *multi,
							const char *uri);

int                cal_client_multi_get_n_objects (CalClientMulti *multi, CalObjType type);
CalClientGetStatus cal_client_multi_get_object (CalClientMulti *multi,
						const char *uid,
						CalComponent **comp);
CalClientGetStatus cal_client_multi_get_timezone (CalClientMulti *multi,
						  const char *tzid,
						  icaltimezone **zone);
GList             *cal_client_multi_get_uids (CalClientMulti *multi, CalObjType type);
GList             *cal_client_multi_get_changes (CalClientMulti *multi,
						 CalObjType type,
						 const char *change_id);
GList             *cal_client_multi_get_objects_in_range (CalClientMulti *multi,
							  CalObjType type,
							  time_t start,
							  time_t end);
GList             *cal_client_multi_get_free_busy (CalClientMulti *multi,
						   GList *users,
						   time_t start,
						   time_t end);
void               cal_client_multi_generate_instances (CalClientMulti *multi,
							CalObjType type,
							time_t start,
							time_t end,
							CalRecurInstanceFn cb,
							gpointer cb_data);
GSList            *cal_client_multi_get_alarms_in_range (CalClientMulti *multi,
							 time_t start, time_t end);

G_END_DECLS

#endif
