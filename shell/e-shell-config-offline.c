/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-config-offline.c - Configuration page for offline synchronization.
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


#include "e-shell-config-offline.h"

#include "evolution-config-control.h"
#include "e-storage-set-view.h"

#include "Evolution.h"

#include <gconf/gconf-client.h>

#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtksignal.h>


struct _PageData {
	EShell *shell;
	GtkWidget *storage_set_view;
	EvolutionConfigControl *config_control;
};
typedef struct _PageData PageData;


/* Callbacks.  */

static void
config_control_destroy_notify (void *data,
			       GObject *where_the_config_control_was)
{
	PageData *page_data;

	page_data = (PageData *) data;
	gtk_widget_destroy (page_data->storage_set_view);
	g_free (page_data);
}

static void
config_control_apply_callback (EvolutionConfigControl *config_control,
			       void *data)
{
	GConfClient *gconf_client;
	PageData *page_data;
	GSList *checked_paths;

	page_data = (PageData *) data;

	checked_paths = e_storage_set_view_get_checkboxes_list (E_STORAGE_SET_VIEW (page_data->storage_set_view));

	gconf_client = gconf_client_get_default ();

	gconf_client_set_list (gconf_client, "/apps/evolution/shell/offline/folder_paths",
			       GCONF_VALUE_STRING, checked_paths, NULL);

	g_slist_foreach (checked_paths, (GFunc) g_free, NULL);
	g_slist_free (checked_paths);

	g_object_unref (gconf_client);
}

static void
storage_set_view_checkboxes_changed_callback (EStorageSetView *storage_set_view,
					      void *data)
{
	PageData *page_data;

	page_data = (PageData *) data;
	evolution_config_control_changed (page_data->config_control);
}


/* Construction.  */

static void
init_storage_set_view_status_from_config (EStorageSetView *storage_set_view,
					  EShell *shell)
{
	GConfClient *gconf_client;
	GSList *list;

	gconf_client = gconf_client_get_default ();

	list = gconf_client_get_list (gconf_client, "/apps/evolution/shell/offline/folder_paths",
				      GCONF_VALUE_STRING, NULL);

	e_storage_set_view_set_checkboxes_list (storage_set_view, list);

	g_slist_foreach (list, (GFunc) g_free, NULL);
	g_slist_free (list);

	g_object_unref (gconf_client);
}

static gboolean
storage_set_view_has_checkbox_func (EStorageSet *storage_set,
				    const char *path,
				    void *data)
{
	EFolder *folder;

	folder = e_storage_set_get_folder (storage_set, path);
	if (folder == NULL)
		return FALSE;

	return e_folder_get_can_sync_offline (folder);
}

GtkWidget *
e_shell_config_offline_create_widget (EShell *shell, EvolutionConfigControl *control)
{
	PageData *page_data;
	GtkWidget *scrolled_window;

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	page_data = g_new (PageData, 1);
	page_data->shell = shell;

	page_data->storage_set_view = e_storage_set_create_new_view (e_shell_get_storage_set (shell), NULL);
        e_storage_set_view_set_show_checkboxes (E_STORAGE_SET_VIEW (page_data->storage_set_view), TRUE,
						storage_set_view_has_checkbox_func, NULL);
	gtk_widget_show (page_data->storage_set_view);

	init_storage_set_view_status_from_config (E_STORAGE_SET_VIEW (page_data->storage_set_view), shell);
	g_signal_connect (page_data->storage_set_view, "checkboxes_changed",
			  G_CALLBACK (storage_set_view_checkboxes_changed_callback), page_data);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (scrolled_window), page_data->storage_set_view);
	gtk_widget_show (scrolled_window);

	page_data->config_control = control;

	g_signal_connect (page_data->config_control, "apply",
			  G_CALLBACK (config_control_apply_callback), page_data);

	g_object_weak_ref (G_OBJECT (page_data->config_control), config_control_destroy_notify, page_data);

	return scrolled_window;
}
