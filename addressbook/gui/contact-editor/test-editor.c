/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * test-editor.c
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
#include "e-contact-editor.h"

#define TEST_VCARD                   \
"BEGIN:VCARD
"                      \
"FN:Nat
"                           \
"N:Friedman;Nat;D;Mr.
"             \
"BDAY:1977-08-06
"                  \
"TEL;WORK:617 679 1984
"            \
"TEL;CELL:123 456 7890
"            \
"EMAIL;INTERNET:nat@nat.org
"       \
"EMAIL;INTERNET:nat@helixcode.com
" \
"ADR;WORK;POSTAL:P.O. Box 101;;;Any Town;CA;91921-1234;
" \
"ADR;HOME;POSTAL;INTL:P.O. Box 202;;;Any Town 2;MI;12344-4321;USA
" \
"END:VCARD
"                        \
"
"

static char *
read_file (char *name)
{
	int  len;
	char buff[65536];
	char line[1024];
	FILE *f;

	f = fopen (name, "r");
	if (f == NULL)
		g_error ("Unable to open %s!\n", name);

	len  = 0;
	while (fgets (line, sizeof (line), f) != NULL) {
		strcpy (buff + len, line);
		len += strlen (line);
	}

	fclose (f);

	return g_strdup (buff);
}

/* Callback used when a contact editor is closed */
static void
editor_closed_cb (EContactEditor *ce, gpointer data)
{
	static int count = 2;

	count--;
	gtk_object_unref (GTK_OBJECT (ce));

	if (count == 0)
		exit (0);
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
    gnome_about_new ( _( "Contact Editor Test" ), VERSION,
		      _( "Copyright (C) 2000, Helix Code, Inc." ),
		      authors,
		      _( "This should test the contact editor canvas item" ),
		      NULL);
  gtk_widget_show (about);
}
#endif

int main( int argc, char *argv[] )
{
	char *cardstr;
	EContactEditor *ce;

	/*  bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	    textdomain (PACKAGE);*/

	gnome_init( "Contact Editor Test", VERSION, argc, argv);

	glade_gnome_init ();

	cardstr = NULL;
	if (argc == 2)
		cardstr = read_file (argv [1]);

	if (cardstr == NULL)
		cardstr = TEST_VCARD;

	ce = e_contact_editor_new (e_card_new (cardstr), TRUE);
	gtk_signal_connect (GTK_OBJECT (ce), "editor_closed",
			    GTK_SIGNAL_FUNC (editor_closed_cb), NULL);

	ce = e_contact_editor_new (e_card_new (cardstr), TRUE);
	gtk_signal_connect (GTK_OBJECT (ce), "editor_closed",
			    GTK_SIGNAL_FUNC (editor_closed_cb), NULL);

	gtk_main();

	/* Not reached. */
	return 0;
}
