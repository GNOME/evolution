/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-config.c
 *
 * Copyright (C) 2002 Ximian, Inc.
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

#include "e-shell-config.h"

#include "e-shell-config-folder-settings.h"
#include "evolution-config-control.h"
#include "evolution-folder-selector-button.h"

#include <string.h>
#include <bonobo/bonobo-generic-factory.h>


#define E_SHELL_CONFIG_FACTORY_OAFIID "OAFIID:GNOME_Evolution_Shell_Config_Factory:" BASE_VERSION

#define E_SHELL_CONFIG_FOLDER_SETTINGS_OAFIID "OAFIID:GNOME_Evolution_Shell_Config_FolderSettings_Control:" BASE_VERSION


static BonoboObject *
config_control_factory_cb (BonoboGenericFactory *factory,
			   const char *component_id,
			   gpointer shell)
{
	if (!strcmp (component_id, E_SHELL_CONFIG_FOLDER_SETTINGS_OAFIID))
		return e_shell_config_folder_settings_create_control (shell);
	else {
		g_assert_not_reached();
		return NULL;
	}
}

gboolean
e_shell_config_factory_register (EShell *shell)
{
	BonoboGenericFactory *factory;

	g_return_val_if_fail (E_IS_SHELL (shell), FALSE);

	factory = bonobo_generic_factory_new (E_SHELL_CONFIG_FACTORY_OAFIID,
					      config_control_factory_cb,
					      shell);

	if (factory == NULL) {
		g_warning ("Cannot register factory %s", E_SHELL_CONFIG_FACTORY_OAFIID);
		return FALSE;
	}
	return TRUE;
}
