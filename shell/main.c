/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* main.c
 *
 * Copyright (C) 2000  Ximian, Inc.
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
#include <glib.h>
#include <gtk/gtkmain.h>
#include <gtk/gtklabel.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <libgnomeui/gnome-init.h>
#include <libgnomeui/gnome-stock.h>
#include <libgnomeui/gnome-window-icon.h>
#include <bonobo/bonobo-main.h>
#include <glade/glade.h>
#include <liboaf/liboaf.h>

#ifdef GTKHTML_HAVE_GCONF
#include <gconf/gconf.h>
#endif

#include <gal/widgets/e-gui-utils.h>
#include <gal/widgets/e-cursors.h>
#include "e-setup.h"

#include "e-shell.h"


static EShell *shell = NULL;
static char *evolution_directory = NULL;
static gboolean no_splash = FALSE;
extern char *evolution_debug_log;


static void
no_views_left_cb (EShell *shell, gpointer data)
{
	/* FIXME: This is wrong.  We should exit only when the shell is
	   destroyed.  But refcounting is broken at present, so this is a
	   reasonable workaround for now.  */

	e_shell_unregister_all (shell);

	bonobo_object_unref (BONOBO_OBJECT (shell));

	gtk_main_quit ();
}

static void
destroy_cb (GtkObject *object, gpointer data)
{
	gtk_main_quit ();
}


static void
warning_dialog_clicked_callback (GnomeDialog *dialog,
				 int button_number,
				 void *data)
{
	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
development_warning (void)
{
	GtkWidget *label, *warning_dialog;
	
	warning_dialog = gnome_dialog_new ("Ximian Evolution " VERSION, GNOME_STOCK_BUTTON_OK, NULL);

	label = gtk_label_new (
		/* xgettext:no-c-format */
		_("Hi.  Thanks for taking the time to download this preview release\n"
		  "of the Ximian Evolution groupware suite.\n"
		  "\n"
		  "Ximian Evolution is not yet complete. It's getting close, but there are\n"
		  "places where features are either missing or only half working. \n"
		  "\n"
		  "If you find bugs, please report them to us at bugzilla.ximian.com.\n"
                  "This product comes with no warranty and is not intended for\n"
		  "individuals prone to violent fits of anger.\n"
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
		  "The Ximian Evolution Team\n"
		  ));
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_RIGHT);
	gtk_misc_set_alignment(GTK_MISC(label), 1, .5);
	gtk_widget_show (label);

	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (warning_dialog)->vbox), 
			    label, TRUE, TRUE, 0);

	gtk_widget_show (warning_dialog);
	gtk_signal_connect (GTK_OBJECT (warning_dialog), "clicked",
			    GTK_SIGNAL_FUNC (warning_dialog_clicked_callback), NULL);
}


/* This is for doing stuff that requires the GTK+ loop to be running already.  */

