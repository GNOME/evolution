/*
 * Main evolution shell application
 *
 * Authors:
 *   Miguel de Icaza (miguel@helixcode.com)
 *
 */
#include <config.h>
#include <gnome.h>
#include <libgnorba/gnorba.h>
#include <bonobo/gnome-bonobo.h>
#include <e-util/e-gui-utils.h>
#include <e-util/e-cursors.h>
#include <glade/glade.h>
#include <glade/glade-xml.h>

int shell_debugging = 0;

poptContext ctx;

const struct poptOption shell_popt_options [] = {
	{ "debug", '\0', POPT_ARG_INT, &shell_debugging, 0,
	  N_("Enables some debugging functions"), N_("LEVEL") },
        { NULL, '\0', 0, NULL, 0 }
};

int
main (int argc, char *argv [])
{
	CORBA_Environment ev;
	
	bindtextdomain (PACKAGE, EVOLUTION_LOCALEDIR);
	textdomain (PACKAGE);

	CORBA_exception_init (&ev);
	gnome_CORBA_init_with_popt_table (
		"Evolution", VERSION, &argc, argv,
		shell_popt_options, 0, &ctx, GNORBA_INIT_SERVER_FUNC, &ev);
	CORBA_exception_free (&ev);
	
	if (bonobo_init (gnome_CORBA_ORB (), NULL, NULL) == FALSE){
		e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
			  _("Failed to initialize the Bonobo component system"));
		return 0;
	}

	e_cursors_init ();

	glade_gnome_init ();

	bonobo_activate ();

	gtk_main ();

	/* shutdown */
	e_cursors_shutdown ();

	return 0;
}
