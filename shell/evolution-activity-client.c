/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-activity-client.c
 *
 * Copyright (C) 2001  Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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

#include <gtk/gtksignal.h>
#include <gtk/gtkmain.h>

#include <bonobo/bonobo-listener.h>

#include <gal/util/e-util.h>


#define PARENT_TYPE gtk_object_get_type ()
static GtkObjectClass *parent_class = NULL;

/* The minimum time between updates, in msecs.  */
#define UPDATE_DELAY 1000

enum {
	CLICKED,
	LAST_SIGNAL
};

static guint activity_client_signals[LAST_SIGNAL] = { 0 };

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

/* Create an icon from @pixbuf in @icon_return.  */
static void
create_icon_from_pixbuf (GdkPixbuf *pixbuf,
			 GNOME_Evolution_Icon *icon_return)
{
	const char *sp;
	CORBA_octet *dp;
	int width, height, total_width, rowstride;
	int i, j;
	gboolean has_alpha;

	width     = gdk_pixbuf_get_width (pixbuf);
	height    = gdk_pixbuf_get_height (pixbuf);
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);

	if (has_alpha)
		total_width = 4 * width;
	else
		total_width = 3 * width;

	icon_return->width = width;
	icon_return->height = height;
	icon_return->hasAlpha = has_alpha;

	icon_return->rgba_data._length = icon_return->height * total_width;
	icon_return->rgba_data._maximum = icon_return->rgba_data._length;
	icon_return->rgba_data._buffer = CORBA_sequence_CORBA_octet_allocbuf (icon_return->rgba_data._maximum);

	sp = gdk_pixbuf_get_pixels (pixbuf);
	dp = icon_return->rgba_data._buffer;
	for (i = 0; i < height; i ++) {
		for (j = 0; j < total_width; j++)
			*(dp ++) = sp[j];
		sp += rowstride;
	}
}

/* Generate an AnimatedIcon from a NULL-terminated @pixbuf_array.  */
static GNOME_Evolution_AnimatedIcon *
create_corba_animated_icon_from_pixbuf_array (GdkPixbuf **pixbuf_array)
{
	GNOME_Evolution_AnimatedIcon *animated_icon;
	GdkPixbuf **p;
	int num_frames;
	int i;

	num_frames = 0;
	for (p = pixbuf_array; *p != NULL; p++)
		num_frames++; 

	if (num_frames == 0)
		return NULL;

	animated_icon = GNOME_Evolution_AnimatedIcon__alloc ();

	animated_icon->_length = num_frames;
	animated_icon->_maximum = num_frames;
	animated_icon->_buffer = CORBA_sequence_GNOME_Evolution_Icon_allocbuf (animated_icon->_maximum);

	for (i = 0; i < num_frames; i++)
		create_icon_from_pixbuf (pixbuf_array[i], & animated_icon->_buffer[i]);

	CORBA_sequence_set_release (animated_icon, TRUE);

	return animated_icon;
}

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

	if (ev._major == CORBA_NO_EXCEPTION) {
		retval = TRUE;
	} else {
		g_warning ("EvolutionActivityClient: Error updating progress -- %s",
			   ev._repo_id);
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
		corba_update_progress (activity_client, priv->new_information, priv->new_progress);
		return TRUE;
	} else {
		return FALSE;
	}
}


/* BonoboListener callback.  */

static void
listener_callback (BonoboListener *listener,
		   char *event_name, 
		   CORBA_any *any,
		   CORBA_Environment *ev,
		   gpointer data)
{
	/* FIXME: Implement.  */
	g_print ("EvolutionActivityClient: BonoboListener event -- %s\n", event_name);
}


/* GtkObject methods.  */

