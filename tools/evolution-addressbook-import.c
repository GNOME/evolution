/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#include <config.h>

#include <liboaf/liboaf.h>
#include <bonobo/bonobo-main.h>
#include <backend/ebook/e-book-util.h>
#include <gnome.h>

static int exec_ref_count = 0;

static void
ref_executable (void)
{
	exec_ref_count ++;
}

static void
unref_executable (void)
{
	exec_ref_count --;
	if (exec_ref_count == 0)
		gtk_exit (0);
}

static void
add_cb (EBook *book, EBookStatus status, const char *id, gpointer closure)
{
	switch (status) {
	case E_BOOK_STATUS_SUCCESS:
		unref_executable ();
		break;
	default:
		gtk_exit (status);
		break;
	}
}

static void
use_addressbook (EBook *book, gpointer closure)
{
	GList *cards, *list;
	char *filename = closure;

	if (book == NULL)
		g_error (_("Error loading default addressbook."));

	cards = e_card_load_cards_from_file (filename);

	ref_executable ();

	for (list = cards; list; list = list->next) {
		ref_executable ();
		e_book_add_card (book, list->data, add_cb, closure);
	}
	sync();

	unref_executable ();
}

int
main (int argc, char *argv[])
{
	char *filename = NULL;

	struct poptOption options[] = {
		{ "input-file", '\0', POPT_ARG_STRING, &filename, 0, N_("Input File"), NULL },
		{ NULL, '\0', POPT_ARG_INCLUDE_TABLE, &oaf_popt_options, 0, NULL, NULL },
		POPT_AUTOHELP
		{ NULL, '\0', 0, NULL, 0, NULL, NULL }
	};

	bindtextdomain (PACKAGE, EVOLUTION_LOCALEDIR);
	textdomain (PACKAGE);

	gnome_init_with_popt_table ("evolution-addressbook-clean", "0.0",
				    argc, argv, options, 0, NULL);

	if (filename == NULL) {
		g_error (_("No filename provided."));
	}

	oaf_init (argc, argv);

	if (bonobo_init (CORBA_OBJECT_NIL, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL) == FALSE)
		g_error (_("Could not initialize Bonobo"));

	e_book_use_default_book (use_addressbook, filename);

	bonobo_main ();

	return 0;
}
