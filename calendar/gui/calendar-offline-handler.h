/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* calendar-offline-handler.h
 *
 * Copyright (C) 2001  Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#ifndef _CALENDAR_OFFLINE_HANDLER_H_
#define _CALENDAR_OFFLINE_HANDLER_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <bonobo/bonobo-object.h>
#include "Evolution.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define CALENDAR_TYPE_OFFLINE_HANDLER			(calendar_offline_handler_get_type ())
#define CALENDAR_OFFLINE_HANDLER(obj)			(GTK_CHECK_CAST ((obj), CALENDAR_TYPE_OFFLINE_HANDLER, CalendarOfflineHandler))
#define CALENDAR_OFFLINE_HANDLER_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), CALENDAR_TYPE_OFFLINE_HANDLER, CalendarOfflineHandlerClass))
#define CALENDAR_IS_OFFLINE_HANDLER(obj)			(GTK_CHECK_TYPE ((obj), CALENDAR_TYPE_OFFLINE_HANDLER))
#define CALENDAR_IS_OFFLINE_HANDLER_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), CALENDAR_TYPE_OFFLINE_HANDLER))


typedef struct _CalendarOfflineHandler        CalendarOfflineHandler;
typedef struct _CalendarOfflineHandlerPrivate CalendarOfflineHandlerPrivate;
typedef struct _CalendarOfflineHandlerClass   CalendarOfflineHandlerClass;

struct _CalendarOfflineHandler {
	BonoboObject parent;

	CalendarOfflineHandlerPrivate *priv;
};

struct _CalendarOfflineHandlerClass {
	BonoboObjectClass parent_class;

	POA_GNOME_Evolution_Offline__epv epv;
};


GtkType             calendar_offline_handler_get_type  (void);
CalendarOfflineHandler *calendar_offline_handler_new       (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _CALENDAR_OFFLINE_HANDLER_H_ */
