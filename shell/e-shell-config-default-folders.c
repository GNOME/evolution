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

#include <glade/glade-xml.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtksignal.h>

#include <libgnome/gnome-i18n.h>

#include <gconf/gconf-client.h>


typedef struct {
	GladeXML *glade;
	EvolutionConfigControl *config_control;

	char *mail_uri, *mail_path;
	char *contacts_uri, *contacts_path;
	char *calendar_uri, *calendar_path;
	char *tasks_uri, *tasks_path;

	EvolutionShellClient *shell_client;
} EvolutionDefaultFolderConfig;

static void
folder_selected (EvolutionFolderSelectorButton *button,
		 GNOME_Evolution_Folder *folder,
		 EvolutionDefaultFolderConfig *dfc)
{
	char **uri_ptr, **path_ptr;

	uri_ptr = g_object_get_data (G_OBJECT (button), "uri_ptr");
	path_ptr = g_object_get_data (G_OBJECT (button), "path_ptr");

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
	GConfClient *client;

	client = gconf_client_get_default ();

	gconf_client_set_string (client, "/apps/evolution/shell/default_folders/mail_path", dfc->mail_path, NULL);
	gconf_client_set_string (client, "/apps/evolution/shell/default_folders/mail_uri", dfc->mail_uri, NULL);
	gconf_client_set_string (client, "/apps/evolution/shell/default_folders/contacts_path", dfc->contacts_path, NULL);
	gconf_client_set_string (client, "/apps/evolution/shell/default_folders/contacts_uri", dfc->contacts_uri, NULL);
	gconf_client_set_string (client, "/apps/evolution/shell/default_folders/calendar_path", dfc->calendar_path, NULL);
	gconf_client_set_string (client, "/apps/evolution/shell/default_folders/calendar_uri", dfc->calendar_uri, NULL);
	gconf_client_set_string (client, "/apps/evolution/shell/default_folders/tasks_path", dfc->tasks_path, NULL);
	gconf_client_set_string (client, "/apps/evolution/shell/default_folders/tasks_uri", dfc->tasks_uri, NULL);


	g_object_unref (client);
}

static void
config_control_destroy_notify (void *data,
			       GObject *where_the_config_control_was)
{
	EvolutionDefaultFolderConfig *dfc = (EvolutionDefaultFolderConfig *) data;

	g_free (dfc->mail_uri);
	g_free (dfc->mail_path);
	g_free (dfc->contacts_uri);
	g_free (dfc->contacts_path);
	g_free (dfc->calendar_uri);
	g_free (dfc->calendar_path);
	g_free (dfc->tasks_uri);
	g_free (dfc->tasks_path);

	g_object_unref (dfc->glade);
	g_object_unref (dfc->shell_client);

	g_free (dfc);
}

static const char *mail_types[] = { "mail", NULL };
static const char *contacts_types[] = { "contacts", "contacts/ldap", NULL };
static const char *calendar_types[] = { "calendar", NULL };
static const char *tasks_types[] = { "tasks", NULL };

static void
setup_folder_selector (EvolutionDefaultFolderConfig *dfc,
		       const char *widget_name,
		       char **path_ptr, char *path_dbpath,
		       char **uri_ptr, char *uri_dbpath,
		       const char **types)
{
	GConfClient *client;
	GtkWidget *button;

	client = gconf_client_get_default ();

	*path_ptr = gconf_client_get_string (client, path_dbpath, NULL);
	*uri_ptr = gconf_client_get_string (client, uri_dbpath, NULL);

	g_object_unref (client);

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

	/* XXX libglade2 seems to not show custom widgets even when
	   they're flagged Visible.*/
	gtk_widget_show (button);
}

GtkWidget*
e_shell_config_default_folders_create_widget (EShell *shell, EvolutionConfigControl *config_control)
{
	CORBA_Environment ev;
	EvolutionDefaultFolderConfig *dfc;
	GtkWidget *widget;

	dfc = g_new0 (EvolutionDefaultFolderConfig, 1);

	dfc->shell_client = evolution_shell_client_new (BONOBO_OBJREF (shell));

	dfc->glade = glade_xml_new (EVOLUTION_GLADEDIR "/e-shell-config-default-folders.glade", NULL, NULL);

	setup_folder_selector (dfc, "default_mail_button",
			       &dfc->mail_path, "/apps/evolution/shell/default_folders/mail_path",
			       &dfc->mail_uri, "/apps/evolution/shell/default_folders/mail_uri",
			       mail_types);
	setup_folder_selector (dfc, "default_contacts_button",
			       &dfc->contacts_path, "/apps/evolution/shell/default_folders/contacts_path",
			       &dfc->contacts_uri, "/apps/evolution/shell/default_folders/contacts_uri",
			       contacts_types);
	setup_folder_selector (dfc, "default_calendar_button",
			       &dfc->calendar_path, "/apps/evolution/shell/default_folders/calendar_path",
			       &dfc->calendar_uri, "/apps/evolution/shell/default_folders/calendar_uri",
			       calendar_types);
	setup_folder_selector (dfc, "default_tasks_button",
			       &dfc->tasks_path, "/apps/evolution/shell/default_folders/tasks_path",
			       &dfc->tasks_uri, "/apps/evolution/shell/default_folders/tasks_uri",
			       tasks_types);

	widget = glade_xml_get_widget (dfc->glade, "default_folders_table");
	gtk_widget_ref (widget);
	gtk_container_remove (GTK_CONTAINER (widget->parent), widget);
	gtk_widget_show (widget);
	dfc->config_control = config_control;

	g_signal_connect (dfc->config_control, "apply",
			  G_CALLBACK (config_control_apply_cb), dfc);

	g_object_weak_ref (G_OBJECT (dfc->config_control), config_control_destroy_notify, dfc);

	return widget;
}
