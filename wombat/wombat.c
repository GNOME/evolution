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



static CORBA_ORB orb;

/* The and addressbook calendar factories */

static CalFactory *cal_factory;
static PASBookFactory *pas_book_factory;

/* Timeout interval in milliseconds for termination */
#define EXIT_TIMEOUT 5000

/* Timeout ID for termination handler */
static guint termination_handler_id;



/* Termination */

/* Termination handler.  Checks if both factories have zero running backends,
 * and if so terminates the program.
 */
static gboolean
termination_handler (gpointer data)
{
	/* FIXME: add the test for the PAS as well */

	if (cal_factory_get_n_backends (cal_factory) == 0)
		gtk_main_quit ();

	termination_handler_id = 0;
	return FALSE;
}

/* Queues a timeout for handling termination of Wombat */
static void
queue_termination (void)
{
	if (termination_handler_id)
		return;

	termination_handler_id = g_timeout_add (EXIT_TIMEOUT, termination_handler, NULL);
}



static void
setup_pas (int argc, char **argv)
{
	pas_book_factory = pas_book_factory_new ();

	pas_book_factory_register_backend (
		pas_book_factory, "file", pas_backend_file_new);

#ifdef HAVE_LDAP
	pas_book_factory_register_backend (
		pas_book_factory, "ldap", pas_backend_ldap_new);
#endif

	pas_book_factory_activate (pas_book_factory);
}



/* Personal calendar server */

/* Callback used when the calendar factory has no more running backends */
static void
last_calendar_gone_cb (CalFactory *factory, gpointer data)
{
	queue_termination ();
}

/* Creates the calendar factory object and registers it with GOAD */
static void
setup_pcs (int argc, char **argv)
{
	CORBA_Object object;
	CORBA_Environment ev;
	int result;

	cal_factory = cal_factory_new ();

	if (!cal_factory) {
		g_message ("setup_pcs(): Could not create the calendar factory");
		return;
	}

	object = bonobo_object_corba_objref (BONOBO_OBJECT (cal_factory));

	CORBA_exception_init (&ev);
	result = goad_server_register (CORBA_OBJECT_NIL,
				       object,
				       "evolution:calendar-factory",
				       "object",
				       &ev);

	/* FIXME: should Wombat die if it gets errors here? */

	if (ev._major != CORBA_NO_EXCEPTION || result == -1) {
		g_message ("setup_pcs(): could not register the calendar factory");
		bonobo_object_unref (BONOBO_OBJECT (cal_factory));
		cal_factory = NULL;
		CORBA_exception_free (&ev);
		return;
	} else if (result == -2) {
		g_message ("setup_pcs(): a calendar factory is already registered");
		bonobo_object_unref (BONOBO_OBJECT (cal_factory));
		cal_factory = NULL;
		CORBA_exception_free (&ev);
		return;
	}

	gtk_signal_connect (GTK_OBJECT (cal_factory), "last_calendar_gone",
			    GTK_SIGNAL_FUNC (last_calendar_gone_cb),
			    NULL);

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
		exit (EXIT_FAILURE);
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
		exit (EXIT_FAILURE);
	}

	CORBA_exception_free (&ev);

	orb = gnome_CORBA_ORB ();

	if (!bonobo_init (orb, NULL, NULL)) {
		g_message ("init_bonobo(): could not initialize Bonobo");
		exit (EXIT_FAILURE);
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

	bonobo_object_unref (BONOBO_OBJECT (cal_factory));
	cal_factory = NULL;

	return 0;
}
