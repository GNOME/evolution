/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright (C) 2005  Novell, Inc.
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
 * Author: Michael Zucchi <notzed@novell.com>
 *
 * Abstract class wrapper for EvolutionComponent
 */

#ifndef _EVOLUTION_COMPONENT_H_
#define _EVOLUTION_COMPONENT_H_

#include <bonobo/bonobo-object.h>
#include "shell/Evolution.h"

typedef struct _EvolutionComponent        EvolutionComponent;
typedef struct _EvolutionComponentClass   EvolutionComponentClass;

struct _EvolutionComponent {
	BonoboObject parent;
};

struct _EvolutionComponentClass {
	BonoboObjectClass parent_class;

	POA_GNOME_Evolution_Component__epv epv;
};

GType           evolution_component_get_type(void);

#endif /* _EVOLUTION_COMPONENT_H_ */
