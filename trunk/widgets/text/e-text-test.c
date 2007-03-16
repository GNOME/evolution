/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-text-test.c - E-Text item test program
 * Copyright 2000: Iain Holmes <ih@csd.abdn.ac.uk>
 *
 * Authors:
 *   Iain Holmes <ih@csd.abdn.ac.uk>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <gnome.h>

#include "misc/e-canvas.h"
#include "misc/e-unicode.h"

#include "e-text.h"

GnomeCanvasItem *rect;

static void allocate_callback(GtkWidget *canvas, GtkAllocation *allocation, GnomeCanvasItem *item)
{
  double height;
  gnome_canvas_item_set( item,
			 "width", (double) allocation->width,
			 NULL );
  g_object_get(item,
	       "height", &height,
	       NULL);
  height = MAX(height, allocation->height);
  gnome_canvas_set_scroll_region(GNOME_CANVAS( canvas ), 0, 0, allocation->width, height );
  gnome_canvas_item_set( rect,
			 "x2", (double) allocation->width,
			 "y2", (double) height,
			 NULL );
}

static void
reflow (GtkWidget *canvas, GnomeCanvasItem *item)
{
  double height;
  g_object_get(item,
	       "height", &height,
	       NULL);
  height = MAX(height, canvas->allocation.height);
  gnome_canvas_set_scroll_region(GNOME_CANVAS( canvas ), 0, 0, canvas->allocation.width, height );
  gnome_canvas_item_set( rect,
			 "x2", (double) canvas->allocation.width,
			 "y2", (double) height,
			 NULL );
}

static void
quit_cb (gpointer data, GObject *where_object_was)
{
  gtk_main_quit ();
}

static void
change_text_cb (GtkEntry *entry,
		EText *text)
{
  gchar *str;

  str = e_utf8_gtk_entry_get_text (entry);
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
  g_object_weak_ref (G_OBJECT (window),
		     quit_cb, NULL);

  gtk_widget_push_colormap (gdk_rgb_get_cmap ());
  canvas = e_canvas_new ();
  gtk_widget_pop_colormap ();
  scroller = gtk_scrolled_window_new (NULL, NULL);
  vbox = gtk_vbox_new (FALSE, 2);

  gtk_container_add (GTK_CONTAINER (window), vbox);
  gtk_box_pack_start (GTK_BOX (vbox), scroller, TRUE, TRUE, 2);
  gtk_container_add (GTK_CONTAINER (scroller), canvas);

  frame = gtk_frame_new ("Text");
  text = gtk_entry_new ();
  gtk_entry_set_text(GTK_ENTRY(text), "Hello World! This is a really long string to test out the ellipsis stuff.");
  gtk_container_add (GTK_CONTAINER (frame), text);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);
  
  frame = gtk_frame_new ("Font");
  font = gtk_entry_new ();
  gtk_entry_set_text(GTK_ENTRY(font), "fixed");
  gtk_container_add (GTK_CONTAINER (frame), font);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 0);

  rect = gnome_canvas_item_new( gnome_canvas_root( GNOME_CANVAS( canvas ) ),
				gnome_canvas_rect_get_type(),
				"x1", (double) 0,
				"y1", (double) 0,
				"x2", (double) 100,
				"y2", (double) 100,
				"fill_color", "white",
				NULL );

  item = gnome_canvas_item_new (gnome_canvas_root (GNOME_CANVAS (canvas)),
				e_text_get_type (),
				"text", "Hello World! This is a really long string to test out the ellipsis stuff.",
				"font", "fixed",
				"fill_color", "black",
				"anchor", GTK_ANCHOR_NW,
				"clip", TRUE,
				"use_ellipsis", TRUE,
				"editable", TRUE,
				"line_wrap", TRUE,
				"max_lines", 2,
				"width", 150.0,
				NULL);

  g_signal_connect (text, "activate",
		    G_CALLBACK (change_text_cb), item);
  g_signal_connect (font, "activate",
		    G_CALLBACK (change_font_cb), item);

  g_signal_connect (canvas , "size_allocate",
		    G_CALLBACK (allocate_callback),
		    item );
  g_signal_connect (canvas , "reflow",
		    G_CALLBACK (reflow),
		    item );
  gnome_canvas_set_scroll_region (GNOME_CANVAS (canvas), 0.0, 0.0, 400.0, 400.0);
  gtk_widget_show_all (window);
  gtk_main ();

  return 0;
}
