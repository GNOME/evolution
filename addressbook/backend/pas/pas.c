/*
 * Author:
 *   Nat Friedman (nat@helixcode.com)
 *
 * Copyright 2000, Helix Code, Inc.
 */
#include <config.h>
#include <bonobo.h>
#include <libgnorba/gnorba.h>

#include <pas-book-factory.h>
#include <pas-backend-file.h>

CORBA_Environment ev;
CORBA_ORB orb;

static void
init_bonobo (int argc, char **argv)
{

	gnome_CORBA_init_with_popt_table (
		"Personal Addressbook Server", "0.0",
		&argc, argv, NULL, 0, NULL, GNORBA_INIT_SERVER_FUNC, &ev);

	orb = gnome_CORBA_ORB ();

	if (bonobo_init (orb, NULL, NULL) == FALSE)
		g_error (_("Could not initialize Bonobo"));
}

int
main (int argc, char **argv)
{
	PASBookFactory *factory;

	CORBA_exception_init (&ev);


	init_bonobo (argc, argv);

	/*
	 * Create the factory and register the local-file backend with
	 * it.
	 */
	factory = pas_book_factory_new ();

	pas_book_factory_register_backend (
		factory, "file", pas_backend_file_new);

	pas_book_factory_activate (factory);

	bonobo_main ();

	return 0;
}
