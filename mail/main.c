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

#if 0
static int blowup(int status)
{
	printf("memory blew up, status %d\n", status);
	/*abort();*/
	return status;
}
#endif

int
main (int argc, char *argv [])
{
	CORBA_ORB orb;

#if 0
	/* used to make elfence work */
#if 0
	free (malloc (10));
#else
	/*mtrace();*/
	mcheck(blowup);
#endif
#endif
#ifdef ENABLE_NLS
	bindtextdomain (PACKAGE, EVOLUTION_LOCALEDIR);
	textdomain (PACKAGE);
#endif

	g_thread_init( NULL );

	gnome_init_with_popt_table ("evolution-mail-component", VERSION,
				    argc, argv, oaf_popt_options, 0, NULL);
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

	e_unicode_init ();

	e_cursors_init ();

	component_factory_init ();
	evolution_composer_factory_init (composer_send_cb,
					 composer_postpone_cb);

	signal (SIGSEGV, SIG_DFL);
	signal (SIGBUS, SIG_DFL);

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
