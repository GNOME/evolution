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
#include <gtk.h> /* XXX needed only until the calendar switches to straight GObject's for their backend */
#include <libgnome/gnome-init.h>
#include <bonobo-activation/bonobo-activation.h>
#include <libgnomevfs/gnome-vfs-init.h>
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-i18n.h>

#include "pas/pas-book-factory.h"
#include "pas/pas-backend-file.h"

#ifdef PENDING_PORT_WORK
#include "calendar/pcs/cal-factory.h"
#include "calendar/pcs/cal-backend-file.h"
#endif

#ifdef HAVE_LDAP
#include "pas/pas-backend-ldap.h"
#endif

#define CAL_FACTORY_OAF_ID "OAFIID:GNOME_Evolution_Wombat_CalendarFactory"
#define PAS_BOOK_FACTORY_OAF_ID "OAFIID:GNOME_Evolution_Wombat_ServerFactory"

/* The and addressbook calendar factories */

#ifdef PENDING_PORT_WORK
static CalFactory *cal_factory;
#endif

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
	if (
#ifdef PENDING_PORT_WORK
	    cal_factory_get_n_backends (cal_factory) == 0 &&
#endif
	    pas_book_factory_get_n_backends (pas_book_factory) == 0) {
		fprintf (stderr, "termination_handler(): Terminating the Wombat.  Have a nice day.\n");
		gtk_main_quit ();
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
setup_pas (int argc, char **argv)
{
	pas_book_factory = pas_book_factory_new ();

	if (!pas_book_factory)
		return FALSE;

	pas_book_factory_register_backend (
		pas_book_factory, "file", pas_backend_file_new);

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



#ifdef PENDING_PORT_WORK
/* Personal calendar server */

/* Callback used when the calendar factory has no more running backends */
static void
last_calendar_gone_cb (CalFactory *factory, gpointer data)
{
	queue_termination ();
}

/* Creates the calendar factory object and registers it */
static gboolean
setup_pcs (int argc, char **argv)
{
	cal_factory = cal_factory_new ();

	if (!cal_factory) {
		g_message ("setup_pcs(): Could not create the calendar factory");
		return FALSE;
	}

	cal_factory_register_method (cal_factory, "file", CAL_BACKEND_FILE_TYPE);

	if (!cal_factory_oaf_register (cal_factory, CAL_FACTORY_OAF_ID)) {
		bonobo_object_unref (BONOBO_OBJECT (cal_factory));
		cal_factory = NULL;
		return FALSE;
	}

	gtk_signal_connect (GTK_OBJECT (cal_factory),
			    "last_calendar_gone",
			    GTK_SIGNAL_FUNC (last_calendar_gone_cb),
			    NULL);

	return TRUE;
}
#endif



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

	bindtextdomain (PACKAGE, EVOLUTION_LOCALEDIR);
	textdomain (PACKAGE);

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

	/*g_log_set_always_fatal (G_LOG_LEVEL_ERROR |
				G_LOG_LEVEL_CRITICAL |
				G_LOG_LEVEL_WARNING);*/

	if (!( (did_pas = setup_pas (argc, argv))
#ifdef PENDING_PORT_WORK
	       && (did_pcs = setup_pcs (argc, argv))
#endif
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

#ifdef PENDING_PORT_WORK
		if (cal_factory) {
			bonobo_object_unref (BONOBO_OBJECT (cal_factory));
			cal_factory = NULL;
		}
#endif
		exit (EXIT_FAILURE);
	}

	g_print ("Wombat up and running\n");

	bonobo_main ();

#if PENDING_PORT_WORK
	bonobo_object_unref (BONOBO_OBJECT (cal_factory));
	cal_factory = NULL;
#endif

	bonobo_object_unref (BONOBO_OBJECT (pas_book_factory));
	pas_book_factory = NULL;

	gnome_vfs_shutdown ();

	return 0;
}
