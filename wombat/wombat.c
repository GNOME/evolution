/*
 * Author:
 *   Nat Friedman (nat@helixcode.com)
 *
 * Copyright 2000, Helix Code, Inc.
 */

#include <config.h>
#include <bonobo.h>
#include <pas-book-factory.h>
#include <pas-backend-file.h>
#include <libgnomevfs/gnome-vfs-init.h>
#include <libgnorba/gnorba.h>
#include <cal-factory.h>
#include <calobj.h>

CORBA_Environment ev;
CORBA_ORB orb;

static void
setup_pas (int argc, char **argv)
{
	static PASBookFactory *factory;

	factory = pas_book_factory_new ();

	pas_book_factory_register_backend (
		factory, "file", pas_backend_file_new);

	pas_book_factory_activate (factory);
}


static void
setup_pcs (int argc, char **argv)
{
	int result;
	CORBA_Object object;
	CalFactory *factory = cal_factory_new ();
	
	if (!factory) {
		g_message ("%s: %d: couldn't create a Calendar factory\n",
			   __FILE__, __LINE__);
	}

	object = bonobo_object_corba_objref (BONOBO_OBJECT (factory));

	CORBA_exception_init (&ev);
	result = goad_server_register (CORBA_OBJECT_NIL,
				       object,
				       "evolution:calendar-factory",
				       "server",
				       &ev);

	if (ev._major != CORBA_NO_EXCEPTION || result == -1) {

		g_message ("create_cal_factory(): "
			   "could not register the calendar factory");
		bonobo_object_unref (BONOBO_OBJECT (factory));
		CORBA_exception_free (&ev);

	} else if (result == -2) {

		g_message ("create_cal_factory(): "
			   "a calendar factory is already registered");
		bonobo_object_unref (BONOBO_OBJECT (factory));
		CORBA_exception_free (&ev);

	}

	CORBA_exception_free (&ev);	
}

static void
setup_config (int argc, char **argv)
{
}

static void
setup_vfs (int argc, char **argv)
{
	if (!gnome_vfs_init ()) {
		g_message ("setup_vfs(): could not initialize GNOME-VFS");
		exit (1);
	}
}


static void
init_bonobo (int argc, char **argv)
{
	CORBA_exception_init (&ev);

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
	init_bonobo  (argc, argv);
	setup_vfs    (argc, argv);

	setup_pas    (argc, argv);
	setup_pcs    (argc, argv);
	setup_config (argc, argv);

	bonobo_main  ();

	return 0;
}
