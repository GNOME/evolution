/*
 *
 * Author:
 *   Anders Carlsson (andersca@gnu.org)
 *
 * (C) 2000 Helix Code, Inc.
 */

#include <config.h>
#include <gnome.h>
#include <bonobo.h>
#include <liboaf/liboaf.h>

#include "e-util/e-gui-utils.h"
#include "component-factory.h"

static void
init_corba (gint argc, gchar **argv)
{
	gnome_init_with_popt_table ("evolution-notes-component", VERSION, argc, argv,
				    oaf_popt_options, 0, NULL);
	oaf_init (argc, argv);
}

static void
init_bonobo (void)
{
	if (bonobo_init (CORBA_OBJECT_NIL, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL) == FALSE) {
		e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
			  _("Notes Component: Could not initialize bonobo"));
		exit (1);
	}
}

gint
main (gint argc, gchar **argv)
{
	bindtextdomain (PACKAGE, EVOLUTION_LOCALEDIR);
	textdomain (PACKAGE);

	init_corba (argc, argv);
	init_bonobo ();
	
	e_setup_base_dir ();
	
	notes_factory_init ();
	component_factory_init ();
	
	bonobo_main ();

	return 0;
}
