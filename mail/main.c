/*
 * main.c: The core of the mail component
 *
 * Author:
 *   Miguel de Icaza (miguel@helixcode.com)
 *
 * (C) 2000 Helix Code, Inc.
 */

#include <config.h>
#include <gnome.h>
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-object-directory.h>
#include <glade/glade.h>
#include <liboaf/liboaf.h>
#ifdef GTKHTML_HAVE_GCONF
#include <gconf/gconf.h>
#endif
#include "e-util/e-gui-utils.h"
#include "e-util/e-cursors.h"

#include "component-factory.h"
#include "mail.h"

int
main (int argc, char *argv [])
{
	CORBA_ORB orb;

	bindtextdomain (PACKAGE, EVOLUTION_LOCALEDIR);
	textdomain (PACKAGE);

	g_thread_init( NULL );

	od_assert_using_oaf ();
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

	session_init ();
	e_cursors_init ();

	component_factory_init ();

	GDK_THREADS_ENTER ();
	bonobo_main ();
	GDK_THREADS_LEAVE ();

	mail_config_write_on_exit ();

	return 0;
}