static void
impl_destroy (GtkObject *object)
{
	EvolutionActivityClient *activity_client;
	EvolutionActivityClientPrivate *priv;
	CORBA_Environment ev;

	activity_client = EVOLUTION_ACTIVITY_CLIENT (object);
	priv = activity_client->priv;

	CORBA_exception_init (&ev);

	if (! CORBA_Object_is_nil (priv->activity_interface, &ev)) {
		GNOME_Evolution_Activity_operationFinished (priv->activity_interface,
							    priv->activity_id,
							    &ev);
		if (ev._major != CORBA_NO_EXCEPTION)
			g_warning ("EvolutionActivityClient: Error reporting completion of operation -- %s",
				   ev._repo_id);

		CORBA_Object_release (priv->activity_interface, &ev);
	}

	CORBA_exception_free (&ev);

	if (priv->next_update_timeout_id != 0)
		gtk_timeout_remove (priv->next_update_timeout_id);

	g_free (priv->new_information);

	g_free (priv);

	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
class_init (EvolutionActivityClientClass *klass)
{
	GtkObjectClass *object_class;

	parent_class = gtk_type_class (PARENT_TYPE);

	object_class = GTK_OBJECT_CLASS (klass);
	object_class->destroy = impl_destroy;

	activity_client_signals[CLICKED] = 
		gtk_signal_new ("clicked",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EvolutionActivityClientClass, clicked),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, activity_client_signals, LAST_SIGNAL);
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
				     EvolutionShellClient *shell_client,
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

	g_return_val_if_fail (activity_client != NULL, FALSE);
	g_return_val_if_fail (EVOLUTION_IS_ACTIVITY_CLIENT (activity_client), FALSE);
	g_return_val_if_fail (shell_client != NULL, FALSE);
	g_return_val_if_fail (EVOLUTION_IS_SHELL_CLIENT (shell_client), FALSE);
	g_return_val_if_fail (animated_icon != NULL, FALSE);
	g_return_val_if_fail (*animated_icon != NULL, FALSE);
	g_return_val_if_fail (information != NULL, FALSE);
	g_return_val_if_fail (suggest_display_return != NULL, FALSE);

	priv = activity_client->priv;
	g_return_val_if_fail (priv->activity_interface == CORBA_OBJECT_NIL, FALSE);

	GTK_OBJECT_UNSET_FLAGS (activity_client, GTK_FLOATING);

	CORBA_exception_init (&ev);

	activity_interface = evolution_shell_client_get_activity_interface (shell_client);
	priv->activity_interface = CORBA_Object_duplicate (activity_interface, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		priv->activity_interface = CORBA_OBJECT_NIL;
		CORBA_exception_free (&ev);
		return FALSE;
	}

	GNOME_Evolution_Activity_operationStarted (activity_interface,
						   component_id,
						   create_corba_animated_icon_from_pixbuf_array (animated_icon),
						   information,
						   cancellable,
						   bonobo_object_corba_objref (BONOBO_OBJECT (priv->listener)),
						   &priv->activity_id,
						   &suggest_display,
						   &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		CORBA_exception_free (&ev);
		return FALSE;
	}

	*suggest_display_return = (gboolean) suggest_display;

	CORBA_exception_free (&ev);
	return TRUE;
}

EvolutionActivityClient *
evolution_activity_client_new (EvolutionShellClient *shell_client,
			       const char *component_id,
			       GdkPixbuf **animated_icon,
			       const char *information,
			       gboolean cancellable,
			       gboolean *suggest_display_return)
{
	EvolutionActivityClient *activity_client;

	g_return_val_if_fail (shell_client != NULL, NULL);
	g_return_val_if_fail (EVOLUTION_IS_SHELL_CLIENT (shell_client), NULL);
	g_return_val_if_fail (animated_icon != NULL, NULL);
	g_return_val_if_fail (*animated_icon != NULL, NULL);
	g_return_val_if_fail (information != NULL, NULL);
	g_return_val_if_fail (suggest_display_return != NULL, NULL);

	activity_client = gtk_type_new (evolution_activity_client_get_type ());

	if (! evolution_activity_client_construct (activity_client,
						   shell_client,
						   component_id,
						   animated_icon,
						   information,
						   cancellable,
						   suggest_display_return)) {
		gtk_object_unref (GTK_OBJECT (activity_client));
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
	g_return_val_if_fail (progress >= 0.0 && progress <= 1.0, FALSE);

	priv = activity_client->priv;

	if (priv->next_update_timeout_id == 0) {
		/* There is no pending timeout, so the last CORBA update
		   happened more than UPDATE_DELAY msecs ago.  */

		retval = corba_update_progress (activity_client, information, progress);

		/* Set up a timeout so we can check against it at the next
		   update request.  */

		priv->next_update_timeout_id = g_timeout_add (UPDATE_DELAY,
							      update_timeout_callback,
							      activity_client);

		priv->have_pending_update = FALSE;
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
evolution_activity_client_request_dialog (EvolutionActivityClient *client,
					  GNOME_Evolution_Activity_DialogType dialog_type)
{
	EvolutionActivityClientPrivate *priv;
	GNOME_Evolution_Activity_DialogAction retval;
	CORBA_Environment ev;

	g_return_val_if_fail (client != NULL, GNOME_Evolution_Activity_DIALOG_ACTION_ERROR);
	g_return_val_if_fail (EVOLUTION_IS_ACTIVITY_CLIENT (client), GNOME_Evolution_Activity_DIALOG_ACTION_ERROR);

	priv = client->priv;

	CORBA_exception_init (&ev);

	retval = GNOME_Evolution_Activity_requestDialog (priv->activity_interface,
							 priv->activity_id,
							 dialog_type,
							 &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("EvolutionActivityClient: Error requesting a dialog -- %s", ev._repo_id);
		retval = GNOME_Evolution_Activity_DIALOG_ACTION_ERROR;
	}

	CORBA_exception_free (&ev);

	return retval;
}


E_MAKE_TYPE (evolution_activity_client, "EvolutionActivityClient", EvolutionActivityClient,
	     class_init, init, PARENT_TYPE)
