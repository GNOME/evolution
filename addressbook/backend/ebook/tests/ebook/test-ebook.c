/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#include "ebook/e-book.h"
#include <libgnome/gnome-init.h>
#include <bonobo/bonobo-main.h>
#include <stdlib.h>

static void
print_email (EContact *contact)
{
	char *file_as = e_contact_get (contact, E_CONTACT_FILE_AS);
	GList *emails, *e;

	printf ("Contact: %s\n", file_as);
	printf ("Email addresses:\n");
	emails = e_contact_get (contact, E_CONTACT_EMAIL);
	for (e = emails; e; e = e->next) {
		EVCardAttribute *attr = e->data;
		GList *values = e_vcard_attribute_get_values (attr);
		printf ("\t%s\n",  values && values->data ? (char*)values->data : "");
		e_vcard_attribute_free (attr);
	}
	g_list_free (emails);

	g_free (file_as);

	printf ("\n");
}

static void
print_all_emails (EBook *book)
{
	EBookQuery *query;
	gboolean status;
	GList *cards, *c;

	query = e_book_query_field_exists (E_CONTACT_FULL_NAME);

	status = e_book_get_contacts (book, query, &cards, NULL);

	e_book_query_unref (query);

	if (status == FALSE) {
		printf ("error %d getting card list\n", status);
		exit(0);
	}

	for (c = cards; c; c = c->next) {
		EContact *contact = E_CONTACT (c->data);

		print_email (contact);

		g_object_unref (contact);
	}
	g_list_free (cards);
}

static void
print_one_email (EBook *book)
{
	EContact *contact;
	GError *error = NULL;

	if (!e_book_get_contact (book, "pas-id-0002023", &contact, &error)) {
		printf ("error %d getting card: %s\n", error->code, error->message);
		g_clear_error (&error);
		return;
	}

	print_email (contact);

	g_object_unref (contact);
}

int
main (int argc, char **argv)
{
	EBook *book;
	gboolean status;

	gnome_program_init("test-ebook", "0.0", LIBGNOME_MODULE, argc, argv, NULL);

	if (bonobo_init (&argc, argv) == FALSE)
		g_error ("Could not initialize Bonobo");

	/*
	** the actual ebook foo
	*/

	book = e_book_new ();

	printf ("loading addressbook\n");
	status = e_book_load_local_addressbook (book, NULL);
	if (status == FALSE) {
		printf ("failed to open local addressbook\n");
		exit(0);
	}

	printf ("printing one contact\n");
	print_one_email (book);

	printf ("printing all contacts\n");
	print_all_emails (book);

	g_object_unref (book);

	bonobo_main_quit();

	return 0;
}
