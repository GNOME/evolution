/* Evolution calendar - Live query client object
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

#ifndef CAL_QUERY_H
#define CAL_QUERY_H

#include <glib-object.h>
#include "cal-client-types.h"
#include "query-listener.h"
#include "evolution-calendar.h"

G_BEGIN_DECLS

typedef struct _CalClient CalClient;



#define CAL_QUERY_TYPE            (cal_query_get_type ())
#define CAL_QUERY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAL_QUERY_TYPE, CalQuery))
#define CAL_QUERY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CAL_QUERY_TYPE, CalQueryClass))
#define IS_CAL_QUERY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAL_QUERY_TYPE))
#define IS_CAL_QUERY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CAL_QUERY_TYPE))

typedef struct _CalQueryPrivate CalQueryPrivate;

typedef struct {
	GObject object;

	/* Private data */
	CalQueryPrivate *priv;
} CalQuery;

typedef struct {
	GObjectClass parent_class;

	/* Notification signals */
	void (* objects_added) (CalQuery *query, GList *objects);
	void (* objects_modified) (CalQuery *query, GList *objects);
	void (* objects_removed) (CalQuery *query, GList *uids);
	void (* query_progress) (CalQuery *query, char *message, int percent);
	void (* query_done) (CalQuery *query, ECalendarStatus status);
} CalQueryClass;

GType      cal_query_get_type (void);

CalQuery *cal_query_new (GNOME_Evolution_Calendar_Query corba_query, QueryListener *listener, CalClient *client);
CalClient *cal_query_get_client (CalQuery *query);
void cal_query_start (CalQuery *query);

G_END_DECLS

#endif
