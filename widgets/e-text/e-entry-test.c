/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* test-minicard.c
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
#include "e-entry.h"

static void destroy_callback(GtkWidget *app, gpointer data)
{
  exit(0);
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
    gnome_about_new ( _( "Minicard Test" ), VERSION,
		      _( "Copyright (C) 2000, Helix Code, Inc." ),
		      authors,
		      _( "This should test the minicard canvas item" ),
		      NULL);
  gtk_widget_show (about);                                            
}
#endif

int main( int argc, char *argv[] )
{
  GtkWidget *app;
  GtkWidget *entry;

  /*  bindtextdomain (PACKAGE, GNOMELOCALEDIR);
      textdomain (PACKAGE);*/

  gnome_init( "EEntry Test", VERSION, argc, argv);
  app = gnome_app_new("EEntry Test", NULL);

  entry = e_entry_new();
  gtk_object_set(GTK_OBJECT(entry),
		 "editable", TRUE,
		 "use_ellipsis", TRUE,
		 NULL);
  gnome_app_set_contents( GNOME_APP( app ), entry );

  /* Connect the signals */
  gtk_signal_connect( GTK_OBJECT( app ), "destroy",
		      GTK_SIGNAL_FUNC( destroy_callback ),
		      ( gpointer ) app );

  gtk_widget_show_all( app );

  gtk_main(); 

  /* Not reached. */
  return 0;
}
