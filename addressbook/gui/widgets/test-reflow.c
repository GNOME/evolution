/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* test-reflow.c
 *
 * Copyright (C) 2000 Ximian, Inc.
 * Author: Chris Lahey <clahey@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#define TEST_VCARD                   \
"BEGIN:VCARD
"                      \
"FN:Nat
"                           \
"N:Friedman;Nat;D;Mr.
"             \
"TITLE:Head Geek
"                  \
"BDAY:1977-08-06
"                  \
"TEL;WORK:617 679 1984
"            \
"TEL;CELL:123 456 7890
"            \
"EMAIL;INTERNET:nat@nat.org
"       \
"EMAIL;INTERNET:nat@ximian.com
" \
"ADR;WORK;POSTAL:P.O. Box 101;;;Any Town;CA;91921-1234;
" \
"ADR;HOME;POSTAL;INTL:P.O. Box 202;;;Any Town 2;MI;12344-4321;USA
" \
"END:VCARD
"                        \
"
"


#include "config.h"

#include <gtk/gtkmain.h>
#include <gtk/gtkvbox.h>
#include <libgnomeui/gnome-canvas-rect-ellipse.h>
#include <libgnomeui/gnome-init.h>
#include <gal/widgets/e-canvas.h>
#include <gal/widgets/e-reflow.h>
#include <gal/widgets/e-scroll-frame.h>

#include "e-minicard.h"

/* This is a horrible thing to do, but it is just a test. */
GnomeCanvasItem *reflow;
GnomeCanvasItem *rect;
GtkAllocation last_alloc;

static void destroy_callback(gpointer data, GObject *where_object_was)
{
  exit(0);
}

static void allocate_callback(GtkWidget *canvas, GtkAllocation *allocation, gpointer data)
{
  double width;
  last_alloc = *allocation;
  gnome_canvas_item_set( reflow,
			 "height", (double) allocation->height,
			 NULL );
  gnome_canvas_item_set( reflow,
			 "minimum_width", (double) allocation->width,
			 NULL );
  g_object_get(reflow,
	       "width", &width,
	       NULL);
  width = MAX(width, allocation->width);
  gnome_canvas_set_scroll_region(GNOME_CANVAS( canvas ), 0, 0, width - 1, allocation->height - 1);
  gnome_canvas_item_set( rect,
			 "x2", (double) width,
			 "y2", (double) allocation->height,
			 NULL );
}

static void resize(GnomeCanvas *canvas, gpointer data)
{
	double width;
	g_object_get(reflow,
		     "width", &width,
		     NULL);
	width = MAX(width, last_alloc.width);
	gnome_canvas_set_scroll_region(canvas , 0, 0, width - 1, last_alloc.height - 1);
	gnome_canvas_item_set( rect,
			       "x2", (double) width,
			       "y2", (double) last_alloc.height,
			       NULL );	
}

int main( int argc, char *argv[] )
{
  GtkWidget *app;
  GtkWidget *canvas;
  GtkWidget *vbox;
  GtkWidget *scrollframe;
  int i;

  /*  bindtextdomain (PACKAGE, GNOMELOCALEDIR);
      textdomain (PACKAGE);*/

  gnome_init( "Reflow Test", VERSION, argc, argv);
  app = gnome_app_new("Reflow Test", NULL);

  vbox = gtk_vbox_new(FALSE, 0);

  canvas = e_canvas_new();
  rect = gnome_canvas_item_new( gnome_canvas_root( GNOME_CANVAS( canvas ) ),
				gnome_canvas_rect_get_type(),
				"x1", (double) 0,
				"y1", (double) 0,
				"x2", (double) 100,
				"y2", (double) 100,
				"fill_color", "white",
				NULL );
  reflow = gnome_canvas_item_new( gnome_canvas_root( GNOME_CANVAS( canvas ) ),
				  e_reflow_get_type(),
				  "height", (double) 100,
				  "minimum_width", (double) 100,
				  NULL );
  g_signal_connect( canvas, "reflow",
		    G_CALLBACK ( resize ),
		    ( gpointer ) app);
  for ( i = 0; i < 200; i++ )
    {
      GnomeCanvasItem *item;
      ECard *card = e_card_new (TEST_VCARD);
      item = gnome_canvas_item_new( GNOME_CANVAS_GROUP(reflow),
				    e_minicard_get_type(),
				    "card", card,
				    NULL);
      e_reflow_add_item(E_REFLOW(reflow), item, NULL);
    }
  gnome_canvas_set_scroll_region ( GNOME_CANVAS( canvas ),
				   0, 0,
				   100, 100 );

  scrollframe = e_scroll_frame_new (gtk_layout_get_hadjustment(GTK_LAYOUT(canvas)),
				    gtk_layout_get_vadjustment(GTK_LAYOUT(canvas)));
  e_scroll_frame_set_policy (E_SCROLL_FRAME (scrollframe),
			     GTK_POLICY_AUTOMATIC,
			     GTK_POLICY_NEVER);
  
  gtk_container_add (GTK_CONTAINER (scrollframe), canvas);

  gnome_app_set_contents( GNOME_APP( app ), scrollframe );

  /* Connect the signals */
  g_object_weak_ref (app, destroy_callback, app);

  g_signal_connect( canvas, "size_allocate",
		    G_CALLBACK ( allocate_callback ),
		    ( gpointer ) app );

  gtk_widget_show_all( app );
  gdk_window_set_back_pixmap( GTK_LAYOUT(canvas)->bin_window, NULL, FALSE);

  gtk_main(); 

  /* Not reached. */
  return 0;
}
