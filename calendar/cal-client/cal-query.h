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

#include "evolution-calendar.h"

G_BEGIN_DECLS

typedef struct _CalClient CalClient;



#define CAL_QUERY_TYPE            (cal_query_get_type ())
#define CAL_QUERY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CAL_QUERY_TYPE, CalQuery))
#define CAL_QUERY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CAL_QUERY_TYPE, CalQueryClass))
#define IS_CAL_QUERY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CAL_QUERY_TYPE))
#define IS_CAL_QUERY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CAL_QUERY_TYPE))

#define CAL_QUERY_DONE_STATUS_ENUM_TYPE (cal_query_done_status_enum_get_type ())

/* Status values when a query terminates */
typedef enum {
	CAL_QUERY_DONE_SUCCESS,
	CAL_QUERY_DONE_PARSE_ERROR
} CalQueryDoneStatus;

typedef struct _CalQueryPrivate CalQueryPrivate;

typedef struct {
	GObject object;

	/* Private data */
	CalQueryPrivate *priv;
} CalQuery;

typedef struct {
	GObjectClass parent_class;

	/* Notification signals */

	void (* obj_updated) (CalQuery *query, const char *uid,
			      gboolean query_in_progress, int n_scanned, int total);
	void (* obj_removed) (CalQuery *query, const char *uid);

	void (* query_done) (CalQuery *query, CalQueryDoneStatus status, const char *error_str);

	void (* eval_error) (CalQuery *query, const char *error_str);
} CalQueryClass;

GType      cal_query_get_type (void);

GType      cal_query_done_status_enum_get_type (void);

CalQuery  *cal_query_construct (CalQuery *query,
				GNOME_Evolution_Calendar_Cal cal,
				const char *sexp);

CalQuery  *cal_query_new (CalClient *client,
			  GNOME_Evolution_Calendar_Cal cal,
			  const char *sexp);
CalClient *cal_query_get_client (CalQuery *query);



G_END_DECLS

#endif
