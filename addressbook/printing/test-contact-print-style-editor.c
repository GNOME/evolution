/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * test-contact-print-style-editor.c
 * Copyright (C) 2000  Helix Code, Inc.
 * Author: Chris Lahey <clahey@helixcode.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <gnome.h>
#include "e-contact-print-style-editor.h"

/* This is a horrible thing to do, but it is just a test. */
GtkWidget *editor;

static void destroy_callback(GtkWidget *app, gpointer data)
{
	static int count = 2;
	count --;
	if ( count <= 0 )
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
    gnome_about_new ( _( "Contact Print Style Editor Test" ), VERSION,
		      _( "Copyright (C) 2000, Helix Code, Inc." ),
		      authors,
		      _( "This should test the contact print style editor widget" ),
		      NULL);
  gtk_widget_show (about);                                            
}
#endif

int main( int argc, char *argv[] )
{
  GtkWidget *app;

  /*  bindtextdomain (PACKAGE, GNOMELOCALEDIR);
      textdomain (PACKAGE);*/

  gnome_init( "Contact Print Style Editor Test", VERSION, argc, argv);

  glade_gnome_init ();

  app = gnome_app_new("Contact Print Style Editor Test", NULL);

  editor = e_contact_print_style_editor_new("");
  
  gnome_app_set_contents( GNOME_APP( app ), editor );

  /* Connect the signals */
  gtk_signal_connect( GTK_OBJECT( app ), "destroy",
		      GTK_SIGNAL_FUNC( destroy_callback ),
		      ( gpointer ) app );

  gtk_widget_show_all( app );

  gtk_main(); 

  /* Not reached. */
  return 0;
}
