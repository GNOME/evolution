/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-activity-client.c
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

/* Another evil GTK+ object wrapper for a CORBA API.  In this case, the wrapper
   is needed to avoid sending too frequent CORBA requests across the wire, thus
   slowing the client down.  The wrapper makes sure that there is a minimum
   amount of time between each CORBA method invocation.  */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "evolution-activity-client.h"

#include "e-shell-corba-icon-utils.h"

#include "e-shell-marshal.h"

#include <string.h>

#include <gtk/gtksignal.h>
#include <gtk/gtkmain.h>

#include <bonobo/bonobo-listener.h>
#include <bonobo/bonobo-exception.h>

#include <bonobo/Bonobo.h>

#include <gal/util/e-util.h>


#define PARENT_TYPE gtk_object_get_type ()
static GtkObjectClass *parent_class = NULL;

/* The minimum time between updates, in msecs.  */
#define UPDATE_DELAY 1000

enum {
	SHOW_DETAILS,
	CANCEL,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

struct _EvolutionActivityClientPrivate {
	/* The ::Activity interface that we QI from the shell.  */
	GNOME_Evolution_Activity activity_interface;

	/* BonoboListener used to get notification about actions that the user
	   requested on the activity.  */
	BonoboListener *listener;

	/* Id of this activity.  */
	GNOME_Evolution_Activity_ActivityId activity_id;

	/* Id for the GTK+ timeout used to do updates.  */
	int next_update_timeout_id;

	/* Wether we have to actually push an update at this timeout.  */
	int have_pending_update;

