/* test-name-selector.c - Test for name selector components.
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
 * Author: Hans Petter Jansson <hpj@novell.com>
 */

#include <camel/camel.h>
#include <e-util/e-util.h>

static ENameSelectorDialog *name_selector_dialog;
static GtkWidget           *name_selector_entry_window;

static void
close_dialog (GtkWidget *widget,
              gint response,
              gpointer data)
{
	gtk_widget_destroy (GTK_WIDGET (name_selector_dialog));
	gtk_widget_destroy (name_selector_entry_window);

	g_timeout_add (4000, (GSourceFunc) gtk_main_quit, NULL);
}

static gboolean
start_test (EClientCache *client_cache)
{
	ENameSelectorModel *name_selector_model;
	EDestinationStore *destination_store;
	GtkWidget *name_selector_entry;
	GtkWidget *container;

	destination_store = e_destination_store_new ();
	name_selector_model = e_name_selector_model_new ();

	e_name_selector_model_add_section (name_selector_model, "to", "To", destination_store);
	e_name_selector_model_add_section (name_selector_model, "cc", "Cc", NULL);
	e_name_selector_model_add_section (name_selector_model, "bcc", "Bcc", NULL);

	name_selector_dialog = e_name_selector_dialog_new (client_cache);
	e_name_selector_dialog_set_model (name_selector_dialog, name_selector_model);
	gtk_window_set_modal (GTK_WINDOW (name_selector_dialog), FALSE);

	name_selector_entry = e_name_selector_entry_new (client_cache);
	e_name_selector_entry_set_destination_store (
		E_NAME_SELECTOR_ENTRY (name_selector_entry),
		destination_store);

	g_signal_connect (
		name_selector_dialog, "response",
		G_CALLBACK (close_dialog), name_selector_dialog);

	gtk_widget_show (GTK_WIDGET (name_selector_dialog));

	container = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_container_add (GTK_CONTAINER (container), name_selector_entry);
	gtk_widget_show_all (container);

	name_selector_entry_window = container;

	g_object_unref (name_selector_model);
	g_object_unref (destination_store);
	return FALSE;
}

gint
main (gint argc,
      gchar **argv)
{
	ESourceRegistry *registry;
	EClientCache *client_cache;
	GError *error = NULL;

	gtk_init (&argc, &argv);

	camel_init (NULL, 0);

	registry = e_source_registry_new_sync (NULL, &error);

	if (error != NULL) {
		g_error (
			"Failed to load ESource registry: %s",
			error->message);
		g_return_val_if_reached (-1);
	}

	client_cache = e_client_cache_new (registry);

	g_idle_add ((GSourceFunc) start_test, client_cache);

	gtk_main ();

	g_object_unref (registry);
	g_object_unref (client_cache);
	e_misc_util_free_global_memory ();

	return 0;
}
