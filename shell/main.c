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
#include <fcntl.h>
#include <gnome.h>
#include <bonobo.h>
#include <libgnomeui/gnome-window-icon.h>
#include <glade/glade.h>
#include <liboaf/liboaf.h>
#include <unicode.h>

#include <gal/widgets/e-gui-utils.h>
#include <gal/widgets/e-cursors.h>
#include "e-setup.h"

#include "e-shell.h"


#define STARTUP_URI "evolution:/local/Inbox"


static EShell *shell = NULL;
static char *evolution_directory = NULL;
static gboolean no_splash = FALSE;
char *debug_log = NULL;


static void
no_views_left_cb (EShell *shell, gpointer data)
{
	e_shell_quit (shell);
	gtk_main_quit ();
}

static void
destroy_cb (GtkObject *object, gpointer data)
{
	gtk_main_quit ();
}


static void
development_warning (void)
{
	GtkWidget *label, *warning_dialog;
	int ret;
	
	warning_dialog = gnome_dialog_new (
		"Evolution" VERSION,
		GNOME_STOCK_BUTTON_OK,
		NULL);
	label = gtk_label_new (
		/* xgettext:no-c-format */
		_("Hi.  Thanks for taking the time to download this PREVIEW RELEASE\n"
		  "of the Evolution groupware suite.\n"
		  "\n"
		  "Evolution is not yet complete. It's getting close, but there are\n"
		  "places where features are either missing or only half working. If\n"
		  "you can't figure out how to do something, it's probably because\n"
		  "there's no way to do it yet! :-)\n"
		  "\n"
		  "We hope Evolution will be usable for you, but we still feel the\n"
		  "need to warn you that it may: crash, lose your mail, leave stray\n"
		  "processes running, consume 100% of your CPU, send non-\n"
		  "compliant commands to your servers, and generally embarass you\n"
		  "in front of your friends and co-workers. Use only as directed.\n"
		  "\n"
		  "We hope that you enjoy the results of our hard work, and we\n"
		  "eagerly await your contributions!\n"
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


/* This is for doing stuff that requires the GTK+ loop to be running already.  */

static void
new_view_on_running_shell (void)
{
	CORBA_Object corba_object;
	GNOME_Evolution_ShellView shell_view;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	corba_object = oaf_activate_from_id (E_SHELL_OAFIID, 0, NULL, &ev);
	if (ev._major != CORBA_NO_EXCEPTION
	    || CORBA_Object_is_nil (corba_object, &ev)) {
		e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
			  _("Cannot initialize the Evolution shell."));
		return;
	}

	shell_view = GNOME_Evolution_Shell_createNewView ((GNOME_Evolution_Shell) corba_object, STARTUP_URI, &ev);
	if (ev._major == CORBA_NO_EXCEPTION) {
		Bonobo_Unknown_unref ((Bonobo_Unknown) shell_view, &ev);
		CORBA_Object_release ((CORBA_Object) shell_view, &ev);
	}

	CORBA_exception_free (&ev);
}

static gint
idle_cb (gpointer data)
{
	EShellView *view;

	shell = e_shell_new (evolution_directory, ! no_splash);
	g_free (evolution_directory);

	if (shell == NULL) {
		/* A new shell cannot be created, so try to get a new view from
                   an already running one.  */
		new_view_on_running_shell ();
		exit (1);
	}

	gtk_signal_connect (GTK_OBJECT (shell), "no_views_left",
			    GTK_SIGNAL_FUNC (no_views_left_cb), NULL);
	gtk_signal_connect (GTK_OBJECT (shell), "destroy",
			    GTK_SIGNAL_FUNC (destroy_cb), NULL);

	if (! e_shell_restore_from_settings (shell))
		view = e_shell_new_view (shell, STARTUP_URI);

	if (!getenv ("EVOLVE_ME_HARDER"))
		development_warning ();

	return FALSE;
}

int
main (int argc, char **argv)
{
	struct poptOption options[] = {
		{ "no-splash", '\0', POPT_ARG_NONE, &no_splash, 0, N_("Disable splash screen"), NULL },
		{ "debug", '\0', POPT_ARG_STRING, &debug_log, 0, N_("Send the debugging output of all components to a file."), NULL },
		{ NULL, '\0', POPT_ARG_INCLUDE_TABLE, &oaf_popt_options, 0, NULL, NULL },
		POPT_AUTOHELP
		{ NULL, '\0', 0, NULL, 0, NULL, NULL }
	};

	bindtextdomain (PACKAGE, EVOLUTION_LOCALEDIR);
	textdomain (PACKAGE);

	gnome_init_with_popt_table ("Evolution", VERSION, argc, argv, options, 0, NULL);

	if (debug_log) {
		int fd;

		fd = open (debug_log, O_WRONLY | O_CREAT | O_TRUNC, 0600);
		if (fd) {
			dup2 (fd, STDOUT_FILENO);
			dup2 (fd, STDERR_FILENO);
			close (fd);
		} else
			g_warning ("Could not set up debugging output file.");
	}

	oaf_init (argc, argv);

	glade_gnome_init ();
	unicode_init ();
	e_cursors_init ();

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

	return 0;
}
