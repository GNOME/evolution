/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#include <config.h>
#include <bonobo.h>
#include <gnome.h>
#include <stdio.h>

#include <e-book.h>

static CORBA_Environment ev;

#ifdef USING_OAF

#include <liboaf/liboaf.h>

static void
init_corba (int *argc, char **argv)
{
	gnome_init_with_popt_table("blah", "0.0", *argc, argv, NULL, 0, NULL);

	oaf_init (*argc, argv);
}

#else

#include <libgnorba/gnorba.h>

static void
init_corba (int *argc, char **argv)
{
	gnome_CORBA_init_with_popt_table (
		"blah", "0.0",
		argc, argv, NULL, 0, NULL, GNORBA_INIT_SERVER_FUNC, &ev);
}

#endif

static void
init_bonobo (int argc, char **argv)
{
	if (bonobo_init (CORBA_OBJECT_NIL, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL) == FALSE)
		g_error (_("Could not initialize Bonobo"));
}

static void
add_card_cb (EBook *book, EBookStatus status, const gchar *id, gpointer closure)
{
	ECard *card = E_CARD(closure);
	char *vcard = e_card_get_vcard(card);
	g_print ("Saved card: %s\n", vcard);
	g_free(vcard);
	gtk_object_unref(GTK_OBJECT(card));
}

static void
book_open_cb (EBook *book, EBookStatus status, gpointer closure)
{
	GList *list = e_card_load_cards_from_file("gnomecard.vcf");
	GList *iterator;
	for (iterator = list; iterator; iterator = g_list_next(iterator)) {
		ECard *card = iterator->data;
		e_book_add_card(book, card, add_card_cb, card);
	}
	g_list_free(list);
}

static guint
ebook_create (void)
{
	EBook *book;
	gchar *path, *uri;
	
	book = e_book_new ();

	if (!book) {
		printf ("%s: %s(): Couldn't create EBook, bailing.\n",
			__FILE__,
			__FUNCTION__);
		return FALSE;
	}
	

	path = g_concat_dir_and_file (g_get_home_dir (),
				      "evolution/local/Contacts/addressbook.db");
	uri = g_strdup_printf ("file://%s", path);
	g_free (path);

	if (! e_book_load_uri (book, uri, book_open_cb, NULL)) {
		printf ("error calling load_uri!\n");
	}
	g_free(uri);


	return FALSE;
}

int
main (int argc, char **argv)
{

	CORBA_exception_init (&ev);

	init_corba (&argc, argv);
	init_bonobo (argc, argv);

	gtk_idle_add ((GtkFunction) ebook_create, NULL);
	
	bonobo_main ();

	return 0;
}
