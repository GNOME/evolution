/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#include <config.h>

#include <bonobo-activation/bonobo-activation.h>
#include <bonobo/bonobo-main.h>

#include <backend/ebook/e-book-util.h>
#include <gnome.h>

static int cards_to_add_total = 1000;
static int cards_to_add = 50;
static int call_count = 0;

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

/* ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** */

static void
add_cb (EBook *book, EBookStatus status, const char *id, gpointer closure)
{
	switch (status) {
	case E_BOOK_STATUS_SUCCESS:
		--cards_to_add_total;
		g_message ("succesful add! (%d remaining)", cards_to_add_total);
		if (cards_to_add_total <= 0)
			g_main_loop_quit (NULL);
		break;
	default:
		g_message ("something went wrong...");
		g_main_loop_quit (NULL);
		break;
	}
}

static void
use_addressbook (EBook *book, EBookStatus status, gpointer closure)
{
	gint i;

	if (book == NULL || status != E_BOOK_STATUS_SUCCESS)
		g_error (_("Error loading default addressbook."));

	for (i = 0; i < cards_to_add; ++i) {
		gchar *vcard = make_random_vcard ();
		ECard *card = e_card_new (vcard);
		g_message ("adding %d", i);
		e_book_add_card (book, card, add_cb, NULL);
		g_free (vcard);
		g_object_unref (card);
	}

	g_object_unref (book);
}

static gint
abuse_timeout (gpointer foo)
{
	EBook *book = e_book_new ();
	e_book_load_default_book (book, use_addressbook, NULL);

	++call_count;
	g_message ("timeout!");
	return call_count < cards_to_add_total / cards_to_add;
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

	if (getenv ("ABUSE_THE_WOMBAT") == NULL) {
		g_print ("You probably don't want to use this program.\n"
			 "It isn't very nice.\n");
		exit(0);
	}

	bindtextdomain (PACKAGE, EVOLUTION_LOCALEDIR);
	textdomain (PACKAGE);

	gnome_program_init ("evolution-addressbook-abuse", VERSION,
			    LIBGNOMEUI_MODULE, argc, argv, 
			    GNOME_PROGRAM_STANDARD_PROPERTIES,
			    GNOME_PARAM_POPT_TABLE, options,
			    NULL);

	g_timeout_add (20, abuse_timeout, NULL);

	bonobo_main ();

	return 0;
}
