/*
  ETextModelTest
*/

#include <gnome.h>
#include <gal/widgets/e-canvas.h>
#include "e-text-model.h"
#include "e-text-model-uri.h"
#include "e-text.h"
#include <gal/util/e-util.h>

#if 0
static void
describe_model (ETextModel *model)
{
  gint i, N;
  g_return_if_fail (E_IS_TEXT_MODEL (model));

  N = e_text_model_object_count (model);

  g_print ("text: %s\n", e_text_model_get_text (model));
  if (N > 0) {
    gchar *s = e_text_model_strdup_expanded_text (model);
    g_print ("expd: %s\n", s);
    g_free (s);
  }
  g_print ("objs: %d\n", N);

  for (i=0; i<N; ++i)
    g_print ("obj%d: %s\n", i, e_text_model_get_nth_object (model, i));
}
#endif

int
main (int argc, gchar **argv)
{
  GtkWidget *win, *canvas;
  GnomeCanvasItem *item;
  ETextModel *model;

  gnome_init ("ETextModelTest", "0.0", argc, argv);

  model = e_text_model_uri_new ();
  e_text_model_set_text (model, "My favorite website is http://www.ximian.com.  My next favorite is http://www.gnome.org.");

  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  
  gtk_widget_push_visual (gdk_rgb_get_visual ());
  gtk_widget_push_colormap (gdk_rgb_get_cmap ());
  canvas = e_canvas_new ();
  gtk_widget_pop_visual ();
  gtk_widget_pop_colormap ();

  item = gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (canvas)),
				e_text_get_type (),
				"model", model,
				"font", "-adobe-helvetica-medium-r-normal--12-120-75-75-p-67-iso8859-1",
				"anchor", GTK_ANCHOR_SOUTH_WEST,
				"line_wrap", TRUE,
				"width", 150.0,
				"editable", TRUE,
				NULL);

  gtk_container_add (GTK_CONTAINER (win), canvas);
  gtk_widget_show_all (win);

  gtk_main ();

  return 0;
}