	/* Data for the next update.  */
	char *new_information;
	double new_progress;
};


/* Utility functions.  */

static gboolean
corba_update_progress (EvolutionActivityClient *activity_client,
		       const char *information,
		       double progress)
{
	EvolutionActivityClientPrivate *priv;
	CORBA_Environment ev;
	gboolean retval;

	priv = activity_client->priv;

	CORBA_exception_init (&ev);

	GNOME_Evolution_Activity_operationProgressing (priv->activity_interface,
						       priv->activity_id,
						       information,
						       progress,
						       &ev);

	if (! BONOBO_EX (&ev)) {
		retval = TRUE;
	} else {
		g_warning ("EvolutionActivityClient: Error updating progress -- %s",
			   BONOBO_EX_REPOID (&ev));
		retval = FALSE;
	}

	CORBA_exception_free (&ev);

	return retval;
}

static gboolean
update_timeout_callback (void *data)
{
	EvolutionActivityClient *activity_client;
	EvolutionActivityClientPrivate *priv;

	activity_client = EVOLUTION_ACTIVITY_CLIENT (data);
	priv = activity_client->priv;

	if (priv->have_pending_update) {
		priv->have_pending_update = FALSE;
		corba_update_progress (activity_client, priv->new_information, priv->new_progress);
		return TRUE;
	} else {
		priv->next_update_timeout_id = 0;
		return FALSE;
	}
}

static CORBA_Object
get_shell_activity_iface (GNOME_Evolution_Shell shell_iface)
{
	CORBA_Object iface_object;
	const char *iface_name = "IDL:GNOME/Evolution/Activity:" BASE_VERSION;

 	iface_object = bonobo_object_query_remote (shell_iface, iface_name, NULL);
	if (iface_object == CORBA_OBJECT_NIL)
		g_warning ("EvolutionActivityClient: No iface %s on Shell", iface_name);

	return iface_object;
}


/* BonoboListener callback.  */

static void
listener_callback (BonoboListener *listener,
		   const char *event_name, 
		   const CORBA_any *any,
		   CORBA_Environment *ev,
		   void *data)
{
	EvolutionActivityClient *activity_client;

	activity_client = EVOLUTION_ACTIVITY_CLIENT (data);

	if (strcmp (event_name, "ShowDetails") == 0)
		g_signal_emit (activity_client, signals[SHOW_DETAILS], 0);
	else if (strcmp (event_name, "Cancel") == 0)
		g_signal_emit (activity_client, signals[CANCEL], 0);
	else
		g_warning ("EvolutionActivityClient: Unknown event from listener -- %s", event_name);
}


/* GObject methods.  */

static void
impl_dispose (GObject *object)
{
	EvolutionActivityClient *activity_client;
	EvolutionActivityClientPrivate *priv;
	CORBA_Environment ev;

	activity_client = EVOLUTION_ACTIVITY_CLIENT (object);
	priv = activity_client->priv;

	if (priv->next_update_timeout_id != 0) {
		g_source_remove (priv->next_update_timeout_id);
		priv->next_update_timeout_id = 0;
	}

	CORBA_exception_init (&ev);

	if (! CORBA_Object_is_nil (priv->activity_interface, &ev)) {
		GNOME_Evolution_Activity_operationFinished (priv->activity_interface,
							    priv->activity_id,
							    &ev);
		if (BONOBO_EX (&ev))
			g_warning ("EvolutionActivityClient: Error reporting completion of operation -- %s",
				   BONOBO_EX_REPOID (&ev));

		CORBA_Object_release (priv->activity_interface, &ev);

		priv->activity_interface = CORBA_OBJECT_NIL;
	}

	CORBA_exception_free (&ev);

	if (priv->listener != NULL) {
		bonobo_object_unref (BONOBO_OBJECT (priv->listener));
		priv->listener = NULL;
	}

	(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
impl_finalize (GObject *object)
{
	EvolutionActivityClient *activity_client;
	EvolutionActivityClientPrivate *priv;

	activity_client = EVOLUTION_ACTIVITY_CLIENT (object);
	priv = activity_client->priv;

	g_free (priv->new_information);
	g_free (priv);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}


static void
class_init (EvolutionActivityClientClass *klass)
{
	GObjectClass *object_class;

	parent_class = g_type_class_ref(PARENT_TYPE);

	object_class = G_OBJECT_CLASS (klass);
	object_class->dispose  = impl_dispose;
	object_class->finalize = impl_finalize;

	signals[SHOW_DETAILS] 
		= g_signal_new ("show_details",
				G_OBJECT_CLASS_TYPE (object_class),
				G_SIGNAL_RUN_FIRST,
				G_STRUCT_OFFSET (EvolutionActivityClientClass, show_details),
				NULL, NULL,
				e_shell_marshal_NONE__NONE,
				G_TYPE_NONE, 0);

	signals[CANCEL] 
		= g_signal_new ("cancel",
				G_OBJECT_CLASS_TYPE (object_class),
				G_SIGNAL_RUN_FIRST,
				G_STRUCT_OFFSET (EvolutionActivityClientClass, cancel),
				NULL, NULL,
				e_shell_marshal_NONE__NONE,
				G_TYPE_NONE, 0);
}


static void
init (EvolutionActivityClient *activity_client)
{
	EvolutionActivityClientPrivate *priv;

	priv = g_new (EvolutionActivityClientPrivate, 1);
	priv->activity_interface     = CORBA_OBJECT_NIL;
	priv->listener               = bonobo_listener_new (listener_callback, activity_client);
	priv->activity_id            = (GNOME_Evolution_Activity_ActivityId) 0;
	priv->next_update_timeout_id = 0;
	priv->have_pending_update    = FALSE;
	priv->new_information        = NULL;
	priv->new_progress           = 0.0;

	activity_client->priv = priv;
}


gboolean
evolution_activity_client_construct (EvolutionActivityClient *activity_client,
				     GNOME_Evolution_Shell shell_iface,
				     const char *component_id,
				     GdkPixbuf **animated_icon,
				     const char *information,
				     gboolean cancellable,
				     gboolean *suggest_display_return)
{
	EvolutionActivityClientPrivate *priv;
	GNOME_Evolution_Activity activity_interface;
	CORBA_Environment ev;
	CORBA_boolean suggest_display;
	GNOME_Evolution_AnimatedIcon *corba_animated_icon;

	g_return_val_if_fail (activity_client != NULL, FALSE);
	g_return_val_if_fail (EVOLUTION_IS_ACTIVITY_CLIENT (activity_client), FALSE);
	g_return_val_if_fail (animated_icon != NULL, FALSE);
	g_return_val_if_fail (*animated_icon != NULL, FALSE);
	g_return_val_if_fail (information != NULL, FALSE);
	g_return_val_if_fail (suggest_display_return != NULL, FALSE);

	priv = activity_client->priv;
	g_return_val_if_fail (priv->activity_interface == CORBA_OBJECT_NIL, FALSE);

	GTK_OBJECT_UNSET_FLAGS (activity_client, GTK_FLOATING);

	CORBA_exception_init (&ev);

	activity_interface = get_shell_activity_iface (shell_iface);
	priv->activity_interface = CORBA_Object_duplicate (activity_interface, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		priv->activity_interface = CORBA_OBJECT_NIL;
		CORBA_exception_free (&ev);
		return FALSE;
	}

	corba_animated_icon = e_new_corba_animated_icon_from_pixbuf_array (animated_icon);

	GNOME_Evolution_Activity_operationStarted (activity_interface,
						   component_id,
						   corba_animated_icon,
						   information,
						   cancellable,
						   bonobo_object_corba_objref (BONOBO_OBJECT (priv->listener)),
						   &priv->activity_id,
						   &suggest_display,
						   &ev);

	CORBA_free (corba_animated_icon);

	if (ev._major != CORBA_NO_EXCEPTION) {
		CORBA_exception_free (&ev);
		return FALSE;
	}

	*suggest_display_return = (gboolean) suggest_display;

	CORBA_exception_free (&ev);
	return TRUE;
}

EvolutionActivityClient *
evolution_activity_client_new (GNOME_Evolution_Shell shell_iface,
			       const char *component_id,
			       GdkPixbuf **animated_icon,
			       const char *information,
			       gboolean cancellable,
			       gboolean *suggest_display_return)
{
	EvolutionActivityClient *activity_client;

	g_return_val_if_fail (shell_iface != CORBA_OBJECT_NIL, NULL);
	g_return_val_if_fail (animated_icon != NULL, NULL);
	g_return_val_if_fail (*animated_icon != NULL, NULL);
	g_return_val_if_fail (information != NULL, NULL);
	g_return_val_if_fail (suggest_display_return != NULL, NULL);

	activity_client = g_object_new (evolution_activity_client_get_type (), NULL);

	if (! evolution_activity_client_construct (activity_client,
						   shell_iface,
						   component_id,
						   animated_icon,
						   information,
						   cancellable,
						   suggest_display_return)) {
		g_object_unref (activity_client);
		return NULL;
	}

	return activity_client;
}


gboolean
evolution_activity_client_update (EvolutionActivityClient *activity_client,
				  const char *information,
				  double progress)
{
	EvolutionActivityClientPrivate *priv;
	gboolean retval;

	g_return_val_if_fail (activity_client != NULL, FALSE);
	g_return_val_if_fail (EVOLUTION_IS_ACTIVITY_CLIENT (activity_client), FALSE);
	g_return_val_if_fail (information != NULL, FALSE);
	g_return_val_if_fail (progress == -1.0 || (progress >= 0.0 && progress <= 1.0), FALSE);

	priv = activity_client->priv;

	if (priv->next_update_timeout_id == 0) {
		/* There is no pending timeout, so the last CORBA update
		   happened more than UPDATE_DELAY msecs ago. So we set up a
		   timeout so we can check against it at the next update
		   request.

		   Notice that GLib timeouts or other operations on this object
		   can be invoked within a remote CORBA invocation, so we need
		   to set `next_update_timeout_id' and `have_pending_update'
		   before doing the CORBA call, or nasty race conditions might
		   happen.  */

		priv->have_pending_update = FALSE;

		priv->next_update_timeout_id = g_timeout_add (UPDATE_DELAY,
							      update_timeout_callback,
							      activity_client);

		retval = corba_update_progress (activity_client, information, progress);
	} else {
		/* There is a pending timeout, so the last CORBA update
		   happened less than UPDATE_DELAY msecs ago.  So just queue an
		   update instead.  */

		g_free (priv->new_information);
		priv->new_information = g_strdup (information);
		priv->new_progress = progress;

		priv->have_pending_update = TRUE;

		retval = TRUE;
	}

	return retval;
}

GNOME_Evolution_Activity_DialogAction
evolution_activity_client_request_dialog (EvolutionActivityClient *activity_client,
					  GNOME_Evolution_Activity_DialogType dialog_type)
{
	EvolutionActivityClientPrivate *priv;
	GNOME_Evolution_Activity_DialogAction retval;
	CORBA_Environment ev;

	g_return_val_if_fail (activity_client != NULL, GNOME_Evolution_Activity_DIALOG_ACTION_ERROR);
	g_return_val_if_fail (EVOLUTION_IS_ACTIVITY_CLIENT (activity_client), GNOME_Evolution_Activity_DIALOG_ACTION_ERROR);

	priv = activity_client->priv;

	CORBA_exception_init (&ev);

	retval = GNOME_Evolution_Activity_requestDialog (priv->activity_interface,
							 priv->activity_id,
							 dialog_type,
							 &ev);
	if (BONOBO_EX (&ev) != CORBA_NO_EXCEPTION) {
		g_warning ("EvolutionActivityClient: Error requesting a dialog -- %s", BONOBO_EX_REPOID (&ev));
		retval = GNOME_Evolution_Activity_DIALOG_ACTION_ERROR;
	}

	CORBA_exception_free (&ev);

	return retval;
}


E_MAKE_TYPE (evolution_activity_client, "EvolutionActivityClient", EvolutionActivityClient,
	     class_init, init, PARENT_TYPE)
