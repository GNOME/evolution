/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* main.c
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

#include <config.h>
#include <gnome.h>
#include <bonobo.h>
#include <libgnomeui/gnome-window-icon.h>
#include <glade/glade.h>

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
	int ret;
	
	warning_dialog = gnome_dialog_new (
		"Evolution" VERSION,
		GNOME_STOCK_BUTTON_OK,
		NULL);

	label = gtk_label_new (
		_(
		  "Hi.  Thanks for taking the time to download this preview release\n"
		  "of the Evolution groupware suite.\n"
		  "\n"
		  "Over the last month and a half, our focus has been on making\n"
		  "Evolution usable. Many of the Evolution developers are now using\n"
		  "Evolution to read their mail full time. You could too. (Just\n"
		  "be sure to keep a backup.)\n"
		  "\n"
		  "But while we have fixed many bugs affecting its stability and\n"
		  "security, you still get the disclaimer:  Evolution will: crash,\n"
		  "lose your mail when you don't want it to, refuse to delete your\n"
		  "mail when you do want it to, leave stray processes running,\n"
		  "consume 100% CPU, race, lock, send HTML mail to random mailing\n"
		  "lists, and embarass you in front of your friends and co-workers.\n"
		  "Use only as directed.\n"
		  "\n"
		  "We hope that you enjoy the results of our hard work, and we eagerly\n"
		  "await your contributions!\n"
		  ));
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	gtk_widget_show (label);

	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (warning_dialog)->vbox), 
			    label, TRUE, TRUE, 4);

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

	ret = gnome_dialog_run (GNOME_DIALOG (warning_dialog));
	if (ret != -1)
		gtk_object_destroy (GTK_OBJECT (warning_dialog));
}

static void
view_delete_event_cb (GtkWidget *widget,
		      GdkEventAny *event,
		      void *data)
{
	EShell *shell;

	shell = E_SHELL (data);

	gtk_widget_destroy (widget);

	/* FIXME we should keep track of the number of views and exit when all the views are gone.  */
	e_shell_quit (shell);
}

static gint
idle_cb (gpointer data)
{
	GtkWidget *view;
	GConfClient *gconf_client;
	char *evolution_directory;

	evolution_directory = (char *) data;

	gconf_client = gconf_client_get_default ();

	shell = e_shell_new (evolution_directory, gconf_client);
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

	if (! e_shell_restore_from_settings (shell)) {
		view = e_shell_new_view (shell, STARTUP_URI);
		/* FIXME: Do this for all the shell views even when the shell
                   is restored.  */
		gtk_signal_connect (GTK_OBJECT (view), "delete_event",
				    GTK_SIGNAL_FUNC (view_delete_event_cb), shell);
	}

	if (!getenv ("EVOLVE_ME_HARDER"))
		development_warning ();

	gtk_object_unref (GTK_OBJECT (gconf_client));

	return FALSE;
}

int
main (int argc, char **argv)
{
	GConfError *err = NULL;
	char *evolution_directory;

	bindtextdomain (PACKAGE, EVOLUTION_LOCALEDIR);
	textdomain (PACKAGE);

	init_corba (&argc, argv);

	glade_gnome_init ();

	gnome_window_icon_set_default_from_file (EVOLUTION_IMAGES "/evolution-inbox.png");

	if (! gconf_init (argc, argv, &err)) {
		e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
			  _("Cannot initialize the configuration system."));
		exit (1);
	}

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

	return 0;
}
