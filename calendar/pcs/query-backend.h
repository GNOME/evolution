/* Evolution calendar - Backend cache for calendar queries.
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

#ifndef QUERY_BACKEND_H
#define QUERY_BACKEND_H

#include "cal-backend.h"
#include "query.h"

BEGIN_GNOME_DECLS

#define QUERY_BACKEND_TYPE            (query_backend_get_type ())
#define QUERY_BACKEND(obj)            (GTK_CHECK_CAST ((obj), QUERY_BACKEND_TYPE, QueryBackend))
#define QUERY_BACKEND_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), QUERY_BACKEND_TYPE, QueryBackendClass))
#define IS_QUERY_BACKEND(obj)         (GTK_CHECK_TYPE ((obj), QUERY_BACKEND_TYPE))
#define IS_QUERY_BACKEND_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), QUERY_BACKEND_TYPE))

typedef struct _QueryBackendPrivate QueryBackendPrivate;

typedef struct {
	GtkObject object;

	/* Private data */
	QueryBackendPrivate *priv;
} QueryBackend;

typedef struct {
	GtkObjectClass parent;
} QueryBackendClass;

GtkType       query_backend_get_type (void);
QueryBackend *query_backend_new (Query *query, CalBackend *backend);

END_GNOME_DECLS

#endif
