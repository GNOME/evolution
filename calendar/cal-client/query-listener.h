/* Evolution calendar - Live search query listener implementation
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

#ifndef QUERY_LISTENER_H
#define QUERY_LISTENER_H

#include <bonobo/bonobo-object.h>
#include "cal-client-types.h"
#include "evolution-calendar.h"

G_BEGIN_DECLS



#define QUERY_LISTENER_TYPE            (query_listener_get_type ())
#define QUERY_LISTENER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), QUERY_LISTENER_TYPE, QueryListener))
#define QUERY_LISTENER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), QUERY_LISTENER_TYPE,	\
					QueryListenerClass))
#define IS_QUERY_LISTENER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), QUERY_LISTENER_TYPE))
#define IS_QUERY_LISTENER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), QUERY_LISTENER_TYPE))

typedef struct _QueryListenerPrivate QueryListenerPrivate;

typedef struct {
	BonoboObject xobject;

	/* Private data */
	QueryListenerPrivate *priv;
} QueryListener;

typedef struct {
	BonoboObjectClass parent_class;

	POA_GNOME_Evolution_Calendar_QueryListener__epv epv;

	void (*objects_added) (QueryListener *listener, GList *objects);
	void (*objects_modified) (QueryListener *listener, GList *objects);
	void (*objects_removed) (QueryListener *listener, GList *uids);
	void (*query_progress) (QueryListener *listener, const char *message, int percent);
	void (*query_done) (QueryListener *listener, ECalendarStatus status);
} QueryListenerClass;

/* Notification functions */

GType query_listener_get_type (void);
QueryListener *query_listener_new (void);



G_END_DECLS

#endif
