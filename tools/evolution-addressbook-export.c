/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#include <config.h>

#include <liboaf/liboaf.h>
#include <bonobo/bonobo-main.h>
#include <backend/ebook/e-book-util.h>
#include <gnome.h>
#include <fcntl.h>
#include <gal/util/e-util.h>

static void
save_cards (EBook *book, EBookSimpleQueryStatus status, const GList *cards, gpointer closure)
{
	char *filename = closure;
	char *vcard;
	int result;
	/* This has to be an array so that it's not const. */
	char tmpname[] = "/tmp/evo-addressbook-tmp.XXXXXX";

	vcard = e_card_list_get_vcard (cards);

	if (filename)
		result = e_write_file (filename, vcard, O_CREAT | O_EXCL);
	else
		result = e_write_file_mkstemp (tmpname, vcard);
	printf (tmpname);
	sync();
	gtk_exit (result);
}

static void
use_addressbook (EBook *book, gpointer closure)
{
	if (book == NULL)
		g_error (_("Error loading default addressbook."));
	e_book_simple_query (book, "(contains \"x-evolution-any-field\" \"\")", save_cards, closure);
}

int
main (int argc, char *argv[])
{
	char *filename = NULL;

	struct poptOption options[] = {
		{ "output-file", '\0', POPT_ARG_STRING, &filename, 0, N_("Output File"), NULL },
		{ NULL, '\0', POPT_ARG_INCLUDE_TABLE, &oaf_popt_options, 0, NULL, NULL },
		POPT_AUTOHELP
		{ NULL, '\0', 0, NULL, 0, NULL, NULL }
	};

	bindtextdomain (PACKAGE, EVOLUTION_LOCALEDIR);
	textdomain (PACKAGE);

	gnome_init_with_popt_table ("evolution-addressbook-clean", "0.0",
				    argc, argv, options, 0, NULL);
	oaf_init (argc, argv);

	if (bonobo_init (CORBA_OBJECT_NIL, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL) == FALSE)
		g_error (_("Could not initialize Bonobo"));

	e_book_use_default_book (use_addressbook, filename);

	bonobo_main ();

	return 0;
}
