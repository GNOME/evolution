/* Wombat personal information server - main file
 *
 * Author: Nat Friedman <nat@helixcode.com>
 *
 * Copyright 2000, Helix Code, Inc.
 */

#include <config.h>
#include <gnome.h>
#include <bonobo.h>
#include <liboaf/liboaf.h>
#include <pas/pas-book-factory.h>
#include <pas/pas-backend-file.h>
#include <libgnomevfs/gnome-vfs-init.h>

#ifdef HAVE_LDAP
#include <pas/pas-backend-ldap.h>
#endif

#include "calendar/pcs/cal-factory.h"
#include "calendar/pcs/cal-backend-file.h"

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
	if (cal_factory_get_n_backends (cal_factory) == 0
	    && pas_book_factory_get_n_backends (pas_book_factory) == 0) {
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

	gtk_signal_connect (GTK_OBJECT (pas_book_factory),
			    "last_book_gone",
			    GTK_SIGNAL_FUNC (last_book_gone_cb),
			    NULL);

	pas_book_factory_activate (pas_book_factory);
}



/* Personal calendar server */

/* Callback used when the calendar factory has no more running backends */
static void
last_calendar_gone_cb (CalFactory *factory, gpointer data)
{
	fprintf (stderr, "last_calendar_gone_cb() called!  Queueing termination...\n");
	queue_termination ();
}

static gboolean
register_pcs (CORBA_Object obj)
{
	OAF_RegistrationResult result;

	result = oaf_active_server_register
		("OAFIID:evolution:calendar-factory:1c915858-ece3-4a6f-9d81-ea0f108a9554",
		 obj);

	switch (result) {
	case OAF_REG_SUCCESS:
		return TRUE;	/* Wooho! */
	case OAF_REG_NOT_LISTED:
		g_message ("Cannot register the PCS because not listed");
		return FALSE;
	case OAF_REG_ALREADY_ACTIVE:
		g_message ("Cannot register the PCS because already active");
		return FALSE;
	case OAF_REG_ERROR:
	default:
		g_message ("Cannot register the PCS because we suck");
		return FALSE;
	}
}

/* Creates the calendar factory object and registers it */
static void
setup_pcs (int argc, char **argv)
{
	CORBA_Object object;

	cal_factory = cal_factory_new ();

	if (!cal_factory) {
		g_message ("setup_pcs(): Could not create the calendar factory");
		return;
	}

	cal_factory_register_method (cal_factory, "file", CAL_BACKEND_FILE_TYPE);

	object = bonobo_object_corba_objref (BONOBO_OBJECT (cal_factory));

	if (! register_pcs (object)) {
		bonobo_object_unref (BONOBO_OBJECT (cal_factory));
		cal_factory = NULL;
		return;
	}

	gtk_signal_connect (GTK_OBJECT (cal_factory),
			    "last_calendar_gone",
			    GTK_SIGNAL_FUNC (last_calendar_gone_cb),
			    NULL);
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
init_corba (int *argc, char **argv)
{
	gnome_init_with_popt_table ("Personal Addressbook Server", "0.0",
				    *argc, argv, oaf_popt_options, 0, NULL);
	oaf_init (*argc, argv);
}

static void
init_bonobo (int *argc, char **argv)
{
	init_corba (argc, argv);

	if (!bonobo_init (CORBA_OBJECT_NIL, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL)) {
		g_message ("init_bonobo(): could not initialize Bonobo");
		exit (EXIT_FAILURE);
	}
}

int
main (int argc, char **argv)
{
	{
		char *c = malloc (10);
		if (c)
			free (c);
	}
	init_bonobo  (&argc, argv);
	setup_vfs    (argc, argv);

	setup_pas    (argc, argv);
	setup_pcs    (argc, argv);
	setup_config (argc, argv);

	/*g_log_set_always_fatal (G_LOG_LEVEL_ERROR |
				G_LOG_LEVEL_CRITICAL |
				G_LOG_LEVEL_WARNING);*/

	bonobo_main  ();

	bonobo_object_unref (BONOBO_OBJECT (cal_factory));
	cal_factory = NULL;

	bonobo_object_unref (BONOBO_OBJECT (pas_book_factory));
	pas_book_factory = NULL;

	return 0;
}
