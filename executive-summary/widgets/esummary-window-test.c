#include <gnome.h>
#include <gal/widgets/e-canvas.h>
#include "e-summary-subwindow.h"
#include "e-summary-titlebar.h"

void
close_test (GtkWidget *widget,
	    gpointer data)
{
  gtk_main_quit ();
}

int
main (int argc,
      char **argv)
{
  GtkWidget *window, *canvas;
  ESummarySubwindow *subwindow;
  GtkWidget *control;

  gnome_init ("Executive Summary Subwindow Test", "1.0", argc, argv);
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_signal_connect (GTK_OBJECT (window), "destroy",
		      GTK_SIGNAL_FUNC (close_test), NULL);

  canvas = e_canvas_new ();

  subwindow = e_summary_subwindow_new (GNOME_CANVAS_GROUP (GNOME_CANVAS (canvas)->root), 100, 100);

  control = gtk_button_new_with_label ("A big button");
  gtk_widget_set_usize (control, 400, 200);

  e_summary_subwindow_add (subwindow, control); 
  gtk_widget_show (control);

  gnome_canvas_set_scroll_region (GNOME_CANVAS (canvas), 0.0, 0.0, 1000.0, 1300.0); 
  gtk_container_add (GTK_CONTAINER (window), canvas);
  gtk_widget_show_all (window);

  gtk_main ();

  exit(0);
}
