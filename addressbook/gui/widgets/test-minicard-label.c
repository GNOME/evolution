/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* test-minicard-label.c
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

#include "config.h"

#include <gtk/gtkmain.h>
#include <libgnomeui/gnome-app.h>
#include <libgnomeui/gnome-canvas-rect-ellipse.h>
#include <libgnomeui/gnome-init.h>
#include <gal/widgets/e-canvas.h>
#include "e-minicard-label.h"

/* This is a horrible thing to do, but it is just a test. */
GnomeCanvasItem *label;
GnomeCanvasItem *rect;

static void destroy_callback(gpointer data, GObject *where_object_was)
{
  exit(0);
}

static void allocate_callback(GtkWidget *canvas, GtkAllocation *allocation, gpointer data)
{
  gnome_canvas_set_scroll_region(GNOME_CANVAS( canvas ), 0, 0, allocation->width, allocation->height );
  gnome_canvas_item_set( label,
			 "width", (double) allocation->width,
			 "height", (double) allocation->height,
			 NULL );
  gnome_canvas_item_set( rect,
			 "x2", (double) allocation->width,
			 "y2", (double) allocation->height,
			 NULL );
}

#if 0
static void about_callback( GtkWidget *widget, gpointer data )
{
  
  const gchar *authors[] =
  {
    "Christopher James Lahey <clahey@umich.edu>",
    NULL
  };

  GtkWidget *about =
    gnome_about_new ( _( "Minicard Label Test" ), VERSION,
		      _( "Copyright (C) 2000, Ximian, Inc." ),
		      authors,
		      _( "This should test the minicard label canvas item" ),
		      NULL);
  gtk_widget_show (about);                                            
}
#endif

static void button_press_callback( GtkWidget *widget, gpointer data )
{
  gnome_canvas_item_grab_focus( label );
}

int main( int argc, char *argv[] )
{
  GtkWidget *app;
  GtkWidget *canvas;

  /*  bindtextdomain (PACKAGE, GNOMELOCALEDIR);
      textdomain (PACKAGE);*/

  gnome_init( "Minicard Label Test", VERSION, argc, argv);
  app = gnome_app_new("Minicard Label Test", NULL);

  canvas = e_canvas_new();
  rect = gnome_canvas_item_new( gnome_canvas_root( GNOME_CANVAS( canvas ) ),
				gnome_canvas_rect_get_type(),
				"x1", (double) 0,
				"y1", (double) 0,
				"x2", (double) 100,
				"y2", (double) 100,
				"fill_color", "white",
				NULL );
  label = e_minicard_label_new(gnome_canvas_root( GNOME_CANVAS( canvas ) ));
  gnome_canvas_item_set( label,
			 "width", (double) 100,
			 "height", (double) 100,
			 "fieldname", "Full Name:",
			 "field", "Christopher James Lahey",
			 NULL );
  gnome_canvas_set_scroll_region ( GNOME_CANVAS( canvas ),
				   0, 0,
				   100, 100 );

  gnome_app_set_contents( GNOME_APP( app ), canvas );


  /* Connect the signals */
  g_object_weak_ref (app, destroy_callback, app);

  g_signal_connect( canvas , "size_allocate",
		    G_CALLBACK ( allocate_callback ),
		    ( gpointer ) app );

  g_signal_connect( canvas , "button_press_event",
		    G_CALLBACK ( button_press_callback ),
		    ( gpointer ) app );

  gtk_widget_show_all( app );

  gtk_main(); 

  /* Not reached. */
  return 0;
}
