/*
 * mail-component.c: The core of the mail component
 *
 * Author:
 *   Miguel de Icaza (miguel@helixcode.com)
 *
 * (C) 2000 Helix Code, Inc.
 */
#include <config.h>
#include <gnome.h>
#include <libgnorba/gnorba.h>
#include <bonobo/bonobo-main.h>
#include "e-util/e-gui-utils.h"
#include "main.h"

CORBA_ORB orb;

static void
init_bonobo (int argc, char **argv)
{
	CORBA_Environment ev;
	
	CORBA_exception_init (&ev);

	gnome_CORBA_init_with_popt_table (
		"evolution-mail-component", "1.0",
		&argc, argv, NULL, 0, NULL, GNORBA_INIT_SERVER_FUNC, &ev);

	orb = gnome_CORBA_ORB ();

	if (bonobo_init (orb, NULL, NULL) == FALSE){
		e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
			  _("Mail Component: I could not initialize Bonobo"));
		exit (1);
	}

	CORBA_exception_free (&ev);
}

int
main (int argc, char *argv [])
{
	bindtextdomain (PACKAGE, EVOLUTION_LOCALEDIR);
	textdomain (PACKAGE);

	init_bonobo (argc, argv);

	session_init ();
	e_cursors_init ();

	folder_browser_factory_init ();

	bonobo_main ();

	return 0;
}




