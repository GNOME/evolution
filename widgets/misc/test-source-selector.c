/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* test-source-list-selector.c - Test program for the ESourceListSelector
 * widget.
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

#include "e-source-selector.h"

#include <gtk/gtkmain.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkwindow.h>

#include <libgnomeui/gnome-ui-init.h>


static void
dump_selection (ESourceSelector *selector)
{
	GSList *selection = e_source_selector_get_selection (selector);

	g_print ("Current selection:\n");
	if (selection == NULL) {
		g_print ("\t(None)\n");
	} else {
		GSList *p;

		for (p = selection; p != NULL; p = p->next) {
			ESource *source = E_SOURCE (p->data);

			g_print ("\tSource %s (group %s)\n",
				 e_source_peek_name (source),
				 e_source_group_peek_name (e_source_peek_group (source)));
		}
	}

	e_source_selector_free_selection (selection);
}

static void
selection_changed_callback (ESourceSelector *selector,
			    void *unused_data)
{
	g_print ("Selection changed!\n");
	dump_selection (selector);
}

static void
check_toggled_callback (GtkToggleButton *button,
			ESourceSelector *selector)
{
	e_source_selector_show_selection (selector, gtk_toggle_button_get_active (button));
}

static int
on_idle_create_widget (void *unused_data)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *selector;
	GtkWidget *scrolled_window;
	GtkWidget *check;
	ESourceList *list;
	GConfClient *gconf_client;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size (GTK_WINDOW (window), 200, 300);

	vbox = gtk_vbox_new (FALSE, 3);
	gtk_container_add (GTK_CONTAINER (window), vbox);

	gconf_client = gconf_client_get_default ();
	list = e_source_list_new_for_gconf (gconf_client, "/apps/evolution/test/source_list");
	selector = e_source_selector_new (list);
	g_signal_connect (selector, "selection_changed", G_CALLBACK (selection_changed_callback), NULL);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (scrolled_window), selector);
	gtk_box_pack_start (GTK_BOX (vbox), scrolled_window, TRUE, TRUE, 3);

	check = gtk_check_button_new_with_label ("Show checkboxes");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check),
				      e_source_selector_selection_shown (E_SOURCE_SELECTOR (selector)));
	g_signal_connect (check, "toggled", G_CALLBACK (check_toggled_callback), selector);
	gtk_box_pack_start (GTK_BOX (vbox), check, FALSE, TRUE, 3);

	gtk_widget_show_all (window);

	g_object_unref (gconf_client);
	return FALSE;
}


int
main (int argc, char **argv)
{
	GnomeProgram *program;

	program = gnome_program_init ("test-source-list", "0.0",
				      LIBGNOMEUI_MODULE, argc, argv,
				      NULL);

	g_idle_add (on_idle_create_widget, NULL);
	gtk_main ();

	return 0;
}
