/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 * Copyright (C) 2005  Novell, Inc.
 *
 * Authors: Michael Zucchi <notzed@novell.com>
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
evolution_listener_new(EvolutionListenerFunc complete, void *data)
{
	EvolutionListener *el;

	el = g_object_new(evolution_listener_get_type(), NULL);
	el->complete = complete;
	el->data = data;

	return el;
}
