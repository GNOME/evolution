/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-activity-client.h
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

#ifndef _EVOLUTION_ACTIVITY_CLIENT_H_
#define _EVOLUTION_ACTIVITY_CLIENT_H_

#include "evolution-shell-client.h"

#include "Evolution.h"

#include <gtk/gtkobject.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define EVOLUTION_TYPE_ACTIVITY_CLIENT			(evolution_activity_client_get_type ())
#define EVOLUTION_ACTIVITY_CLIENT(obj)			(GTK_CHECK_CAST ((obj), EVOLUTION_TYPE_ACTIVITY_CLIENT, EvolutionActivityClient))
#define EVOLUTION_ACTIVITY_CLIENT_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), EVOLUTION_TYPE_ACTIVITY_CLIENT, EvolutionActivityClientClass))
#define EVOLUTION_IS_ACTIVITY_CLIENT(obj)			(GTK_CHECK_TYPE ((obj), EVOLUTION_TYPE_ACTIVITY_CLIENT))
#define EVOLUTION_IS_ACTIVITY_CLIENT_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), EVOLUTION_TYPE_ACTIVITY_CLIENT))


typedef struct _EvolutionActivityClient        EvolutionActivityClient;
typedef struct _EvolutionActivityClientPrivate EvolutionActivityClientPrivate;
typedef struct _EvolutionActivityClientClass   EvolutionActivityClientClass;

struct _EvolutionActivityClient {
	GtkObject parent;

	EvolutionActivityClientPrivate *priv;
};

struct _EvolutionActivityClientClass {
	GtkObjectClass parent_class;

	/* Signals.  */
	void (* show_details) (EvolutionActivityClient *activity_client);
	void (* cancel) (EvolutionActivityClient *activity_client);
};


GtkType                  evolution_activity_client_get_type   (void);
gboolean                 evolution_activity_client_construct  (EvolutionActivityClient  *activity_client,
							       GNOME_Evolution_Shell     shell,
							       const char               *component_id,
							       GdkPixbuf               **animated_icon,
							       const char               *information,
							       gboolean                  cancellable,
							       gboolean                 *suggest_display_return);
EvolutionActivityClient *evolution_activity_client_new        (GNOME_Evolution_Shell     shell,
							       const char               *component_id,
							       GdkPixbuf               **animated_icon,
							       const char               *information,
							       gboolean                  cancellable,
							       gboolean                 *suggest_display_return);

gboolean  evolution_activity_client_update  (EvolutionActivityClient *activity_client,
					     const char              *information,
					     double                   progress);

GNOME_Evolution_Activity_DialogAction
evolution_activity_client_request_dialog  (EvolutionActivityClient             *activity_client,
					   GNOME_Evolution_Activity_DialogType  dialog_type);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _EVOLUTION_ACTIVITY_CLIENT_H_ */
