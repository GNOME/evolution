/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#include <config.h>
#include <bonobo.h>
#include <gnome.h>
#include <libgnorba/gnorba.h>

#include <e-book.h>

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
"END:VCARD
"                        \
"
"

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
	
	for ( i = 0; i < length; i++ ) {
		ECard *card = e_card_cursor_get_nth(cursor, i);
		char *vcard = e_card_get_vcard(card);
		printf("[%s]\n", vcard);
		g_free(vcard);
		gtk_object_unref(GTK_OBJECT(card));
	}
	gtk_object_unref(GTK_OBJECT(cursor));
}

static void
add_card_cb (EBook *book, EBookStatus status, const gchar *id, gpointer closure)
{
	char *vcard;
	ECard *card;
	GTimer *timer;

	printf ("Status: %d\n", status);

	printf ("Id: %s\n", id);

	timer = g_timer_new ();
	g_timer_start (timer);
	card = e_book_get_card (book, id);
	g_timer_stop (timer);

	vcard = e_card_get_vcard(card);
	printf ("%g\n", g_timer_elapsed (timer, NULL));
	printf ("[%s]\n", vcard);
	g_free(vcard);
	gtk_object_unref(GTK_OBJECT(card));

	e_book_get_all_cards(book, get_cursor_cb, NULL);
}

static void
book_open_cb (EBook *book, EBookStatus status, gpointer closure)
{
	e_book_add_vcard(book, TEST_VCARD, add_card_cb, NULL);
}

static guint
ebook_create (void)
{
	EBook *book;
	
	book = e_book_new ();

	if (!book) {
		printf ("%s: %s: Couldn't create EBook, bailing.\n",
			__FILE__,
			__FUNCTION__);
		return FALSE;
	}
	

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
