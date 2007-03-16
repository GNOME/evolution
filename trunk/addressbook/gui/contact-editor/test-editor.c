/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * test-editor.c
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <stdlib.h>
#include <gtk/gtkmain.h>
#include <libgnomeui/gnome-app.h>
#include <libgnomeui/gnome-ui-init.h>
#include <glade/glade.h>
#include "e-contact-editor.h"
#include "ebook/e-card.h"

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
	g_object_unref (ce);

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
		      _( "Copyright (C) 2000, Ximian, Inc." ),
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

	gnome_program_init("Contact Editor Test", VERSION, LIBGNOMEUI_MODULE, argc, argv, NULL);

	glade_init ();

	cardstr = NULL;
	if (argc == 2)
		cardstr = read_file (argv [1]);

	if (cardstr == NULL)
		cardstr = TEST_VCARD;

	ce = e_contact_editor_new (NULL, e_card_new_with_default_charset (cardstr, "ISO-8859-1"), TRUE, FALSE);
	g_signal_connect (ce, "editor_closed",
			  G_CALLBACK (editor_closed_cb), NULL);

	ce = e_contact_editor_new (NULL, e_card_new_with_default_charset (cardstr, "ISO-8859-1"), TRUE, FALSE);
	g_signal_connect (ce, "editor_closed",
			  G_CALLBACK (editor_closed_cb), NULL);

	gtk_main();

	/* Not reached. */
	return 0;
}
