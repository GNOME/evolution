/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#include <config.h>

#include <bonobo-activation/bonobo-activation.h>
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
	g_main_loop_quit (NULL);
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
		POPT_AUTOHELP
		{ NULL, '\0', 0, NULL, 0, NULL, NULL }
	};

	bindtextdomain (GETTEXT_PACKAGE, EVOLUTION_LOCALEDIR);
	textdomain (GETTEXT_PACKAGE);

	gnome_program_init ("evolution-addressbook-export", VERSION,
			    LIBGNOMEUI_MODULE, argc, argv, 
			    GNOME_PROGRAM_STANDARD_PROPERTIES,
			    GNOME_PARAM_POPT_TABLE, options,
			    NULL);

	e_book_use_default_book (use_addressbook, filename);

	bonobo_main ();

	return 0;
}
