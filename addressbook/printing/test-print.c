/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * test-print.c
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
#include <glade/glade.h>
#include "e-contact-print.h"

/* This is a horrible thing to do, but it is just a test. */
GtkWidget *print;

static gint test_close(GnomeDialog *dialog, gpointer data)
{
	exit(0);
	return 1;
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
    gnome_about_new ( _( "Contact Print Test" ), VERSION,
		      _( "Copyright (C) 2000, Helix Code, Inc." ),
		      authors,
		      _( "This should test the contact print code" ),
		      NULL);
  gtk_widget_show (about);                                            
}
#endif

int main( int argc, char *argv[] )
{
  GList *shown_fields = NULL;

  /*  bindtextdomain (PACKAGE, GNOMELOCALEDIR);
      textdomain (PACKAGE);*/

  gnome_init( "Contact Print Test", VERSION, argc, argv);

  glade_gnome_init ();
  
  shown_fields = g_list_append(shown_fields, "First field");
  shown_fields = g_list_append(shown_fields, "Second field");
  shown_fields = g_list_append(shown_fields, "Third field");
  shown_fields = g_list_append(shown_fields, "Fourth field");

  print = e_contact_print_dialog_new(NULL, NULL);
  gtk_widget_show_all(print);
  gtk_signal_connect(GTK_OBJECT(print), "close", GTK_SIGNAL_FUNC(test_close), NULL);

  gtk_main(); 

  /* Not reached. */
  return 0;
}
