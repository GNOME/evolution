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
  g_print ("objs: %d\n", N);

  for (i=0; i<N; ++i) {
    gchar *s = e_text_model_strdup_nth_object (model, i);
    g_print ("obj%d: %s\n", i, s);
    g_free (s);
  }
}
#endif

int
main (int argc, gchar **argv)
{
  GtkWidget *win[2], *canvas[2];
  GnomeCanvasItem *item[2];
  ETextModel *model;
  gint i;

  gnome_init ("ETextModelTest", "0.0", argc, argv);

  model = e_text_model_uri_new ();

  e_text_model_set_text (model, "My favorite website is http://www.ximian.com.  My next favorite www.assbarn.com.");

  //  describe_model (model);
  
  for (i=0; i<2; ++i) {
    win[i] = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  
    gtk_widget_push_visual (gdk_rgb_get_visual ());
    gtk_widget_push_colormap (gdk_rgb_get_cmap ());
    canvas[i] = e_canvas_new ();
    gtk_widget_pop_visual ();
    gtk_widget_pop_colormap ();

    item[i] = gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (canvas[i])),
				     e_text_get_type (),
				     "model", model,
				     "font", "-adobe-helvetica-medium-r-normal--12-120-75-75-p-67-iso8859-1",
				     "anchor", GTK_ANCHOR_NORTH,
				     "line_wrap", TRUE,
				     "width", 150.0,
				     "editable", TRUE,
				     NULL);

    gtk_container_add (GTK_CONTAINER (win[i]), canvas[i]);
    gtk_widget_show_all (win[i]);
  }

  gtk_main ();

  return 0;
}
