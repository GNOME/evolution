/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-shell-component-client.h
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

#ifndef EVOLUTION_SHELL_COMPONENT_CLIENT_H
#define EVOLUTION_SHELL_COMPONENT_CLIENT_H

#include <bonobo/bonobo-object-client.h>
#include <bonobo/bonobo-ui-handler.h>

#include "evolution-shell-component.h"

#ifdef cplusplus
extern "C" {
#pragma }
#endif /* cplusplus */

#define EVOLUTION_TYPE_SHELL_COMPONENT_CLIENT            (evolution_shell_component_client_get_type ())
#define EVOLUTION_SHELL_COMPONENT_CLIENT(obj)            (GTK_CHECK_CAST ((obj), EVOLUTION_TYPE_SHELL_COMPONENT_CLIENT, EvolutionShellComponentClient))
#define EVOLUTION_SHELL_COMPONENT_CLIENT_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), EVOLUTION_TYPE_SHELL_COMPONENT_CLIENT, EvolutionShellComponentClientClass))
#define EVOLUTION_IS_SHELL_COMPONENT_CLIENT(obj)         (GTK_CHECK_TYPE ((obj), EVOLUTION_TYPE_SHELL_COMPONENT_CLIENT))
#define EVOLUTION_IS_SHELL_COMPONENT_CLIENT_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((obj), EVOLUTION_TYPE_SHELL_COMPONENT_CLIENT))


typedef struct _EvolutionShellComponentClient        EvolutionShellComponentClient;
typedef struct _EvolutionShellComponentClientPrivate EvolutionShellComponentClientPrivate;
typedef struct _EvolutionShellComponentClientClass   EvolutionShellComponentClientClass;

struct _EvolutionShellComponentClient {
	BonoboObjectClient parent;

	EvolutionShellComponentClientPrivate *priv;
};

struct _EvolutionShellComponentClientClass {
	BonoboObjectClientClass parent_class;
};

typedef void (* EvolutionShellComponentClientCallback) (EvolutionShellComponentClient *shell_component_client,
							EvolutionShellComponentResult result,
							void *data);


/* Construction.  */
GtkType                        evolution_shell_component_client_get_type   (void);
void                           evolution_shell_component_client_construct  (EvolutionShellComponentClient *shell_component_client,
									    CORBA_Object                   corba_object);
EvolutionShellComponentClient *evolution_shell_component_client_new        (const char                    *id);

/* Synchronous operations.  */
EvolutionShellComponentResult  evolution_shell_component_client_set_owner    (EvolutionShellComponentClient  *shell_component_client,
									      Evolution_Shell                 shell);
EvolutionShellComponentResult  evolution_shell_component_client_unset_owner  (EvolutionShellComponentClient  *shell_component_client,
									      Evolution_Shell                 shell);
EvolutionShellComponentResult  evolution_shell_component_client_create_view  (EvolutionShellComponentClient  *shell_component_client,
									      BonoboUIHandler                *uih,
									      const char                     *physical_uri,
									      const char                     *type_string,
									      BonoboControl                 **control_return);

/* Asyncronous operations.  */
void  evolution_shell_component_client_async_create_folder  (EvolutionShellComponentClient         *shell_component_client,
							     const char                            *physical_uri,
							     const char                            *type,
							     EvolutionShellComponentClientCallback  callback,
							     void                                  *data);
void  evolution_shell_component_client_async_remove_folder  (EvolutionShellComponentClient         *shell_component_client,
							     const char                            *physical_uri,
							     EvolutionShellComponentClientCallback  callback,
							     void                                  *data);

void  evolution_shell_component_client_populate_folder_context_menu  (EvolutionShellComponentClient *shell_component_client,
								      BonoboUIHandler               *uih,
								      const char                    *physical_uri,
								      const char                    *type);

#ifdef cplusplus
}
#endif /* cplusplus */

#endif /* EVOLUTION_SHELL_COMPONENT_CLIENT_H */
