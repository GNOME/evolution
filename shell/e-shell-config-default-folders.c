/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-config-default-folders.c - Configuration page for specifying default
 * folders.
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
 * Author: Dan Winship <danw@ximian.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-shell-config-default-folders.h"

#include "evolution-folder-selector-button.h"

#include "e-util/e-config-listener.h"

#include <glade/glade-xml.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtksignal.h>

#include <libgnome/gnome-i18n.h>


typedef struct {
	GladeXML *glade;
	EvolutionConfigControl *config_control;

	char *mail_uri, *mail_path;
	char *contacts_uri, *contacts_path;
	char *calendar_uri, *calendar_path;
	char *tasks_uri, *tasks_path;

	EConfigListener *config_listener;
	EvolutionShellClient *shell_client;
} EvolutionDefaultFolderConfig;

static void
folder_selected (EvolutionFolderSelectorButton *button,
		 GNOME_Evolution_Folder *folder,
		 EvolutionDefaultFolderConfig *dfc)
{
	char **uri_ptr, **path_ptr;

	uri_ptr = gtk_object_get_data (GTK_OBJECT (button), "uri_ptr");
	path_ptr = gtk_object_get_data (GTK_OBJECT (button), "path_ptr");

	g_free (*uri_ptr);
	g_free (*path_ptr);
	*uri_ptr = g_strdup (folder->physicalUri);
	*path_ptr = g_strdup (folder->evolutionUri);

	evolution_config_control_changed (dfc->config_control);
}

GtkWidget *e_shell_config_default_folder_selector_button_new (char *widget_name, char *string1, char *string2, int int1, int int2);

GtkWidget *
e_shell_config_default_folder_selector_button_new (char *widget_name,
						   char *string1,
						   char *string2,
						   int int1, int int2)
{
	return (GtkWidget *) g_object_new (EVOLUTION_TYPE_FOLDER_SELECTOR_BUTTON, NULL);
}

static void
config_control_apply_cb (EvolutionConfigControl *control,
			 EvolutionDefaultFolderConfig *dfc)
{
	e_config_listener_set_string (dfc->config_listener, "/DefaultFolders/mail_path", dfc->mail_path);
	e_config_listener_set_string (dfc->config_listener, "/DefaultFolders/mail_uri", dfc->mail_uri);
	e_config_listener_set_string (dfc->config_listener, "/DefaultFolders/contacts_path", dfc->contacts_path);
	e_config_listener_set_string (dfc->config_listener, "/DefaultFolders/contacts_uri", dfc->contacts_uri);
	e_config_listener_set_string (dfc->config_listener, "/DefaultFolders/calendar_path", dfc->calendar_path);
	e_config_listener_set_string (dfc->config_listener, "/DefaultFolders/calendar_uri", dfc->calendar_uri);
	e_config_listener_set_string (dfc->config_listener, "/DefaultFolders/tasks_path", dfc->tasks_path);
	e_config_listener_set_string (dfc->config_listener, "/DefaultFolders/tasks_uri", dfc->tasks_uri);
}

static void
config_control_destroy_cb (EvolutionConfigControl *config_control,
			   EvolutionDefaultFolderConfig *dfc)
{
	g_object_unref (dfc->config_listener);

	g_free (dfc->mail_uri);
	g_free (dfc->mail_path);
	g_free (dfc->contacts_uri);
	g_free (dfc->contacts_path);
	g_free (dfc->calendar_uri);
	g_free (dfc->calendar_path);
	g_free (dfc->tasks_uri);
	g_free (dfc->tasks_path);

	g_object_unref (dfc->glade);
	bonobo_object_unref (BONOBO_OBJECT (dfc->shell_client));
	g_free (dfc);
}

static const char *mail_types[] = { "mail", NULL };
static const char *contacts_types[] = { "contacts", "ldap-contacts", NULL };
static const char *calendar_types[] = { "calendar", NULL };
static const char *tasks_types[] = { "tasks", NULL };

static void
setup_folder_selector (EvolutionDefaultFolderConfig *dfc,
		       const char *widget_name,
		       char **path_ptr, char *path_dbpath,
		       char **uri_ptr, char *uri_dbpath,
		       const char **types)
{
	GtkWidget *button;

	*path_ptr = e_config_listener_get_string_with_default (dfc->config_listener, path_dbpath, NULL, NULL);
	*uri_ptr = e_config_listener_get_string_with_default (dfc->config_listener, uri_dbpath, NULL, NULL);

	button = glade_xml_get_widget (dfc->glade, widget_name);
	evolution_folder_selector_button_construct (
		EVOLUTION_FOLDER_SELECTOR_BUTTON (button),
		dfc->shell_client, _("Select Default Folder"),
		*uri_ptr, types);
	g_object_set_data (G_OBJECT (button), "uri_ptr", uri_ptr);
	g_object_set_data (G_OBJECT (button), "path_ptr", path_ptr);
	g_signal_connect (button, "selected",
			  G_CALLBACK (folder_selected),
			  dfc);
}

GtkWidget*
e_shell_config_default_folders_create_widget (EShell *shell, EvolutionConfigControl *config_control)
{
	GNOME_Evolution_Shell shell_dup;
	CORBA_Environment ev;
	EvolutionDefaultFolderConfig *dfc;
	GtkWidget *widget;

	dfc = g_new0 (EvolutionDefaultFolderConfig, 1);

	dfc->config_listener = e_config_listener_new ();

	CORBA_exception_init (&ev);
	shell_dup = CORBA_Object_duplicate (BONOBO_OBJREF (shell), &ev);
	CORBA_exception_free (&ev);
	dfc->shell_client = evolution_shell_client_new (shell_dup);

	dfc->glade = glade_xml_new (EVOLUTION_GLADEDIR "/e-shell-config-default-folders.glade", NULL, NULL);

	setup_folder_selector (dfc, "default_mail_button",
			       &dfc->mail_path, "/DefaultFolders/mail_path",
			       &dfc->mail_uri, "/DefaultFolders/mail_uri",
			       mail_types);
	setup_folder_selector (dfc, "default_contacts_button",
			       &dfc->contacts_path, "/DefaultFolders/contacts_path",
			       &dfc->contacts_uri, "/DefaultFolders/contacts_uri",
			       contacts_types);
	setup_folder_selector (dfc, "default_calendar_button",
			       &dfc->calendar_path, "/DefaultFolders/calendar_path",
			       &dfc->calendar_uri, "/DefaultFolders/calendar_uri",
			       calendar_types);
	setup_folder_selector (dfc, "default_tasks_button",
			       &dfc->tasks_path, "/DefaultFolders/tasks_path",
			       &dfc->tasks_uri, "/DefaultFolders/tasks_uri",
			       tasks_types);

	widget = glade_xml_get_widget (dfc->glade, "default_folders_table");
	gtk_widget_ref (widget);
	gtk_container_remove (GTK_CONTAINER (widget->parent), widget);
	gtk_widget_show (widget);
	dfc->config_control = config_control;

	g_signal_connect (dfc->config_control, "apply",
			  G_CALLBACK (config_control_apply_cb), dfc);
	g_signal_connect (dfc->config_control, "destroy",
			  G_CALLBACK (config_control_destroy_cb), dfc);

	return widget;
}
