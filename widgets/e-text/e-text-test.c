/*
  ETextTest: E-Text item test program
  Copyright (C)2000: Iain Holmes  <ih@csd.abdn.ac.uk>

  This code is licensed under the GPL
*/

#include "e-text.h"
#include <gnome.h>

static void
quit_cb (GtkWidget *widget,
	 gpointer data)
{
  gtk_main_quit ();
}

static void
change_text_cb (GtkEntry *entry,
		EText *text)
{
  gchar *str;

  str = gtk_entry_get_text (entry);
  gnome_canvas_item_set (GNOME_CANVAS_ITEM (text),
			 "text", str,
			 NULL);
}

static void
change_font_cb (GtkEntry *entry,
		EText *text)
{
  gchar *font;

  font = gtk_entry_get_text (entry);
  gnome_canvas_item_set (GNOME_CANVAS_ITEM (text),
			 "font", font,
			 NULL);
}

int
main (int argc,
      char **argv)
{
  GtkWidget *window, *canvas, *scroller, *vbox, *text, *font;
  GtkWidget *frame;
  GnomeCanvasItem *item;

  gnome_init ("ETextTest", "0.0.1", argc, argv);
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "EText Test");
  gtk_signal_connect (GTK_OBJECT (window), "destroy",
		      GTK_SIGNAL_FUNC (quit_cb), NULL);

  gtk_widget_push_visual (gdk_rgb_get_visual ());
  gtk_widget_push_colormap (gdk_rgb_get_cmap ());
  canvas = gnome_canvas_new ();
  gtk_widget_pop_visual ();
  gtk_widget_pop_colormap ();
  scroller = gtk_scrolled_window_new (NULL, NULL);
  vbox = gtk_vbox_new (FALSE, 2);

  gtk_container_add (GTK_CONTAINER (window), vbox);
  gtk_box_pack_start (GTK_BOX (vbox), scroller, TRUE, TRUE, 2);
  gtk_container_add (GTK_CONTAINER (scroller), canvas);

  frame = gtk_frame_new ("Text");
  text = gtk_entry_new ();
  gtk_container_add (GTK_CONTAINER (frame), text);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
  
  frame = gtk_frame_new ("Font");
  font = gtk_entry_new ();
  gtk_container_add (GTK_CONTAINER (frame), font);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);

  item = gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (canvas)),
				e_text_get_type (),
				"text", "Hello World! This is a really long string to test out the ellipsis stuff.",
				"x", 10.0,
				"y", 10.0,
				"font", "-adobe-helvetica-medium-r-normal--12-*-72-72-p-*-iso8859-1",
				"fill_color", "black",
				"anchor", GTK_ANCHOR_NW,
				"use_ellipsis", TRUE,
				"ellipsis", "...",
				"editable", TRUE,
				"line_wrap", TRUE,
				"max_lines", 3,
				"clip_width", 150.0,
				NULL);

  gtk_signal_connect (GTK_OBJECT (text), "activate",
		      GTK_SIGNAL_FUNC (change_text_cb), item);
  gtk_signal_connect (GTK_OBJECT (font), "activate",
		      GTK_SIGNAL_FUNC (change_font_cb), item);

  gnome_canvas_set_scroll_region (GNOME_CANVAS (canvas), 0.0, 0.0, 400.0, 400.0);
  gtk_widget_show_all (window);
  gtk_main ();

  return 0;
}
