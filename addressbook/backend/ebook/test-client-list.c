/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#include <config.h>
#include <bonobo.h>
#include <gnome.h>
#include <libgnorba/gnorba.h>

#include <e-book.h>

CORBA_Environment ev;
CORBA_ORB orb;

static void
init_bonobo (int argc, char **argv)
{

	gnome_CORBA_init_with_popt_table (
		"blah", "0.0",
		&argc, argv, NULL, 0, NULL, GNORBA_INIT_SERVER_FUNC, &ev);

	orb = gnome_CORBA_ORB ();

	if (bonobo_init (orb, NULL, NULL) == FALSE)
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
		char *vcard = e_card_get_vcard(card);
		printf("[%s]\n", vcard);
		g_free(vcard);
		gtk_object_unref(GTK_OBJECT(card));
	}
}

static void
book_open_cb (EBook *book, EBookStatus status, gpointer closure)
{
	printf ("Book opened.\n");
	e_book_get_cursor(book, "", get_cursor_cb, NULL);
}

static guint
ebook_create (void)
{
	EBook *book;
	
	book = e_book_new ();

	if (! e_book_load_uri (book, "file:/tmp/test.db", book_open_cb, NULL)) {
		printf ("error calling load_uri!\n");
	}


	return FALSE;
}

int
main (int argc, char **argv)
{

	CORBA_exception_init (&ev);
	init_bonobo (argc, argv);

	gtk_idle_add ((GtkFunction) ebook_create, NULL);

	bonobo_main ();

	return 0;
}
