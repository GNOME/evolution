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

#include "e-util/e-gui-utils.h"
#include "e-setup.h"

#include "e-shell.h"


#define STARTUP_URI "evolution:/local/Inbox"


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


static gint
new_view_idle_cb (gpointer data)
{
	EShell *shell;

	shell = E_SHELL (data);
	e_shell_new_view (shell, STARTUP_URI);

	return FALSE;
}


int
main (int argc, char **argv)
{
	EShell *shell;
	char *evolution_directory;

	init_corba (&argc, argv);

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

	shell = e_shell_new (evolution_directory);
	if (shell == NULL) {
		e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
			  _("Cannot initialize the Evolution shell."));
		exit (1);
	}

	gtk_signal_connect (GTK_OBJECT (shell), "no_views_left",
			    GTK_SIGNAL_FUNC (no_views_left_cb), NULL);
	gtk_signal_connect (GTK_OBJECT (shell), "destroy",
			    GTK_SIGNAL_FUNC (destroy_cb), NULL);

	gtk_idle_add (new_view_idle_cb, shell);

	bonobo_main ();

	g_free (evolution_directory);

	return 0;
}
