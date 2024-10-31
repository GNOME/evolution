/* test-source-combo-box.c - Test for ESourceComboBox.
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#include "evolution-config.h"

#include <gtk/gtk.h>

#include <e-util/e-util.h>

static const gchar *extension_name;

static void
source_changed_cb (ESourceComboBox *combo_box)
{
	ESource *source;

	source = e_source_combo_box_ref_active (combo_box);
	if (source != NULL) {
		const gchar *display_name;
		display_name = e_source_get_display_name (source);
		g_print ("source selected: \"%s\"\n", display_name);
		g_object_unref (source);
	} else {
		g_print ("source selected: (none)\n");
	}
}

static gint
on_idle_create_widget (ESourceRegistry *registry)
{
	GtkWidget *window;
	GtkWidget *box;
	GtkWidget *combo_box;
	GtkWidget *button;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_container_add (GTK_CONTAINER (window), box);

	combo_box = e_source_combo_box_new (registry, extension_name);
	g_signal_connect (
		combo_box, "changed",
		G_CALLBACK (source_changed_cb), NULL);
	gtk_box_pack_start (GTK_BOX (box), combo_box, FALSE, FALSE, 0);

	button = gtk_toggle_button_new_with_label ("Show Colors");
	gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);

	e_binding_bind_property (
		combo_box, "show-colors",
		button, "active",
		G_BINDING_SYNC_CREATE |
		G_BINDING_BIDIRECTIONAL);

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
		g_return_val_if_reached (-1);
	}

	g_idle_add ((GSourceFunc) on_idle_create_widget, registry);

	gtk_main ();

	e_misc_util_free_global_memory ();

	return 0;
}
