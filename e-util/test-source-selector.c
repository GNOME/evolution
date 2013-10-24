/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* test-source-list-selector.c - Test program for the ESourceListSelector
 * widget.
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#include <e-util/e-util.h>

static const gchar *extension_name;

static void
dump_selection (ESourceSelector *selector)
{
	GList *list, *link;

	list = e_source_selector_get_selection (selector);

	g_print ("Current selection:\n");

	if (list == NULL)
		g_print ("\t(None)\n");

	for (link = list; link != NULL; link = g_list_next (link->next)) {
		ESource *source = E_SOURCE (link->data);
		ESourceBackend *extension;

		extension = e_source_get_extension (source, extension_name);

		g_print (
			"\tSource %s (backend %s)\n",
			e_source_get_display_name (source),
			e_source_backend_get_backend_name (extension));
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);
}

static void
selection_changed_callback (ESourceSelector *selector)
{
	g_print ("Selection changed!\n");
	dump_selection (selector);
}

static gint
on_idle_create_widget (ESourceRegistry *registry)
{
	GtkWidget *window;
	GtkWidget *vgrid;
	GtkWidget *selector;
	GtkWidget *scrolled_window;
	GtkWidget *check;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size (GTK_WINDOW (window), 200, 300);

	g_signal_connect (
		window, "delete-event",
		G_CALLBACK (gtk_main_quit), NULL);

	vgrid = g_object_new (
		GTK_TYPE_GRID,
		"orientation", GTK_ORIENTATION_VERTICAL,
		"column-homogeneous", FALSE,
		"row-spacing", 6,
		NULL);
	gtk_container_add (GTK_CONTAINER (window), vgrid);

	selector = e_source_selector_new (registry, extension_name);
	g_signal_connect (
		selector, "selection_changed",
		G_CALLBACK (selection_changed_callback), NULL);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (scrolled_window),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (scrolled_window), selector);
	gtk_widget_set_hexpand (scrolled_window, TRUE);
	gtk_widget_set_halign (scrolled_window, GTK_ALIGN_FILL);
	gtk_widget_set_vexpand (scrolled_window, TRUE);
	gtk_widget_set_valign (scrolled_window, GTK_ALIGN_FILL);
	gtk_container_add (GTK_CONTAINER (vgrid), scrolled_window);

	check = gtk_check_button_new_with_label ("Show colors");
	gtk_widget_set_halign (check, GTK_ALIGN_FILL);
	gtk_container_add (GTK_CONTAINER (vgrid), check);

	g_object_bind_property (
		selector, "show-colors",
		check, "active",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	check = gtk_check_button_new_with_label ("Show toggles");
	gtk_widget_set_halign (check, GTK_ALIGN_FILL);
	gtk_container_add (GTK_CONTAINER (vgrid), check);

	g_object_bind_property (
		selector, "show-toggles",
		check, "active",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	gtk_widget_show_all (window);

	return FALSE;
}

gint
main (gint argc,
      gchar **argv)
{
	ESourceRegistry *registry;
	GError *error = NULL;

	gtk_init (&argc, &argv);

	if (argc < 2)
		extension_name = E_SOURCE_EXTENSION_ADDRESS_BOOK;
	else
		extension_name = argv[1];

	registry = e_source_registry_new_sync (NULL, &error);

	if (error != NULL) {
		g_error (
			"Failed to load ESource registry: %s",
			error->message);
		g_assert_not_reached ();
	}

	g_idle_add ((GSourceFunc) on_idle_create_widget, registry);

	gtk_main ();

	return 0;
}
