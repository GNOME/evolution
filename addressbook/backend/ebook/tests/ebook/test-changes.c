/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#include "ebook/e-book.h"
#include <libgnome/gnome-init.h>
#include <bonobo/bonobo-main.h>
#include <stdlib.h>

#define NEW_VCARD "BEGIN:VCARD\n\
X-EVOLUTION-FILE-AS:Toshok, Chris\n\
FN:Chris Toshok\n\
EMAIL;INTERNET:toshok@ximian.com\n\
ORG:Ximian, Inc.;\n\
END:VCARD"

static char file_template[]="file:///tmp/change-test-XXXXXX";

int
main (int argc, char **argv)
{
	EBook *book;
	gboolean status;
	EContact *contact;
	GList *changes;
	GError *error = NULL;
	EBookChange *change;

	gnome_program_init("test-changes", "0.0", LIBGNOME_MODULE, argc, argv, NULL);

	if (bonobo_init (&argc, argv) == FALSE)
		g_error ("Could not initialize Bonobo");

	mktemp (file_template);

	/* create a temp addressbook in /tmp */
	book = e_book_new ();

	printf ("loading addressbook\n");
	if (!e_book_load_uri (book, file_template, FALSE, &error)) {
		printf ("failed to open addressbook: `%s': %s\n", file_template, error->message);
		exit(0);
	}

	/* get an initial change set */
	if (!e_book_get_changes (book, "changeidtest", &changes, &error)) {
		printf ("failed to get changes: %s\n", error->message);
		exit(0);
	}

	/* make a change to the book */
	contact = e_contact_new_from_vcard (NEW_VCARD);
	if (!e_book_add_contact (book, contact, &error)) {
		printf ("failed to add new contact: %s\n", error->message);
		exit(0);
	}

	/* get another change set */
	if (!e_book_get_changes (book, "changeidtest", &changes, &error)) {
		printf ("failed to get second set of changes: %s\n", error->message);
		exit(0);
	}

	/* make sure that 1 change has occurred */
	if (g_list_length (changes) != 1) {
		printf ("got back %d changes, was expecting 1\n", g_list_length (changes));
		exit(0);
	}

	change = changes->data;
	if (change->change_type != E_BOOK_CHANGE_CARD_ADDED) {
		printf ("was expecting a CARD_ADDED change, but didn't get it.\n");
		exit(0);
	}

	printf ("got changed vcard back: %s\n", change->vcard);

	e_book_free_change_list (changes);


	if (!e_book_remove (book, &error)) {
		printf ("failed to remove book; %s\n", error->message);
		exit(0);
	}

	g_object_unref (book);

	bonobo_main_quit();

	return 0;
}
