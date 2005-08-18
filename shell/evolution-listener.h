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
 * Abstract class wrapper for EvolutionListener
 */

#ifndef _EVOLUTION_LISTENER_H_
#define _EVOLUTION_LISTENER_H_

#include <bonobo/bonobo-object.h>
#include "shell/Evolution.h"

typedef struct _EvolutionListener        EvolutionListener;
typedef struct _EvolutionListenerClass   EvolutionListenerClass;

typedef void (*EvolutionListenerFunc)(EvolutionListener *, void *);

struct _EvolutionListener {
	BonoboObject parent;

	/* we dont need signals, so why bother wasting resources on it */
	EvolutionListenerFunc complete;
	void *data;
};

struct _EvolutionListenerClass {
	BonoboObjectClass parent_class;

	POA_GNOME_Evolution_Listener__epv epv;
};

GType           evolution_listener_get_type(void);
EvolutionListener *evolution_listener_new(EvolutionListenerFunc complete, void *data);

#endif /* _EVOLUTION_LISTENER_H_ */
