/* Wombat personal information server - main file
 *
 * Author: Nat Friedman <nat@helixcode.com>
 *
 * Copyright 2000, Helix Code, Inc.
 */

#include <config.h>
#include <bonobo.h>
#include <pas/pas-book-factory.h>
#include <pas/pas-backend-file.h>
#ifdef HAVE_LDAP
#include <pas/pas-backend-ldap.h>
#endif
#include <libgnomevfs/gnome-vfs-init.h>
#include <libgnorba/gnorba.h>
#include "calendar/pcs/cal-factory.h"

CORBA_ORB orb;

static void
setup_pas (int argc, char **argv)
{
	static PASBookFactory *factory;

	factory = pas_book_factory_new ();

	pas_book_factory_register_backend (
		factory, "file", pas_backend_file_new);

#ifdef HAVE_LDAP
	pas_book_factory_register_backend (
		factory, "ldap", pas_backend_ldap_new);
#endif

	pas_book_factory_activate (factory);
}

/* Creates the calendar factory object and registers it with GOAD */
static void
setup_pcs (int argc, char **argv)
{
	CalFactory *factory;
	CORBA_Object object;
	CORBA_Environment ev;
	int result;

	factory = cal_factory_new ();

	if (!factory) {
		g_message ("setup_pcs(): Could not create the calendar factory");
		return;
	}

	object = bonobo_object_corba_objref (BONOBO_OBJECT (factory));

	CORBA_exception_init (&ev);
	result = goad_server_register (CORBA_OBJECT_NIL,
				       object,
				       "evolution:calendar-factory",
				       "object",
				       &ev);

	/* FIXME: should Wombat die if it gets errors here? */

	if (ev._major != CORBA_NO_EXCEPTION || result == -1) {
		g_message ("setup_pcs(): could not register the calendar factory");
		bonobo_object_unref (BONOBO_OBJECT (factory));
		CORBA_exception_free (&ev);
		return;
	} else if (result == -2) {
		g_message ("setup_pcs(): a calendar factory is already registered");
		bonobo_object_unref (BONOBO_OBJECT (factory));
		CORBA_exception_free (&ev);
		return;
	}

	/* FIXME: we never connect to the destroy signal of the factory.  We
	 * need to add a signal to it to indicate that the last client died.
	 * The PAS factory needs to have the same thing.  When Wombat sees that
	 * both factories have lost all their clients, it should destroy the
	 * factories and terminate.  */

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
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	gnome_CORBA_init_with_popt_table (
		"Personal Addressbook Server", "0.0",
		&argc, argv, NULL, 0, NULL, GNORBA_INIT_SERVER_FUNC, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_message ("init_bonobo(): could not initialize GOAD");
		CORBA_exception_free (&ev);
		exit (1);
	}

	CORBA_exception_free (&ev);

	orb = gnome_CORBA_ORB ();

	if (!bonobo_init (orb, NULL, NULL)) {
		g_message ("init_bonobo(): could not initialize Bonobo");
		exit (1);
	}
}

int
main (int argc, char **argv)
{
	init_bonobo  (argc, argv);
	setup_vfs    (argc, argv);

	setup_pas    (argc, argv);
	setup_pcs    (argc, argv);
	setup_config (argc, argv);

	/*g_log_set_always_fatal ((GLogLevelFlags) 0xFFFF);*/

	bonobo_main  ();

	return 0;
}
