/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-shell-client.h
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

/* FIXME: This file will have to go away.  */

#ifndef __EVOLUTION_SHELL_CLIENT_H__
#define __EVOLUTION_SHELL_CLIENT_H__

#include <gtk/gtkwindow.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <glib-object.h>

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
	GObject base;
};

struct _EvolutionShellClientClass {
	GObjectClass parent_class;
};


GtkType                 evolution_shell_client_get_type            (void);
void                    evolution_shell_client_construct           (EvolutionShellClient  *shell_client,
								    GNOME_Evolution_Shell        corba_shell);
EvolutionShellClient   *evolution_shell_client_new                 (GNOME_Evolution_Shell        shell);

GNOME_Evolution_Shell evolution_shell_client_corba_objref (EvolutionShellClient *shell_client);

void                    evolution_shell_client_user_select_folder  (EvolutionShellClient    *shell_client,
								    GtkWindow               *parent,
								    const char              *title,
								    const char              *default_folder,
								    const char             **possible_types,
								    GNOME_Evolution_Folder **folder_return);

GNOME_Evolution_Activity        evolution_shell_client_get_activity_interface         (EvolutionShellClient *shell_client);
GNOME_Evolution_Shortcuts       evolution_shell_client_get_shortcuts_interface        (EvolutionShellClient *shell_client);
GNOME_Evolution_StorageRegistry evolution_shell_client_get_storage_registry_interface (EvolutionShellClient *shell_client);

GNOME_Evolution_Storage  evolution_shell_client_get_local_storage        (EvolutionShellClient *shell_client);

void                     evolution_shell_client_set_line_status          (EvolutionShellClient *shell_client,
									  gboolean              online);

GdkPixbuf               *evolution_shell_client_get_pixbuf_for_type      (EvolutionShellClient *shell_client,
									  const char           *folder_type,
									  gboolean              mini);

GtkWidget *evolution_shell_client_create_storage_set_view  (EvolutionShellClient           *shell_client,
							    Bonobo_UIComponent              uic,
							    Bonobo_Control                 *bonobo_control_iface_return,
							    GNOME_Evolution_StorageSetView *storage_set_view_iface_return,
							    CORBA_Environment              *ev);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __EVOLUTION_SHELL_CLIENT_H__ */
