/**
 * sample-control-factory.c
 *
 * Copyright 1999, Helix Code, Inc.
 * 
 * Author:
 *   Nat Friedman (nat@nat.org)
 *
 */

#include <config.h>
#include <gnome.h>
#include <bonobo.h>
#include <glade/glade.h>
#include <e-util/e-cursors.h>

#include "addressbook.h"
#include "addressbook-component.h"
#include "select-names/e-select-names-factory.h"

#ifdef USING_OAF

#include <liboaf/liboaf.h>

static void
init_corba (int *argc, char **argv)
{
	gnome_init_with_popt_table ("evolution-addressbook", "0.0",
				    *argc, argv, oaf_popt_options, 0, NULL);

	oaf_init (*argc, argv);
}

#else

#include <libgnorba/gnorba.h>
 
static void
init_corba (int *argc, char **argv)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	gnome_CORBA_init_with_popt_table (
		"evolution-addressbook", "0.0",
		argc, argv, NULL, 0, NULL, GNORBA_INIT_SERVER_FUNC, &ev);

	CORBA_exception_free (&ev);
}

#endif

static void
init_bonobo (int argc, char **argv)
{
	if (bonobo_init (CORBA_OBJECT_NIL, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL) == FALSE)
		g_error (_("Could not initialize Bonobo"));

	glade_gnome_init ();
}

int
main (int argc, char **argv)
{
	init_corba (&argc, argv);

	init_bonobo (argc, argv);

	/* FIXME: Messy names here.  This file should be `main.c'.  `addressbook.c' should
           be `addressbook-control-factory.c' and the functions should be called
           `addressbook_control_factory_something()'.  And `addressbook-component.c'
           should be `addressbook-component-factory.c'.  */

	addressbook_factory_init ();
	addressbook_component_factory_init ();

	e_select_names_factory_init ();

	e_cursors_init();

	bonobo_main ();

	return 0;
}
