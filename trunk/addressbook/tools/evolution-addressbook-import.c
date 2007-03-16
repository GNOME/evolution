/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#include <config.h>

#include <bonobo-activation/bonobo-activation.h>
#include <bonobo/bonobo-main.h>
#include <backend/ebook/e-book-async.h>
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
		g_main_loop_quit (0);
}

static void
add_cb (EBook *book, EBookStatus status, const char *id, gpointer closure)
{
	switch (status) {
	case E_BOOK_ERROR_OK:
		unref_executable ();
		break;
	default:
		g_main_loop_quit (NULL);
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
		POPT_AUTOHELP
		{ NULL, '\0', 0, NULL, 0, NULL, NULL }
	};

	bindtextdomain (GETTEXT_PACKAGE, EVOLUTION_LOCALEDIR);
	textdomain (GETTEXT_PACKAGE);

	gnome_program_init ("evolution-addressbook-import", VERSION,
			    LIBGNOMEUI_MODULE, argc, argv, 
			    GNOME_PROGRAM_STANDARD_PROPERTIES,
			    GNOME_PARAM_POPT_TABLE, options,
			    NULL);

	if (filename == NULL) {
		g_error (_("No filename provided."));
	}

	e_book_async_get_default_addressbook (use_addressbook, filename);

	bonobo_main ();

	return 0;
}
