/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#include <config.h>

#include <glib.h>
#include <bonobo/bonobo-i18n.h>
#include <bonobo/bonobo-main.h>
#include <libgnome/gnome-init.h>

#include "e-book.h"

static void
init_bonobo (int *argc, char **argv)
{
	if (bonobo_init (argc, argv) == FALSE)
		g_error (_("Could not initialize Bonobo"));
}

static void
get_cursor_cb (EBook *book, EBookStatus status, ECardCursor *cursor, gpointer closure)
{
	long length = e_card_cursor_get_length(cursor);
	long i;
	
	printf ("Length: %d\n", (int) length);
	for ( i = 0; i < length; i++ ) {
		ECard *card = e_card_cursor_get_nth(cursor, i);
		char *vcard = e_card_get_vcard_assume_utf8(card);
		printf("[%s]\n", vcard);
		g_free(vcard);
		g_object_unref(card);
	}
}

static void
book_open_cb (EBook *book, EBookStatus status, gpointer closure)
{
	printf ("Book opened.\n");
	e_book_get_cursor(book, "", get_cursor_cb, NULL);
}

static gboolean
ebook_create (gpointer data)
{
	EBook *book;
	
	book = e_book_new ();

	e_book_load_uri (book, "file:/tmp/test.db", book_open_cb, NULL);

	return FALSE;
}

int
main (int argc, char **argv)
{
	gnome_program_init("test-client-list", "0.0", LIBGNOME_MODULE, argc, argv, NULL);

	init_bonobo (&argc, argv);

	g_idle_add (ebook_create, NULL);

	bonobo_main ();

	return 0;
}
