/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-config-autocompletion.c - Configuration page for addressbook autocompletion.
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
 * Authors: Chris Lahey <clahey@ximian.com>
 *          Chris Toshok <toshok@ximian.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


#include "e-shell-config-autocompletion.h"

#include "e-folder-list.h"

#include "Evolution.h"

#include <bonobo-conf/Bonobo_Config.h>
#include <bonobo/bonobo-exception.h>

#include <libgnome/gnome-i18n.h>
#include <gtk/gtkwidget.h>


typedef struct {
	EvolutionConfigControl *config_control;

	GtkWidget *control_widget;

	Bonobo_ConfigDatabase db;
	EvolutionShellClient *shell_client;
} EvolutionAutocompletionConfig;

static void
folder_list_changed_callback (EFolderList *efl,
			      EvolutionAutocompletionConfig *ac)
{
	evolution_config_control_changed (ac->config_control);
}

static void
config_control_destroy_callback (EvolutionConfigControl *config_control,
				 EvolutionAutocompletionConfig *ac)
{
	bonobo_object_unref (BONOBO_OBJECT (ac->shell_client));
	g_free (ac);
}


static void
config_control_apply_callback (EvolutionConfigControl *config_control,
			       EvolutionAutocompletionConfig *ac)
{
	char *xml;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	xml = e_folder_list_get_xml (E_FOLDER_LIST (ac->control_widget));
	bonobo_config_set_string (ac->db, "/Addressbook/Completion/uris", xml, &ev);
	g_free (xml);

	CORBA_exception_free (&ev);
}

GtkWidget *
e_shell_config_autocompletion_create_widget (EShell *shell, EvolutionConfigControl *config_control)
{
	GNOME_Evolution_Shell shell_dup;
	EvolutionAutocompletionConfig *ac;
	char *xml;
	CORBA_Environment ev;
	static const char *possible_types[] = { "contacts", "ldap-contacts", NULL };

	ac = g_new0 (EvolutionAutocompletionConfig, 1);
	ac->db = e_shell_get_config_db (shell);

	CORBA_exception_init (&ev);

	shell_dup = CORBA_Object_duplicate (bonobo_object_corba_objref (BONOBO_OBJECT (shell)), &ev);
	ac->shell_client = evolution_shell_client_new (shell_dup);

	xml = bonobo_config_get_string (ac->db, "/Addressbook/Completion/uris", &ev);

	ac->control_widget = e_folder_list_new (ac->shell_client, xml);
	g_free (xml);

	gtk_object_set (GTK_OBJECT (ac->control_widget),
			"title", _("Extra Completion folders"),
			"possible_types", possible_types,
			NULL);

	gtk_widget_show (ac->control_widget);

	ac->config_control = config_control;

	gtk_signal_connect (GTK_OBJECT (ac->control_widget), "changed",
			    GTK_SIGNAL_FUNC (folder_list_changed_callback), ac);
	gtk_signal_connect (GTK_OBJECT (ac->config_control), "apply",
			    GTK_SIGNAL_FUNC (config_control_apply_callback), ac);
	gtk_signal_connect (GTK_OBJECT (ac->config_control), "destroy",
			    GTK_SIGNAL_FUNC (config_control_destroy_callback), ac);

	CORBA_exception_free (&ev);

	return ac->control_widget;
}

