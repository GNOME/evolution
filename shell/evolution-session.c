/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-session.c
 *
 * Copyright (C) 2000  Helix Code, Inc.
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
 * Author: Ettore Perazzoli
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gnome.h>

#include "Evolution.h"

#include "e-util/e-util.h"

#include "evolution-session.h"


#define PARENT_TYPE bonobo_object_get_type ()
static BonoboObjectClass *parent_class = NULL;

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
impl_Evolution_Session_save_configuration (PortableServer_Servant servant,
					   const CORBA_char *prefix,
					   CORBA_Environment *ev)
{
	BonoboObject *self;

	self = bonobo_object_from_servant (servant);
	gtk_signal_emit (GTK_OBJECT (self), signals[SAVE_CONFIGURATION], prefix);
}

static void
impl_Evolution_Session_load_configuration (PortableServer_Servant servant,
					   const CORBA_char *prefix,
					   CORBA_Environment *ev)
{
	BonoboObject *self;

	self = bonobo_object_from_servant (servant);
	gtk_signal_emit (GTK_OBJECT (self), signals[LOAD_CONFIGURATION], prefix);
}


/* Initialization.  */

static POA_Evolution_Session__vepv Evolution_Session_vepv;

static void
corba_class_init (void)
{
	POA_Evolution_Session__vepv *vepv;
	POA_Evolution_Session__epv *epv;
	PortableServer_ServantBase__epv *base_epv;

	base_epv = g_new0 (PortableServer_ServantBase__epv, 1);
	base_epv->_private    = NULL;
	base_epv->finalize    = NULL;
	base_epv->default_POA = NULL;

	epv = g_new0 (POA_Evolution_Session__epv, 1);
	epv->save_configuration = impl_Evolution_Session_save_configuration;
	epv->load_configuration = impl_Evolution_Session_load_configuration;

	vepv = &Evolution_Session_vepv;
	vepv->_base_epv             = base_epv;
	vepv->Bonobo_Unknown_epv    = bonobo_object_get_epv ();
	vepv->Evolution_Session_epv = epv;
}

static void
class_init (EvolutionSessionClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	parent_class = gtk_type_class (bonobo_object_get_type ());

	object_class->destroy = impl_destroy;

	signals[LOAD_CONFIGURATION]
		= gtk_signal_new ("load_configuration",
				  GTK_RUN_FIRST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (EvolutionSessionClass, load_configuration),
				  gtk_marshal_NONE__STRING,
				  GTK_TYPE_NONE, 1,
				  GTK_TYPE_STRING);
	signals[SAVE_CONFIGURATION]
		= gtk_signal_new ("save_configuration",
				  GTK_RUN_FIRST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (EvolutionSessionClass, save_configuration),
				  gtk_marshal_NONE__STRING,
				  GTK_TYPE_NONE, 1,
				  GTK_TYPE_STRING);

	corba_class_init ();
}

static void
init (EvolutionSession *session)
{
	EvolutionSessionPrivate *priv;

	priv = g_new (EvolutionSessionPrivate, 1);

	session->priv = priv;
}


static Evolution_Session
create_corba_session (BonoboObject *object)
{
	POA_Evolution_Session *servant;
	CORBA_Environment ev;

	servant = (POA_Evolution_Session *) g_new0 (BonoboObjectServant, 1);
	servant->vepv = &Evolution_Session_vepv;

	CORBA_exception_init (&ev);

	POA_Evolution_Session__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION){
		g_free (servant);
		CORBA_exception_free (&ev);
		return CORBA_OBJECT_NIL;
	}

	CORBA_exception_free (&ev);
	return (Evolution_Session) bonobo_object_activate_servant (object, servant);
}

void
evolution_session_construct (EvolutionSession *session,
			     CORBA_Object corba_session)
{
	g_return_if_fail (session != NULL);
	g_return_if_fail (corba_session != CORBA_OBJECT_NIL);

	bonobo_object_construct (BONOBO_OBJECT (session), corba_session);
}

EvolutionSession *
evolution_session_new (void)
{
	EvolutionSession *session;
	Evolution_Session corba_session;

	session = gtk_type_new (evolution_session_get_type ());

	corba_session = create_corba_session (BONOBO_OBJECT (session));
	if (corba_session == CORBA_OBJECT_NIL) {
		bonobo_object_unref (BONOBO_OBJECT (session));
		return NULL;
	}

	evolution_session_construct (session, corba_session);
	return session;
}


E_MAKE_TYPE (evolution_session, "EvolutionSession", EvolutionSession, class_init, init, PARENT_TYPE)
