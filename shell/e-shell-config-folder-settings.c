/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-config-folder-settings.c - Configuration page for folder settings.
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
 *
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


#include "e-shell-config-folder-settings.h"
#include "e-shell-config-offline.h"
#include "e-shell-config-autocompletion.h"
#include "e-shell-config-default-folders.h"

#include "evolution-config-control.h"
#include "e-storage-set-view.h"

#include "Evolution.h"

#include <bonobo/bonobo-exception.h>

#include <libgnome/gnome-i18n.h>
#include <gtk/gtknotebook.h>
#include <gtk/gtklabel.h>


static void
append_to_notebook (GtkWidget *notebook, char *label_str,
		    GtkWidget *child)
{
	GtkWidget *label;

	label = gtk_label_new (label_str);

	gtk_notebook_append_page (GTK_NOTEBOOK(notebook), child, label);
	gtk_widget_show (label);
	gtk_widget_show (child);
}

BonoboObject *
e_shell_config_folder_settings_create_control (EShell *shell)
{
	GtkWidget *notebook;
	EvolutionConfigControl *control;

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	notebook = gtk_notebook_new ();

	control = evolution_config_control_new (notebook);

	append_to_notebook (notebook, _("Default Folders"),
			    e_shell_config_default_folders_create_widget (shell, control));

	append_to_notebook (notebook, _("Offline Folders"),
			    e_shell_config_offline_create_widget (shell, control));

	append_to_notebook (notebook, _("Autocompletion Folders"),
			    e_shell_config_autocompletion_create_widget (shell, control));

	gtk_widget_show (notebook);

	return BONOBO_OBJECT (control);
}
