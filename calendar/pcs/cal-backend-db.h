/* Evolution calendar - iCalendar DB backend
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Author: Rodrigo Moya <rodrigo@ximian.com>
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

#ifndef CAL_BACKEND_DB_H
#define CAL_BACKEND_DB_H

#include "cal-backend.h"

BEGIN_GNOME_DECLS

#define CAL_BACKEND_DB_TYPE            (cal_backend_db_get_type ())
#define CAL_BACKEND_DB(obj)            (GTK_CHECK_CAST ((obj), CAL_BACKEND_DB_TYPE,		\
					  CalBackendDB))
#define CAL_BACKEND_DB_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), CAL_BACKEND_DB_TYPE,	\
					  CalBackendDBClass))
#define IS_CAL_BACKEND_DB(obj)         (GTK_CHECK_TYPE ((obj), CAL_BACKEND_DB_TYPE))
#define IS_CAL_BACKEND_DB_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), CAL_BACKEND_DB_TYPE))

typedef struct _CalBackendDB CalBackendDB;
typedef struct _CalBackendDBClass CalBackendDBClass;

typedef struct _CalBackendDBPrivate CalBackendDBPrivate;

struct _CalBackendDB {
	CalBackend backend;

	/* Private data */
	CalBackendDBPrivate *priv;
};

struct _CalBackendDBClass {
	CalBackendClass parent_class;
};

GtkType cal_backend_db_get_type (void);

END_GNOME_DECLS

#endif
