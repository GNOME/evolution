/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * test-print.c
 * Copyright (C) 2000  Ximian, Inc.
 * Author: Chris Lahey <clahey@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"

#include <stdlib.h>
#include <gtk/gtkmain.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-ui-init.h>
#include <glade/glade.h>
#include <bonobo/bonobo-main.h>
#include "e-contact-print.h"

/* This is a horrible thing to do, but it is just a test. */
GtkWidget *print;

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
		      _( "Copyright (C) 2000, Ximian, Inc." ),
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

  gnome_program_init ("Contact Print Test", VERSION,
		      LIBGNOMEUI_MODULE,
		      argc, argv,
		      NULL);

  glade_init ();
  
  shown_fields = g_list_append(shown_fields, "First field");
  shown_fields = g_list_append(shown_fields, "Second field");
  shown_fields = g_list_append(shown_fields, "Third field");
  shown_fields = g_list_append(shown_fields, "Fourth field");

  /* does nothing */
  e_contact_print (NULL, NULL, NULL, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG);

  bonobo_main(); 

  /* Not reached. */
  return 0;
}
