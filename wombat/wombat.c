/*
 * Author:
 *   Nat Friedman (nat@helixcode.com)
 *
 * Copyright 2000, Helix Code, Inc.
 */

#include <config.h>
#include <bonobo.h>
#include <backend/pas-book-factory.h>
#include <backend/pas-backend-file.h>

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

#include "cal-factory.h"
#include "calobj.h"
static void
setup_pcs (int argc, char **argv)
{
	CalFactory *factory;

	factory = cal_factory_new ();
	if (!factory) {
		g_message ("%s: %d: couldn't create a Calendar factory\n",
			   __FILE__, __LINE__);
	}
	
		
}

static void
setup_config (int argc, char **argv)
{
}

#include <libgnomevfs/gnome-vfs-init.h>
static void
setup_vfs (int argc, char **argv)
{
	if (!gnome_vfs_init ()) {
		g_message ("setup_vfs(): could not initialize GNOME-VFS");
		exit (1);
	}
}

#include <libgnorba/gnorba.h>
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
