/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-session.c
 *
 * Copyright (C) 2000  Ximian, Inc.
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
 * Author: Ettore Perazzoli
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtksignal.h>
#include <gal/util/e-util.h>

#include "Evolution.h"

#include "evolution-session.h"


#define PARENT_TYPE bonobo_x_object_get_type ()
static BonoboXObjectClass *parent_class = NULL;

struct _EvolutionSessionPrivate {
	int dummy;
};

enum {
	LOAD_CONFIGURATION,
	SAVE_CONFIGURATION,
	LAST_SIGNAL
};

static int signals[LAST_SIGNAL];


/* GtkObject methods.  */

static void
impl_destroy (GtkObject *object)
{
	EvolutionSession *session;
	EvolutionSessionPrivate *priv;

	session = EVOLUTION_SESSION (object);
	priv = session->priv;

	g_free (priv);

	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


/* CORBA interface implementation.  */

static void
impl_GNOME_Evolution_Session_saveConfiguration (PortableServer_Servant servant,
						const CORBA_char *prefix,
						CORBA_Environment *ev)
{
	BonoboObject *self;

	self = bonobo_object_from_servant (servant);
	gtk_signal_emit (GTK_OBJECT (self), signals[SAVE_CONFIGURATION], prefix);
}

static void
impl_GNOME_Evolution_Session_loadConfiguration (PortableServer_Servant servant,
						const CORBA_char *prefix,
						CORBA_Environment *ev)
{
	BonoboObject *self;

	self = bonobo_object_from_servant (servant);
	gtk_signal_emit (GTK_OBJECT (self), signals[LOAD_CONFIGURATION], prefix);
}


/* Initialization.  */

static void
corba_class_init (EvolutionSessionClass *klass)
{
	POA_GNOME_Evolution_Session__epv *epv = & (EVOLUTION_SESSION_CLASS (klass)->epv);

	epv = g_new0 (POA_GNOME_Evolution_Session__epv, 1);
	epv->saveConfiguration = impl_GNOME_Evolution_Session_saveConfiguration;
	epv->loadConfiguration = impl_GNOME_Evolution_Session_loadConfiguration;
}

static void
class_init (EvolutionSessionClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	parent_class = gtk_type_class (PARENT_TYPE);

	object_class->destroy = impl_destroy;

	signals[LOAD_CONFIGURATION]
		= gtk_signal_new ("load_configuration",
				  GTK_RUN_FIRST,
				  GTK_CLASS_TYPE (object_class),
				  GTK_SIGNAL_OFFSET (EvolutionSessionClass, load_configuration),
				  gtk_marshal_NONE__STRING,
				  GTK_TYPE_NONE, 1,
				  GTK_TYPE_STRING);
	signals[SAVE_CONFIGURATION]
		= gtk_signal_new ("save_configuration",
				  GTK_RUN_FIRST,
				  GTK_CLASS_TYPE (object_class),
				  GTK_SIGNAL_OFFSET (EvolutionSessionClass, save_configuration),
				  gtk_marshal_NONE__STRING,
				  GTK_TYPE_NONE, 1,
				  GTK_TYPE_STRING);

	corba_class_init (klass);
}

static void
init (EvolutionSession *session)
{
	EvolutionSessionPrivate *priv;

	priv = g_new (EvolutionSessionPrivate, 1);

	session->priv = priv;
}


EvolutionSession *
evolution_session_new (void)
{
	return gtk_type_new (evolution_session_get_type ());
}


E_MAKE_X_TYPE (evolution_session, "EvolutionSession", EvolutionSession,
	       class_init, init, PARENT_TYPE,
	       POA_GNOME_Evolution_Session__init,
	       GTK_STRUCT_OFFSET (EvolutionSessionClass, epv))
