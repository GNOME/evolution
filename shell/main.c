/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* .c
 *
 * Copyright (C) 2000  Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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
 * Author: Ettore Perazzoli
 */

#include <gnome.h>
#include <bonobo.h>
#include <libgnomeui/gnome-window-icon.h>

#include "e-util/e-gui-utils.h"
#include "e-setup.h"

#include "e-shell.h"


#define STARTUP_URI "evolution:/local/Inbox"

static EShell *shell;


static void
no_views_left_cb (EShell *shell, gpointer data)
{
	gtk_main_quit ();
}

static void
destroy_cb (GtkObject *object, gpointer data)
{
	gtk_main_quit ();
}


#ifdef USING_OAF

#include <liboaf/liboaf.h>

static void
init_corba (int *argc, char **argv)
{
	gnome_init_with_popt_table ("Evolution", VERSION, *argc, argv, oaf_popt_options, 0, NULL);

	oaf_init (*argc, argv);
}

#else  /* USING_OAF */

#include <libgnorba/gnorba.h>

static void
init_corba (int *argc, char **argv)
{
	CORBA_Environment ev;
	
	CORBA_exception_init (&ev);

	gnome_CORBA_init_with_popt_table ("Evolution", VERSION, argc, argv,
					  NULL, 0, NULL,
					  GNORBA_INIT_SERVER_FUNC, &ev);

	if (ev._major != CORBA_NO_EXCEPTION)
		g_error ("Cannot initialize GNOME");

	CORBA_exception_free (&ev);
}

#endif /* USING_OAF */


static void
development_warning ()
{
	GtkWidget *label, *warning_dialog;

	warning_dialog = gnome_dialog_new (
		"Evolution 0.0",
		GNOME_STOCK_BUTTON_OK,
		NULL);

	label = gtk_label_new (
		_(
		  "Hi.  Thanks for taking the time to download this preview release of\n"
		  "the Evolution groupware suite.\n"
		  "\n"
		  "The Evolution team has worked hard to make Evolution as robust,\n"
		  "extensible, pretty, fast and well-suited to heavy internet users as\n"
		  "possible.  And we're very tired.  But we're not done -- not yet.\n"
		  "\n"
		  "As you explore Evolution, please understand that most of our work has\n"
		  "been focused on the backend engine which drives the entire system and\n"
		  "not on the user interface.  We are just cresting the hill now, though,\n"
		  "and will be pouring most of our love and attention into the UI from\n"
		  "here out.  But at least you know that you're not using demoware.\n"
		  "\n"
		  "So, time for the nerdy disclaimer.  Evolution will: crash, lose your\n"
		  "mail, leave stray processes running, consume 100% CPU, race, lock,\n"
		  "send HTML mail to random mailing lists, and embarass you in front of\n"
		  "your friends and co-workers.  Use at your own risk.\n"
		  "\n"
		  "We hope that you enjoy the results of our hard work, and we eagerly\n"
		  "await your contributions!\n"
		  ));
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	gtk_widget_show (label);

	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (warning_dialog)->vbox), 
			    label, TRUE, TRUE, 0);

	label = gtk_label_new (
		_(
		  "Thanks\n"
		  "The Evolution Team\n"
		  ));
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_RIGHT);
	gtk_misc_set_alignment(GTK_MISC(label), 1, .5);
	gtk_widget_show (label);

	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (warning_dialog)->vbox), 
			    label, TRUE, TRUE, 0);

	gnome_dialog_run (GNOME_DIALOG (warning_dialog));
	
	gtk_object_destroy (GTK_OBJECT (warning_dialog));
} 

static gint
idle_cb (gpointer data)
{
	char *evolution_directory;

	evolution_directory = (char *) data;

	shell = e_shell_new (evolution_directory);
	g_free (evolution_directory);

	if (shell == NULL) {
		e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
			  _("Cannot initialize the Evolution shell."));
		exit (1);
	}

	gtk_signal_connect (GTK_OBJECT (shell), "no_views_left",
			    GTK_SIGNAL_FUNC (no_views_left_cb), NULL);
	gtk_signal_connect (GTK_OBJECT (shell), "destroy",
			    GTK_SIGNAL_FUNC (destroy_cb), NULL);

	e_shell_new_view (shell, STARTUP_URI);

	if (!getenv ("EVOLVE_ME_HARDER"))
		development_warning ();

	return FALSE;
}


int
main (int argc, char **argv)
{
	char *evolution_directory;

	init_corba (&argc, argv);

	gnome_window_icon_set_default_from_file (EVOLUTION_IMAGES "/evolution-inbox.png");


	if (! bonobo_init (CORBA_OBJECT_NIL, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL)) {
		e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
			  _("Cannot initialize the Bonobo component system."));
		exit (1);
	}

	/* FIXME */
	evolution_directory = g_concat_dir_and_file (g_get_home_dir (), "evolution");

	if (! e_setup (evolution_directory)) {
		g_free (evolution_directory);
		exit (1);
	}

	gtk_idle_add (idle_cb, evolution_directory);

	bonobo_main ();

	g_free (evolution_directory);

	return 0;
}
