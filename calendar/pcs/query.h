/* Evolution calendar - Live search query implementation
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

#ifndef QUERY_H
#define QUERY_H

#include <bonobo/bonobo-object.h>
#include "pcs/cal-common.h"
#include "pcs/evolution-calendar.h"

G_BEGIN_DECLS



#define QUERY_TYPE            (query_get_type ())
#define QUERY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), QUERY_TYPE, Query))
#define QUERY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), QUERY_TYPE, QueryClass))
#define IS_QUERY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), QUERY_TYPE))
#define IS_QUERY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), QUERY_TYPE))

typedef struct _QueryPrivate QueryPrivate;

typedef struct {
	BonoboObject xobject;

	/* Private data */
	QueryPrivate *priv;
} Query;

typedef struct {
	BonoboObjectClass parent_class;

	POA_GNOME_Evolution_Calendar_Query__epv epv;
} QueryClass;

GType query_get_type (void);

Query *query_construct (Query *query,
			CalBackend *backend,
			GNOME_Evolution_Calendar_QueryListener ql,
			const char *sexp);

Query *query_new (CalBackend *backend,
		  GNOME_Evolution_Calendar_QueryListener ql,
		  const char *sexp);



G_END_DECLS

#endif
