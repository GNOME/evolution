/*
 * Tests the mail summary display bonobo component
 *
 * Author:
 *   Miguel de Icaza (miguel@kernel.org)
 *
 * (C) 2000 Helix Code, Inc.
 */

#include <config.h>

#include <gtk/gtkmain.h>
#include <gtk/gtkwidget.h>
#include <libgnome/gnome-defs.h>
#include <libgnomeui/gnome-init.h>
#include <liboaf/liboaf.h>
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-ui-container.h>
#include <bonobo/bonobo-widget.h>

static guint
create_container (void)
{
	GtkWidget *window, *control;
	BonoboUIContainer *container;

	gdk_rgb_init ();

	gtk_widget_set_default_colormap (gdk_rgb_get_cmap ());
	gtk_widget_set_default_visual (gdk_rgb_get_visual ());

	window = bonobo_window_new ("Test", "test");
	gtk_widget_set_usize (GTK_WIDGET (window), 640, 480);
	gtk_widget_show (GTK_WIDGET (window));

	container = bonobo_ui_container_new ();
	bonobo_ui_container_set_win (container, BONOBO_WINDOW (window));

	control = bonobo_widget_new_control (
		"OAFIID:GNOME_Evolution_Mail_Control",
		bonobo_object_corba_objref (BONOBO_OBJECT (container)));
	
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





