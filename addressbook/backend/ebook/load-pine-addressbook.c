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
	FILE *fp = fopen (".addressbook", "r");
	char line[1024];
	while(fgets(line, 1024, fp)) {
		int length = strlen(line);
		char **strings;
		ECardName *name;
		ECard *card;
		EList *list;
		if (line[length - 1] == '\n')
			line[--length] = 0;
		
		card = e_card_new("");
		strings = g_strsplit(line, "\t", 3);
		name = e_card_name_from_string(strings[1]);
		gtk_object_set(GTK_OBJECT(card),
			       "nickname", strings[0],
			       "full_name", strings[1],
			       "name", name,
			       NULL);
		gtk_object_get(GTK_OBJECT(card),
			       "email", &list,
			       NULL);
		e_list_append(list, strings[2]);
		g_strfreev(strings);
		e_book_add_card(book, card, add_card_cb, card);
	}
}

static guint
ebook_create (void)
{
	EBook *book;
	
	book = e_book_new ();

	if (!book) {
		printf ("%s: %s(): Couldn't create EBook, bailing.\n",
			__FILE__,
			__FUNCTION__);
		return FALSE;
	}
	

	if (! e_book_load_uri (book, "file:/tmp/test.db", book_open_cb, NULL)) {
		printf ("error calling load_uri!\n");
	}


	return FALSE;
}

#if 0
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
#endif

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
