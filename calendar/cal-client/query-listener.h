/* Evolution calendar - Live search query listener implementation
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

#ifndef QUERY_LISTENER_H
#define QUERY_LISTENER_H

#include <bonobo/bonobo-xobject.h>
#include "evolution-calendar.h"

BEGIN_GNOME_DECLS



#define QUERY_LISTENER_TYPE            (query_listener_get_type ())
#define QUERY_LISTENER(obj)            (GTK_CHECK_CAST ((obj), QUERY_LISTENER_TYPE, QueryListener))
#define QUERY_LISTENER_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), QUERY_LISTENER_TYPE,	\
					QueryListenerClass))
#define IS_QUERY_LISTENER(obj)         (GTK_CHECK_TYPE ((obj), QUERY_LISTENER_TYPE))
#define IS_QUERY_LISTENER_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), QUERY_LISTENER_TYPE))

typedef struct _QueryListenerPrivate QueryListenerPrivate;

typedef struct {
	BonoboXObject xobject;

	/* Private data */
	QueryListenerPrivate *priv;
} QueryListener;

typedef struct {
	BonoboXObjectClass parent_class;

	POA_GNOME_Evolution_Calendar_QueryListener__epv epv;
} QueryListenerClass;

/* Notification functions */

typedef void (* QueryListenerObjUpdatedFn) (QueryListener *ql,
					    const GNOME_Evolution_Calendar_CalObjUID uid,
					    CORBA_boolean query_in_progress,
					    CORBA_long n_scanned,
					    CORBA_long total,
					    gpointer data);

typedef void (* QueryListenerObjRemovedFn) (QueryListener *ql,
					    const GNOME_Evolution_Calendar_CalObjUID uid,
					    gpointer data);

typedef void (* QueryListenerQueryDoneFn) (
	QueryListener *ql,
	GNOME_Evolution_Calendar_QueryListener_QueryDoneStatus status,
	const CORBA_char *error_str,
	gpointer data);

typedef void (* QueryListenerEvalErrorFn) (QueryListener *ql,
					   const CORBA_char *error_str,
					   gpointer data);

GtkType query_listener_get_type (void);

QueryListener *query_listener_construct (QueryListener *ql,
					 QueryListenerObjUpdatedFn obj_updated_fn,
					 QueryListenerObjRemovedFn obj_removed_fn,
					 QueryListenerQueryDoneFn query_done_fn,
					 QueryListenerEvalErrorFn eval_error_fn,
					 gpointer fn_data);

QueryListener *query_listener_new (QueryListenerObjUpdatedFn obj_updated_fn,
				   QueryListenerObjRemovedFn obj_removed_fn,
				   QueryListenerQueryDoneFn query_done_fn,
				   QueryListenerEvalErrorFn eval_error_fn,
				   gpointer fn_data);



END_GNOME_DECLS

#endif
