/*
 * Tests the mail summary display bonobo component
 *
 * Author:
 *   Miguel de Icaza (miguel@kernel.org)
 *
 * (C) 2000 Helix Code, Inc.
 */

#include <config.h>

#include <gnome.h>
#include <bonobo.h>
#include <liboaf/liboaf.h>

static guint
create_container (void)
{
	GtkWidget *window, *control;
	BonoboUIHandler *uih;

	gdk_rgb_init ();

	gtk_widget_set_default_colormap (gdk_rgb_get_cmap ());
	gtk_widget_set_default_visual (gdk_rgb_get_visual ());

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_widget_set_usize (GTK_WIDGET (window), 640, 480);
	gtk_widget_show (GTK_WIDGET (window));

	uih = bonobo_ui_handler_new ();

	control = bonobo_widget_new_control ("OAFIID:control:evolution-mail:833d5a71-a201-4a0e-b7e6-5475c5c4cb45",
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
	gnome_init ("sample-control-container", "1.0", argc, argv);
	oaf_init (argc, argv);

	if (bonobo_init (CORBA_OBJECT_NIL, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL) == FALSE)
		g_error ("Could not initialize Bonobo\n");
	
	gtk_idle_add ((GtkFunction) create_container, NULL);

	/*
	 * Main loop
	 */
	bonobo_main ();
	
	return 0;
}





