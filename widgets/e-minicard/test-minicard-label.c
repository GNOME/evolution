/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* test-minicard-label.c
 *
 * Copyright (C) 2000 Helix Code, Inc.
 * Author: Chris Lahey <clahey@helixcode.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */



#include "config.h"

#include <gnome.h>
#include "e-minicard-label.h"

/* This is a horrible thing to do, but it is just a test. */
GnomeCanvasItem *label;
GnomeCanvasItem *rect;

static void destroy_callback(GtkWidget *app, gpointer data)
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
		      _( "Copyright (C) 2000, Helix Code, Inc." ),
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

  canvas = gnome_canvas_new();
  rect = gnome_canvas_item_new( gnome_canvas_root( GNOME_CANVAS( canvas ) ),
				gnome_canvas_rect_get_type(),
				"x1", (double) 0,
				"y1", (double) 0,
				"x2", (double) 100,
				"y2", (double) 100,
				"fill_color", "white",
				NULL );
  label = gnome_canvas_item_new( gnome_canvas_root( GNOME_CANVAS( canvas ) ),
				 e_minicard_label_get_type(),
				 "x", (double) 0,
				 "y", (double) 0,
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
  gtk_signal_connect( GTK_OBJECT( app ), "destroy",
		      GTK_SIGNAL_FUNC( destroy_callback ),
		      ( gpointer ) app );

  gtk_signal_connect( GTK_OBJECT( canvas ), "size_allocate",
		      GTK_SIGNAL_FUNC( allocate_callback ),
		      ( gpointer ) app );

  gtk_signal_connect( GTK_OBJECT( canvas ), "button_press_event",
		      GTK_SIGNAL_FUNC( button_press_callback ),
		      ( gpointer ) app );

  gtk_widget_show_all( app );

  gtk_main(); 

  /* Not reached. */
  return 0;
}
