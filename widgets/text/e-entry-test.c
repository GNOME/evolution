/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-entry-test.c
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
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
