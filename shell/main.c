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
#include <bonobo.h>
#include <e-util/e-gui-utils.h>
#include <e-util/e-cursors.h>
#include <glade/glade.h>
#include <glade/glade-xml.h>
#include "e-shell.h"
#include "e-shell-view.h"

int shell_debugging = 0;

poptContext ctx;

EShell *eshell;

const struct poptOption shell_popt_options [] = {
	{ "debug", '\0', POPT_ARG_INT, &shell_debugging, 0,
	  N_("Enables some debugging functions"), N_("LEVEL") },
        { NULL, '\0', 0, NULL, 0 }
};

static void
corba_init (int *argc, char *argv [])
{
	CORBA_Environment ev;
	
	CORBA_exception_init (&ev);
	gnome_CORBA_init_with_popt_table (
		"Evolution", VERSION, argc, argv,
		shell_popt_options, 0, &ctx, GNORBA_INIT_SERVER_FUNC, &ev);
	CORBA_exception_free (&ev);
	
	if (bonobo_init (gnome_CORBA_ORB (), NULL, NULL) == FALSE){
		e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
			  _("Failed to initialize the Bonobo component system"));
		exit (1);
	}
}

static void
gui_init (void)
{
	e_cursors_init ();

	glade_gnome_init ();

	bonobo_activate ();
}

static void
gui_shutdown (void)
{
	/* shutdown */
	e_cursors_shutdown ();
}

static void
evolution_boot (void)
{
	EShellView *e_shell_view;

	eshell = e_shell_new ();
	e_shell_view = E_SHELL_VIEW (
		e_shell_view_new (eshell,
				  eshell->default_folders.inbox,
				  TRUE));

	gtk_widget_show (GTK_WIDGET (e_shell_view));
}

int
main (int argc, char *argv [])
{
	bindtextdomain (PACKAGE, EVOLUTION_LOCALEDIR);
	textdomain (PACKAGE);

	corba_init (&argc, argv);
	gui_init ();

	evolution_boot ();
	
	gtk_main ();

	gui_shutdown ();

	return 0;
}
