/*
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

#include "evolution-component.h"

#define PARENT_TYPE bonobo_object_get_type ()

static BonoboObjectClass *parent_class = NULL;

/* Evolution.Component */

/* Initialization */

static void
evolution_component_class_init (EvolutionComponentClass *klass)
{
	parent_class = g_type_class_peek_parent (klass);
}

static void
evolution_component_init(EvolutionComponent *emf, EvolutionComponentClass *klass)
{
}

BONOBO_TYPE_FUNC_FULL (EvolutionComponent, GNOME_Evolution_Component, PARENT_TYPE, evolution_component)
