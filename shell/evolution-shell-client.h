/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-shell-client.h
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

#ifndef __EVOLUTION_SHELL_CLIENT_H__
#define __EVOLUTION_SHELL_CLIENT_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <bonobo/bonobo-object.h>

#include "Evolution.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define EVOLUTION_TYPE_SHELL_CLIENT			(evolution_shell_client_get_type ())
#define EVOLUTION_SHELL_CLIENT(obj)			(GTK_CHECK_CAST ((obj), EVOLUTION_TYPE_SHELL_CLIENT, EvolutionShellClient))
#define EVOLUTION_SHELL_CLIENT_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), EVOLUTION_TYPE_SHELL_CLIENT, EvolutionShellClientClass))
#define EVOLUTION_IS_SHELL_CLIENT(obj)			(GTK_CHECK_TYPE ((obj), EVOLUTION_TYPE_SHELL_CLIENT))
#define EVOLUTION_IS_SHELL_CLIENT_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), EVOLUTION_TYPE_SHELL_CLIENT))


typedef struct _EvolutionShellClient        EvolutionShellClient;
typedef struct _EvolutionShellClientPrivate EvolutionShellClientPrivate;
typedef struct _EvolutionShellClientClass   EvolutionShellClientClass;

struct _EvolutionShellClient {
	BonoboObject parent;

	EvolutionShellClientPrivate *priv;
};

struct _EvolutionShellClientClass {
	BonoboObjectClass parent_class;
};


GtkType               evolution_shell_client_get_type   (void);
void                  evolution_shell_client_construct  (EvolutionShellClient *shell_client,
							 Evolution_Shell       corba_shell);
EvolutionShellClient *evolution_shell_client_new        (Evolution_Shell       shell);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __EVOLUTION_SHELL_CLIENT_H__ */