static gint
idle_cb (void *data)
{
	GSList *uri_list;
	GNOME_Evolution_Shell corba_shell;
	CORBA_Environment ev;
	EShellConstructResult result;
	gboolean restored;

	CORBA_exception_init (&ev);

	uri_list = (GSList *) data;

	shell = e_shell_new (evolution_directory, ! no_splash, &result);
	g_free (evolution_directory);

	switch (result) {
	case E_SHELL_CONSTRUCT_RESULT_OK:
		gtk_signal_connect (GTK_OBJECT (shell), "no_views_left",
				    GTK_SIGNAL_FUNC (no_views_left_cb), NULL);
		gtk_signal_connect (GTK_OBJECT (shell), "destroy",
				    GTK_SIGNAL_FUNC (destroy_cb), NULL);

		if (uri_list == NULL)
			restored = e_shell_restore_from_settings (shell);
		else
			restored = FALSE;

		if (!getenv ("EVOLVE_ME_HARDER"))
			development_warning ();

		corba_shell = bonobo_object_corba_objref (BONOBO_OBJECT (shell));
		corba_shell = CORBA_Object_duplicate (corba_shell, &ev);
		break;

	case E_SHELL_CONSTRUCT_RESULT_CANNOTREGISTER:
		corba_shell = oaf_activate_from_id (E_SHELL_OAFIID, 0, NULL, &ev);
		if (ev._major != CORBA_NO_EXCEPTION || corba_shell == CORBA_OBJECT_NIL) {
			e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
				  _("Cannot access the Ximian Evolution shell."));
			CORBA_exception_free (&ev);
			gtk_main_quit ();
			return FALSE;
		}

		restored = FALSE;
		break;

	default:
		e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
			  _("Cannot initialize the Ximian Evolution shell: %s"),
			  e_shell_construct_result_to_string (result));
		CORBA_exception_free (&ev);
		gtk_main_quit ();
		return FALSE;

	}

	if (! restored && uri_list == NULL) {
		const char *uri = E_SHELL_VIEW_DEFAULT_URI;

		GNOME_Evolution_Shell_handleURI (corba_shell, uri, &ev);
		if (ev._major != CORBA_NO_EXCEPTION)
			g_warning ("CORBA exception %s when requesting URI -- %s", ev._repo_id, uri);
	} else {
		GSList *p;

		for (p = uri_list; p != NULL; p = p->next) {
			char *uri;

			uri = (char *) p->data;

			GNOME_Evolution_Shell_handleURI (corba_shell, uri, &ev);
			if (ev._major != CORBA_NO_EXCEPTION)
				g_warning ("CORBA exception %s when requesting URI -- %s", ev._repo_id, uri);
		}

		g_slist_free (uri_list);
	}

	CORBA_Object_release (corba_shell, &ev);

	CORBA_exception_free (&ev);
	
	if (shell == NULL)
		gtk_main_quit ();

	return FALSE;
}

int
main (int argc, char **argv)
{
	struct poptOption options[] = {
		{ "no-splash", '\0', POPT_ARG_NONE, &no_splash, 0, N_("Disable splash screen"), NULL },
		{ "debug", '\0', POPT_ARG_STRING, &evolution_debug_log, 0, N_("Send the debugging output of all components to a file."), NULL },
		{ NULL, '\0', POPT_ARG_INCLUDE_TABLE, &oaf_popt_options, 0, NULL, NULL },
		POPT_AUTOHELP
		{ NULL, '\0', 0, NULL, 0, NULL, NULL }
	};
	GSList *uri_list;
	const char **args;
	poptContext popt_context;

	bindtextdomain (PACKAGE, EVOLUTION_LOCALEDIR);
	textdomain (PACKAGE);

	/* Make ElectricFence work.  */
	free (malloc (10));

	gnome_init_with_popt_table ("Evolution", VERSION, argc, argv, options, 0, &popt_context);

	if (evolution_debug_log) {
		int fd;

		fd = open (evolution_debug_log, O_WRONLY | O_CREAT | O_TRUNC, 0600);
		if (fd) {
			dup2 (fd, STDOUT_FILENO);
			dup2 (fd, STDERR_FILENO);
			close (fd);
		} else
			g_warning ("Could not set up debugging output file.");
	}

	oaf_init (argc, argv);

#ifdef GTKHTML_HAVE_GCONF
	gconf_init (argc, argv, NULL);
#endif

	glade_gnome_init ();
	e_cursors_init ();

	gnome_window_icon_set_default_from_file (EVOLUTION_IMAGES "/evolution-inbox.png");

	if (! bonobo_init (CORBA_OBJECT_NIL, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL)) {
		e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
			  _("Cannot initialize the Bonobo component system."));
		exit (1);
	}

	/* FIXME */
	evolution_directory = g_concat_dir_and_file (g_get_home_dir (), "evolution");

	if (! e_setup (evolution_directory))
		exit (1);

	uri_list = NULL;

	args = poptGetArgs (popt_context);
	if (args != NULL) {
		const char **p;

		for (p = args; *p != NULL; p++)
			uri_list = g_slist_prepend (uri_list, (char *) *p);
	}

	gtk_idle_add (idle_cb, uri_list);

	bonobo_main ();

	return 0;
}
