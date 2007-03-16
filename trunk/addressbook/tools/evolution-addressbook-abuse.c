/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#include <config.h>

#include <bonobo/bonobo-main.h>

#include <backend/ebook/e-book.h>
#include <gnome.h>

#define CONTACTS_TO_ADD 2000

static gchar *
make_random_string (void)
{
	const gchar *elements = " abcdefghijklmnopqrstuvwxyz1234567890  ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	gint len = strlen (elements);
	gint i, N = 5 + (random () % 10);
	gchar *str = g_malloc (N+1);

	for (i = 0; i < N; ++i) {
		str[i] = elements[random () % len];
	}
	str[i] = '\0';

	return str;
}

static gchar *
make_random_vcard (void)
{
	gchar *fa = make_random_string ();
	gchar *name = make_random_string ();
	gchar *email = make_random_string ();
	gchar *org = make_random_string ();

	gchar *vcard;

	vcard = g_strdup_printf ("BEGIN:VCARD\n"
				 "X-EVOLUTION-FILE-AS:%s\n"
				 "N:%s\n"
				 "EMAIL;INTERNET:%s\n"
				 "ORG:%s\n"
				 "END:VCARD",
				 fa, name, email, org);
	g_free (fa);
	g_free (name);
	g_free (email);
	g_free (org);

	return vcard;
}

int
main (int argc, char *argv[])
{
	EBook *book;
	int i;

	if (getenv ("ABUSE_THE_WOMBAT") == NULL) {
		g_print ("You probably don't want to use this program.\n"
			 "It isn't very nice.\n");
		exit(0);
	}

	bindtextdomain (GETTEXT_PACKAGE, EVOLUTION_LOCALEDIR);
	textdomain (GETTEXT_PACKAGE);

	gnome_program_init ("evolution-addressbook-abuse", VERSION,
			    LIBGNOMEUI_MODULE, argc, argv, 
			    GNOME_PROGRAM_STANDARD_PROPERTIES,
			    NULL);

	if (!e_book_get_default_addressbook (&book, NULL)) {
		g_warning ("couldn't open addressbook");
		exit (1);
	}

	for (i = 0; i < CONTACTS_TO_ADD; ++i) {
		gchar *vcard = make_random_vcard ();
		EContact *contact = e_contact_new_from_vcard (vcard);
		g_message ("adding %d", i);
		if (!e_book_add_contact (book, contact, NULL)) {
			g_warning ("something went wrong...");
			exit (1);
		}
		g_free (vcard);
		g_object_unref (contact);
	}

	return 0;
}
