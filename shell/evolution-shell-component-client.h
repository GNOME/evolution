/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-shell-component-client.h
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

#ifndef EVOLUTION_SHELL_COMPONENT_CLIENT_H
#define EVOLUTION_SHELL_COMPONENT_CLIENT_H

#include <glib-object.h>

#include <bonobo/bonobo-ui-container.h>
#include <bonobo/bonobo-ui-component.h>

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
	GObject parent;

	EvolutionShellComponentClientPrivate *priv;
};

struct _EvolutionShellComponentClientClass {
	GObjectClass parent_class;
};

typedef void (* EvolutionShellComponentClientCallback) (EvolutionShellComponentClient *shell_component_client,
							EvolutionShellComponentResult result,
							void *data);


/* Construction.  */
GtkType                        evolution_shell_component_client_get_type   (void);
void                           evolution_shell_component_client_construct  (EvolutionShellComponentClient *shell_component_client,
									    const char                    *id,
									    CORBA_Object                   corba_object);
EvolutionShellComponentClient *evolution_shell_component_client_new        (const char                    *id,
									    CORBA_Environment             *optional_ev);

GNOME_Evolution_ShellComponent  evolution_shell_component_client_corba_objref  (EvolutionShellComponentClient *client);

/* Properties.  */

const char *evolution_shell_component_client_get_id  (EvolutionShellComponentClient *shell_component_client);

/* Querying DnD interfaces.  */

GNOME_Evolution_ShellComponentDnd_SourceFolder
evolution_shell_component_client_get_dnd_source_interface (EvolutionShellComponentClient *shell_component_client);
GNOME_Evolution_ShellComponentDnd_DestinationFolder
evolution_shell_component_client_get_dnd_destination_interface (EvolutionShellComponentClient *shell_component_client);

/* Querying the offline interface.  */
GNOME_Evolution_Offline
evolution_shell_component_client_get_offline_interface (EvolutionShellComponentClient *shell_component_client);

/* Synchronous operations.  */

EvolutionShellComponentResult  evolution_shell_component_client_set_owner    (EvolutionShellComponentClient  *shell_component_client,
									      GNOME_Evolution_Shell           shell,
									      const char                     *evolution_homedir);
EvolutionShellComponentResult  evolution_shell_component_client_unset_owner  (EvolutionShellComponentClient  *shell_component_client,
									      GNOME_Evolution_Shell                 shell);
EvolutionShellComponentResult  evolution_shell_component_client_create_view  (EvolutionShellComponentClient  *shell_component_client,
									      BonoboUIComponent                *uih,
									      const char                     *physical_uri,
									      const char                     *type_string,
									      const char                     *view_info,
									      BonoboControl                 **control_return);

EvolutionShellComponentResult  evolution_shell_component_client_handle_external_uri  (EvolutionShellComponentClient *shell_component_client,
										      const char                    *uri);

/* Asyncronous operations.  */
void  evolution_shell_component_client_async_create_folder  (EvolutionShellComponentClient         *shell_component_client,
							     const char                            *physical_uri,
							     const char                            *type,
							     EvolutionShellComponentClientCallback  callback,
							     void                                  *data);
void  evolution_shell_component_client_async_remove_folder  (EvolutionShellComponentClient         *shell_component_client,
							     const char                            *physical_uri,
							     const char                            *type,
							     EvolutionShellComponentClientCallback  callback,
							     void                                  *data);
void  evolution_shell_component_client_async_xfer_folder    (EvolutionShellComponentClient         *shell_component_client,
							     const char                            *source_physical_uri,
							     const char                            *destination_physical_uri,
							     const char                            *type,
							     gboolean                               remove_source,
							     EvolutionShellComponentClientCallback  callback,
							     void                                  *data);

void  evolution_shell_component_client_populate_folder_context_menu    (EvolutionShellComponentClient *shell_component_client,
								        BonoboUIContainer             *container,
								        const char                    *physical_uri,
								        const char                    *type);
void  evolution_shell_component_client_unpopulate_folder_context_menu  (EvolutionShellComponentClient *shell_component_client,
									BonoboUIContainer             *container,
									const char                    *physical_uri,
									const char                    *type);

void  evolution_shell_component_client_request_quit  (EvolutionShellComponentClient         *shell_component_client,
						      EvolutionShellComponentClientCallback  callback,
						      void                                  *data);

#ifdef cplusplus
}
#endif /* cplusplus */

#endif /* EVOLUTION_SHELL_COMPONENT_CLIENT_H */
