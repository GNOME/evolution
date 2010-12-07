/*
 * test-mail-accounts.c
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 */

#include <misc/e-mail-account-manager.h>
#include <misc/e-mail-identity-combo-box.h>

gint
main (gint argc, gchar **argv)
{
	ESourceRegistry *registry;
	GtkWidget *container;
	GtkWidget *widget;
	GtkWidget *window;

	gtk_init (&argc, &argv);

	registry = e_source_registry_get_default ();

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (window), "Mail Sources");
	gtk_widget_set_default_size (GTK_WINDOW (window), 400, 400);
	gtk_container_set_border_width (GTK_CONTAINER (window), 12);
	gtk_widget_show (window);

	g_signal_connect (
		window, "delete-event",
		G_CALLBACK (gtk_main_quit), NULL);

	widget = gtk_vbox_new (FALSE, 12);
	gtk_container_add (GTK_CONTAINER (window), widget);
	gtk_widget_show (widget);

	container = widget;

	widget = e_mail_identity_combo_box_new (registry);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	widget = e_mail_account_manager_new (registry);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	gtk_main ();

	return 0;
}
