/*
 * Tests the mail summary display bonobo component
 *
 * Author:
 *   Miguel de Icaza (miguel@kernel.org)
 *
 * (C) 2000 Helix Code, Inc.
 */
#include <gnome.h>
#include <bonobo.h>
#include <libgnorba/gnorba.h>

static guint
create_container (void)
{
	GtkWidget *window, *control;
	BonoboUIHandler *uih;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_widget_show (window);

	uih = bonobo_ui_handler_new ();
	control = bonobo_widget_new_control ("GOADID:Evolution:FolderBrowser:1.0",
				     bonobo_object_corba_objref (BONOBO_OBJECT (uih)));

	
	if (control == NULL){
		printf ("Could not launch mail control\n");
		exit (1);
	}
	gtk_container_add (GTK_CONTAINER (window), control);

	gtk_widget_show (window);
	gtk_widget_show (control);


	return FALSE;
}

int
main (int argc, char *argv [])
{
	CORBA_Environment ev;
	CORBA_ORB orb;

	CORBA_exception_init (&ev);

	gnome_CORBA_init ("sample-control-container", "1.0", &argc, argv, 0, &ev);

	CORBA_exception_free (&ev);

	orb = gnome_CORBA_ORB ();

	if (bonobo_init (orb, NULL, NULL) == FALSE)
		g_error ("Could not initialize Bonobo\n");

	
	
	gtk_idle_add ((GtkFunction) create_container, NULL);

	/*
	 * Main loop
	 */
	bonobo_main ();
	
	return 0;
}
