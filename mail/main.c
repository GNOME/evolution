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
#include <gconf/gconf.h>

#include "e-util/e-gui-utils.h"
#include "e-util/e-cursors.h"

#include "component-factory.h"
#include "mail.h"

#ifdef USING_OAF

#include <liboaf/liboaf.h>

static void
init_corba (int *argc, char *argv [])
{
	od_assert_using_oaf ();
	gnome_init_with_popt_table ("evolution-mail-component", VERSION,
				    *argc, argv, oaf_popt_options, 0, NULL);
	oaf_init (*argc, argv);
}

#else  /* USING_OAF */

#include <libgnorba/gnorba.h>

static void
init_corba (int *argc, char *argv [])
{
	CORBA_Environment ev;

	od_assert_using_goad ();
	CORBA_exception_init (&ev);

 	gnome_CORBA_init_with_popt_table (
		"evolution-mail-component", "1.0",
		argc, argv, NULL, 0, NULL, GNORBA_INIT_SERVER_FUNC, &ev);

	CORBA_exception_free (&ev);
}

#endif /* USING_OAF */

static void
init_bonobo (void)
{
	if (bonobo_init (CORBA_OBJECT_NIL, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL) == FALSE){
		e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
			  _("Mail Component: I could not initialize Bonobo"));
		exit (1);
	}
}

int
main (int argc, char *argv [])
{
	bindtextdomain (PACKAGE, EVOLUTION_LOCALEDIR);
	textdomain (PACKAGE);

#ifdef USE_BROKEN_THREADS
	g_thread_init( NULL );
#endif
	init_corba (&argc, argv);
	init_bonobo ();
	gconf_init (argc, argv, NULL);

	glade_gnome_init ();

	session_init ();
	e_cursors_init ();

	component_factory_init ();

#ifdef USE_BROKEN_THREADS
	GDK_THREADS_ENTER ();
#endif
	bonobo_main ();
#ifdef USE_BROKEN_THREADS
	GDK_THREADS_LEAVE ();
#endif

	return 0;
}
