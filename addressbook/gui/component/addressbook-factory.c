/**
 * sample-control-factory.c
 *
 * Copyright 1999, Ximian, Inc.
 * 
 * Author:
 *   Nat Friedman (nat@nat.org)
 *
 */

#include <config.h>
#include <glib.h>
#include <libgnomeui/gnome-ui-init.h>
#include <bonobo/bonobo-main.h>
#include <glade/glade.h>
#include <gal/widgets/e-cursors.h>
#include <e-util/e-passwords.h>

#include <camel/camel.h>

#include "addressbook.h"
#include "addressbook-component.h"
#include "e-address-widget.h"
#include "e-address-popup.h"
#include "addressbook/gui/widgets/e-minicard-control.h"
#include "select-names/e-select-names-factory.h"

int
main (int argc, char **argv)
{
	bindtextdomain (PACKAGE, EVOLUTION_LOCALEDIR);
	textdomain (PACKAGE);

	free (malloc (5));

	gnome_program_init ("evolution-addressbook", VERSION,
			    LIBGNOMEUI_MODULE, argc, argv,
			    GNOME_PROGRAM_STANDARD_PROPERTIES,
			    NULL);

	/* FIXME: Messy names here.  This file should be `main.c'.  `addressbook.c' should
           be `addressbook-control-factory.c' and the functions should be called
           `addressbook_control_factory_something()'.  And `addressbook-component.c'
           should be `addressbook-component-factory.c'.  */

	addressbook_factory_init ();
	addressbook_component_factory_init ();

	e_select_names_factory_init ();
	
	e_minicard_control_factory_init ();

	e_address_widget_factory_init ();
	e_address_popup_factory_init ();

	glade_gnome_init ();
	e_cursors_init();

	e_passwords_init("Addressbook");

#if 0
	g_log_set_always_fatal (G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING);
#endif

	/*g_thread_init (NULL);*/
	camel_type_init ();

	gtk_widget_push_visual (gdk_rgb_get_visual ());
	gtk_widget_push_colormap (gdk_rgb_get_cmap ());

	g_print ("Evolution Addressbook up and running\n");

	bonobo_main ();

	return 0;
}
