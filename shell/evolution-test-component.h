/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-test-component.h
 *
 * Copyright (C) 2004  Novell, Inc.
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
 * Author: JP Rosevear <jpr@ximian.com>
 */

#ifndef _EVOLUTION_TEST_COMPONENT_H_
#define _EVOLUTION_TEST_COMPONENT_H_

#include <bonobo/bonobo-object.h>
#include "Evolution.h"


#define EVOLUTION_TEST_TYPE_COMPONENT			(evolution_test_component_get_type ())
#define EVOLUTION_TEST_COMPONENT(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), EVOLUTION_TEST_TYPE_COMPONENT, EvolutionTestComponent))
#define EVOLUTION_TEST_COMPONENT_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), EVOLUTION_TEST_TYPE_COMPONENT, EvolutionTestComponentClass))
#define EVOLUTION_TEST_IS_COMPONENT(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), EVOLUTION_TEST_TYPE_COMPONENT))
#define EVOLUTION_TEST_IS_COMPONENT_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE ((obj), EVOLUTION_TEST_TYPE_COMPONENT))


typedef struct _EvolutionTestComponent        EvolutionTestComponent;
typedef struct _EvolutionTestComponentPrivate EvolutionTestComponentPrivate;
typedef struct _EvolutionTestComponentClass   EvolutionTestComponentClass;

struct _EvolutionTestComponent {
	BonoboObject parent;

	EvolutionTestComponentPrivate *priv;
};

struct _EvolutionTestComponentClass {
	BonoboObjectClass parent_class;

	POA_GNOME_Evolution_Component__epv epv;
};


GType           evolution_test_component_get_type  (void);

#endif /* _EVOLUTION_TEST_COMPONENT_H_ */
