/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#include <config.h>
#include <stdio.h>
#include <glib.h>

#include <bonobo/bonobo-i18n.h>
#include <bonobo/bonobo-main.h>
#include <libgnome/gnome-init.h>

#include "e-book.h"

static void
add_card_cb (EBook *book, EBookStatus status, const gchar *id, gpointer closure)
{
	ECard *card = E_CARD(closure);
	char *vcard = e_card_get_vcard_assume_utf8(card);
	g_print ("Saved card: %s\n", vcard);
	g_free(vcard);
	g_object_unref(card);
}

static void
book_open_cb (EBook *book, EBookStatus status, gpointer closure)
{
	GList *list = e_card_load_cards_from_file_with_default_charset("gnomecard.vcf", "ISO-8859-1");
	GList *iterator;
	for (iterator = list; iterator; iterator = g_list_next(iterator)) {
		ECard *card = iterator->data;
		e_book_add_card(book, card, add_card_cb, card);
	}
	g_list_free(list);
}

static gboolean
ebook_create (gpointer data)
{
	EBook *book;
	gchar *path, *uri;
	
	book = e_book_new ();

	if (!book) {
		printf ("%s: %s(): Couldn't create EBook, bailing.\n",
			__FILE__,
			G_GNUC_FUNCTION);
		return FALSE;
	}
	

	path = g_build_filename (g_get_home_dir (),
				 "evolution/local/Contacts/addressbook.db",
				 NULL);
	uri = g_strdup_printf ("file://%s", path);
	g_free (path);

	e_book_load_uri (book, uri, book_open_cb, NULL);
	g_free(uri);


	return FALSE;
}

int
main (int argc, char **argv)
{
	GnomeProgram *program;

	program = gnome_program_init ("load-gnomecard-addressbook", VERSION, LIBGNOME_MODULE, argc, argv, 
				      GNOME_PROGRAM_STANDARD_PROPERTIES,
				      NULL);

	g_idle_add (ebook_create, NULL);
	
	bonobo_main ();

	return 0;
}
