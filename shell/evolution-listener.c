/*
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Michael Zucchi <notzed@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "evolution-listener.h"

#define PARENT_TYPE bonobo_object_get_type ()

static BonoboObjectClass *parent_class = NULL;

/* Evolution.Listener */
static void
impl_complete(PortableServer_Servant _servant, CORBA_Environment * ev)
{
	EvolutionListener *el = (EvolutionListener *)bonobo_object_from_servant(_servant);

	if (el->complete)
		el->complete(el, el->data);
}

static void
evolution_listener_class_init (EvolutionListenerClass *klass)
{
	POA_GNOME_Evolution_Listener__epv *epv = &klass->epv;

	parent_class = g_type_class_peek_parent (klass);

	epv->complete = impl_complete;
}

static void
evolution_listener_init(EvolutionListener *emf, EvolutionListenerClass *klass)
{
}

BONOBO_TYPE_FUNC_FULL (EvolutionListener, GNOME_Evolution_Listener, PARENT_TYPE, evolution_listener)

EvolutionListener *
evolution_listener_new(EvolutionListenerFunc complete, gpointer data)
{
	EvolutionListener *el;

	el = g_object_new(evolution_listener_get_type(), NULL);
	el->complete = complete;
	el->data = data;

	return el;
}
