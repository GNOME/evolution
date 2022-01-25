/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * SPDX-FileCopyrightText: (C) 2022 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>
#include <e-util/e-util.h>

static gboolean
window_delete_event_cb (GtkWidget *widget,
			GdkEvent *event,
			gpointer user_data)
{
	gtk_main_quit ();

	return FALSE;
}

static gint
on_idle_create_widget (gpointer user_data)
{
	GtkWidget *window, *editor;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size (GTK_WINDOW (window), 640, 480);

	g_signal_connect (
		window, "delete-event",
		G_CALLBACK (window_delete_event_cb), NULL);

	editor = e_markdown_editor_new ();

	g_object_set (G_OBJECT (editor),
		"halign", GTK_ALIGN_FILL,
		"hexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"visible", TRUE,
		NULL);

	gtk_container_add (GTK_CONTAINER (window), editor);

	gtk_widget_show (window);

	return FALSE;
}

gint
main (gint argc,
      gchar **argv)
{
	GList *modules;

	bindtextdomain (GETTEXT_PACKAGE, EVOLUTION_LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gtk_init (&argc, &argv);

	e_util_init_main_thread (NULL);
	e_passwords_init ();

	g_setenv ("EVOLUTION_SOURCE_WEBKITDATADIR", EVOLUTION_SOURCE_WEBKITDATADIR, FALSE);

	gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (), EVOLUTION_ICONDIR);
	gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (), E_DATA_SERVER_ICONDIR);

	modules = e_module_load_all_in_directory (EVOLUTION_MODULEDIR);
	g_list_free_full (modules, (GDestroyNotify) g_type_module_unuse);

	g_idle_add ((GSourceFunc) on_idle_create_widget, NULL);

	gtk_main ();

	e_misc_util_free_global_memory ();

	return 0;
}
