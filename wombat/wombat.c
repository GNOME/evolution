/* Wombat personal information server - main file
 *
 * Author: Nat Friedman <nat@ximian.com>
 *
 * Copyright 2000, Ximian, Inc.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* define this if you need/want to be able to send USR2 to wombat and
   get a list of the active backends */
/*#define DEBUG_BACKENDS*/

#include <stdlib.h>
#ifdef DEBUG_BACKENDS
#include <sys/signal.h>
#endif

#include <glib.h>
#include <libgnome/gnome-init.h>
#include <bonobo-activation/bonobo-activation.h>
#include <libgnomevfs/gnome-vfs-init.h>
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-i18n.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-generic-factory.h>

#include "pas/pas-book-factory.h"
#include "pas/pas-backend-file.h"
#include "pas/pas-backend-vcf.h"
#ifdef HAVE_LDAP
#include "pas/pas-backend-ldap.h"
#endif

#include "calendar/pcs/cal-factory.h"
#include "calendar/pcs/cal-backend-file-events.h"
#include "calendar/pcs/cal-backend-file-todos.h"

#include "wombat-interface-check.h"

#define CAL_FACTORY_OAF_ID "OAFIID:GNOME_Evolution_Wombat_CalendarFactory"
#define PAS_BOOK_FACTORY_OAF_ID "OAFIID:GNOME_Evolution_Wombat_ServerFactory"

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
	if (cal_factory_get_n_backends (cal_factory) == 0 &&
	    pas_book_factory_get_n_backends (pas_book_factory) == 0) {
		fprintf (stderr, "termination_handler(): Terminating the Wombat.  Have a nice day.\n");
		bonobo_main_quit ();
	}

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
last_book_gone_cb (PASBookFactory *factory, gpointer data)
{
	queue_termination ();
}

static gboolean
setup_pas (void)
{
	pas_book_factory = pas_book_factory_new ();

	if (!pas_book_factory)
		return FALSE;

	pas_book_factory_register_backend (
		pas_book_factory, "file", pas_backend_file_new);

	pas_book_factory_register_backend (
		pas_book_factory, "vcf", pas_backend_vcf_new);

#ifdef HAVE_LDAP
	pas_book_factory_register_backend (
		pas_book_factory, "ldap", pas_backend_ldap_new);
#endif

	g_signal_connect (pas_book_factory,
			  "last_book_gone",
			  G_CALLBACK (last_book_gone_cb),
			  NULL);

	if (!pas_book_factory_activate (pas_book_factory, PAS_BOOK_FACTORY_OAF_ID)) {
		bonobo_object_unref (BONOBO_OBJECT (pas_book_factory));
		pas_book_factory = NULL;
		return FALSE;
	}

	return TRUE;
}


/* Personal calendar server */

/* Callback used when the calendar factory has no more running backends */
static void
last_calendar_gone_cb (CalFactory *factory, gpointer data)
{
	queue_termination ();
}

/* Creates the calendar factory object and registers it */
static gboolean
setup_pcs (void)
{
	cal_factory = cal_factory_new ();

	if (!cal_factory) {
		g_message ("setup_pcs(): Could not create the calendar factory");
		return FALSE;
	}

	cal_factory_register_method (cal_factory, "file", ICAL_VEVENT_COMPONENT, CAL_BACKEND_FILE_EVENTS_TYPE);
	cal_factory_register_method (cal_factory, "file", ICAL_VTODO_COMPONENT, CAL_BACKEND_FILE_TODOS_TYPE);

	if (!cal_factory_register_storage (cal_factory, CAL_FACTORY_OAF_ID)) {
		bonobo_object_unref (BONOBO_OBJECT (cal_factory));
		cal_factory = NULL;
		return FALSE;
	}

	g_signal_connect (G_OBJECT (cal_factory),
			  "last_calendar_gone",
			  G_CALLBACK (last_calendar_gone_cb),
			  NULL);

	return TRUE;
}


/* Interface check iface.  */

static gboolean
setup_interface_check (void)
{
	WombatInterfaceCheck *interface_check_iface = wombat_interface_check_new ();
	int result;

	result = bonobo_activation_active_server_register ("OAFIID:GNOME_Evolution_Wombat_InterfaceCheck",
							   BONOBO_OBJREF (interface_check_iface));

	return result == Bonobo_ACTIVATION_REG_SUCCESS;
}



#ifdef DEBUG_BACKENDS
static void
dump_backends (int signal)
{
	pas_book_factory_dump_active_backends (pas_book_factory);
	cal_factory_dump_active_backends (cal_factory);
}
#endif

int
main (int argc, char **argv)
{
	gboolean did_pas=FALSE, did_pcs=FALSE;

	bindtextdomain (GETTEXT_PACKAGE, EVOLUTION_LOCALEDIR);
	textdomain (GETTEXT_PACKAGE);

	g_message ("Starting wombat");

#ifdef DEBUG_BACKENDS
	signal (SIGUSR2, dump_backends);
#endif

       	gnome_program_init ("Wombat", VERSION,
			    LIBGNOME_MODULE,
			    argc, argv,
			    GNOME_PROGRAM_STANDARD_PROPERTIES, NULL);

	bonobo_init_full (&argc, argv,
			  bonobo_activation_orb_get(),
			  CORBA_OBJECT_NIL,
			  CORBA_OBJECT_NIL);

	if (!( (did_pas = setup_pas ())
	       && (did_pcs = setup_pcs ())
		    )) {

		const gchar *failed = NULL;

		if (!did_pas)
			failed = "PAS";
		else if (!did_pcs)
			failed = "PCS";

		g_message ("main(): could not initialize Wombat service \"%s\"; terminating", failed);

		if (pas_book_factory) {
			bonobo_object_unref (BONOBO_OBJECT (pas_book_factory));
			pas_book_factory = NULL;
		}

		if (cal_factory) {
			bonobo_object_unref (BONOBO_OBJECT (cal_factory));
			cal_factory = NULL;
		}
		exit (EXIT_FAILURE);
	}

	if (! setup_interface_check ()) {
		g_message ("Cannot register Wombat::InterfaceCheck object");
		exit (EXIT_FAILURE);
	}

	g_print ("Wombat up and running\n");

	bonobo_main ();

	bonobo_object_unref (BONOBO_OBJECT (cal_factory));
	cal_factory = NULL;

	bonobo_object_unref (BONOBO_OBJECT (pas_book_factory));
	pas_book_factory = NULL;

	gnome_vfs_shutdown ();

	return 0;
}
