/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-shell-client.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gal/util/e-util.h>

#include "evolution-shell-client.h"
#include "e-shell-corba-icon-utils.h"

#define PARENT_TYPE G_TYPE_OBJECT


static void
class_init (EvolutionShellClientClass *klass)
{
}

static void
init (EvolutionShellClient *shell_client)
{
}


void
evolution_shell_client_construct (EvolutionShellClient *shell_client,
				  GNOME_Evolution_Shell corba_shell)
{
	g_assert_not_reached ();
}

EvolutionShellClient *
evolution_shell_client_new (GNOME_Evolution_Shell corba_shell)
{
	g_assert_not_reached ();
	return NULL;
}

GNOME_Evolution_Shell
evolution_shell_client_corba_objref (EvolutionShellClient *shell_client)
{
	g_return_val_if_fail (EVOLUTION_IS_SHELL_CLIENT (shell_client), CORBA_OBJECT_NIL);
	return CORBA_OBJECT_NIL;
}


void
evolution_shell_client_user_select_folder (EvolutionShellClient *shell_client,
					   GtkWindow *parent,
					   const char *title,
					   const char *default_folder,
					   const char **possible_types,
					   GNOME_Evolution_Folder **folder_return)
{
	g_assert_not_reached ();
}


GNOME_Evolution_Activity
evolution_shell_client_get_activity_interface (EvolutionShellClient *shell_client)
{
	g_assert_not_reached ();
	return CORBA_OBJECT_NIL;
}

GNOME_Evolution_Shortcuts
evolution_shell_client_get_shortcuts_interface  (EvolutionShellClient *shell_client)
{
	g_assert_not_reached ();
	return CORBA_OBJECT_NIL;
}

GNOME_Evolution_StorageRegistry
evolution_shell_client_get_storage_registry_interface (EvolutionShellClient *shell_client)
{
	g_assert_not_reached ();
	return CORBA_OBJECT_NIL;
}


GNOME_Evolution_Storage
evolution_shell_client_get_local_storage (EvolutionShellClient *shell_client)
{
	g_assert_not_reached ();
	return CORBA_OBJECT_NIL;
}

void
evolution_shell_client_set_line_status (EvolutionShellClient *shell_client,
					gboolean              line_status)
{
	g_assert_not_reached ();
}


GdkPixbuf *
evolution_shell_client_get_pixbuf_for_type (EvolutionShellClient *shell_client,
					    const char *folder_type,
					    gboolean mini)
{
	g_assert_not_reached ();
	return NULL;
}


GtkWidget *
evolution_shell_client_create_storage_set_view (EvolutionShellClient *shell_client,
						Bonobo_UIComponent uic,
						Bonobo_Control *bonobo_control_iface_return,
						GNOME_Evolution_StorageSetView *storage_set_view_iface_return,
						CORBA_Environment *ev)
{
	g_assert_not_reached ();
	return NULL;
}


E_MAKE_TYPE (evolution_shell_client, "EvolutionShellClient", EvolutionShellClient, class_init, init, PARENT_TYPE)
