/*
 * main.c: The core of the mail component
 *
 * Author:
 *   Miguel de Icaza (miguel@helixcode.com)
 *
 * (C) 2000 Helix Code, Inc.
 */

#include <config.h>

#include <signal.h>

#include <gnome.h>
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-object-directory.h>
#include <glade/glade.h>
#include <liboaf/liboaf.h>
#include <libgnomevfs/gnome-vfs.h>

#ifdef GTKHTML_HAVE_GCONF
#include <gconf/gconf.h>
#endif

#include <gal/widgets/e-gui-utils.h>
#include <gal/widgets/e-cursors.h>
#include <gal/widgets/e-unicode.h>

#include "component-factory.h"
#include "composer/evolution-composer.h"
#include "mail.h"
#include "mail-mt.h"

/*#define DO_MCHECK*/

#ifdef DO_MCHECK
static int blowup(int status)
{
	switch(status) {
	case 1:
		printf("Double free failure\n");
		break;
	case 2:
		printf("Memory clobbered before block\n");
		break;
	case 3:
		printf("Memory clobbered after block\n");
		break;
	}
	abort();
	return status;
}
#endif

/* The GNOME SEGV handler will lose if it's not run from the main Gtk
 * thread. So if we crash in another thread, redirect the signal.
 */
static void (*gnome_segv_handler) (int);

static void
segv_redirect (int sig)
{
	if (pthread_self () == mail_gui_thread)
		gnome_segv_handler (sig);
	else {
		pthread_kill (mail_gui_thread, sig);
		pthread_exit (NULL);
	}
}

int
main (int argc, char *argv [])
{
	CORBA_ORB orb;
	struct sigaction sa, osa;

#ifdef DO_MCHECK
	/* used to make elfence work */
#if 0
	free (malloc (10));
#else
	/*mtrace();*/
	mcheck(blowup);
#endif
#endif
	bindtextdomain (PACKAGE, EVOLUTION_LOCALEDIR);
	textdomain (PACKAGE);

	g_thread_init( NULL );

	gnome_init_with_popt_table ("evolution-mail-component", VERSION,
				    argc, argv, oaf_popt_options, 0, NULL);

	sa.sa_flags = 0;
	sigemptyset (&sa.sa_mask);
	sa.sa_handler = segv_redirect;
	sigaction (SIGSEGV, &sa, &osa);
	sigaction (SIGBUS, &sa, NULL);
	sigaction (SIGFPE, &sa, NULL);
	gnome_segv_handler = osa.sa_handler;

	orb = oaf_init (argc, argv);

	if (bonobo_init (orb, CORBA_OBJECT_NIL,
			 CORBA_OBJECT_NIL) == FALSE) {
		g_error ("Mail component could not initialize Bonobo.\n"
			 "If there was a warning message about the "
			 "RootPOA, it probably means\nyou compiled "
			 "Bonobo against GOAD instead of OAF.");
	}

#ifdef GTKHTML_HAVE_GCONF
	gconf_init (argc, argv, NULL);
#endif

	glade_gnome_init ();

	gnome_vfs_init ();

	e_cursors_init ();

	mail_msg_init();
	
	component_factory_init ();
	evolution_composer_factory_init (composer_send_cb,
					 composer_postpone_cb);

	if (gdk_threads_mutex) {
		g_mutex_free (gdk_threads_mutex);
		gdk_threads_mutex = NULL;
	}

	GDK_THREADS_ENTER ();
	bonobo_main ();
	GDK_THREADS_LEAVE ();
	
	mail_config_write_on_exit ();

	return 0;
}
