/*
 * compose.c: A commnand line tool to invoke the Evolution mail composer
 *
 * Author:
 *   Miguel de Icaza (miguel@ximian.com)
 *
 * (C) 2001 Ximian, Inc.
 */
#include <config.h>
#include <gnome.h>
#include <liboaf/liboaf.h>
#include <bonobo.h>
#include "composer/Composer.h"

static char *subject;
static char *cc;
static char *bcc;
static char *body;
static char *to = "";

const struct poptOption compose_popt_options [] = {
	{ "subject", 's', POPT_ARG_STRING,
	  &subject,  0,   N_("Subject for the mail message"), N_("SUBJECT") },
	{ "cc",      'c', POPT_ARG_STRING,
	  &cc,       0,   N_("List of people that will be Carbo Copied"), N_("CC-LIST") },
	{ "bcc",     'b', POPT_ARG_STRING,
	  &bcc,      0,   N_("List of people to Blind Carbon Copy this mail to"), N_("BCC-LIST") },
	{ "body",    0,   POPT_ARG_STRING,
	  &body,     0,   N_("Filename containing the body of the message"), N_("BODY-FILE") },
	{ NULL,      0, 0, NULL, 0 }
};

static void
error (const char *msg)
{
	GtkWidget *dialog;
	
	dialog = gnome_message_box_new (
		msg,
		GNOME_MESSAGE_BOX_ERROR,
		GNOME_STOCK_BUTTON_OK,
		NULL);
	
	gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
	exit (1);
	g_assert_not_reached ();
}

GNOME_Evolution_Composer_RecipientList *
make_list (char *str)
{
	GNOME_Evolution_Composer_RecipientList *list;
	char *p;
	int count = 0;

	if (str == NULL)
		str = "";
	
	list = GNOME_Evolution_Composer_RecipientList__alloc();

	if (*str)
		count = 1;

	for (p = str; *p; p++){
		if (*p == ',')
			count++;
	}
	list->_maximum = count;
	list->_length = count;
	list->_buffer = CORBA_sequence_GNOME_Evolution_Composer_Recipient_allocbuf (count);
	
	for (count = 0; (p = strtok (str, ",")) != NULL; count++){
		GNOME_Evolution_Composer_Recipient *x;

		x = GNOME_Evolution_Composer_Recipient__alloc ();

		list->_buffer [count].name = CORBA_string_dup ("");
		list->_buffer [count].address = CORBA_string_dup (p);
		count++;
		str = NULL;
	}

	return list;
}

gint
do_launch (void)
{
	GNOME_Evolution_Composer_RecipientList *to_list, *cc_list, *bcc_list;
	GNOME_Evolution_Composer composer;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	composer = bonobo_get_object (
		"OAFIID:GNOME_Evolution_Mail_Composer",
		"GNOME/Evolution/Composer", &ev);
	CORBA_exception_free (&ev);

	if (composer == CORBA_OBJECT_NIL)
		error (_("It was not possible to start up the Evolution Mail Composer"));

	to_list = make_list (to);
	cc_list = make_list (cc);
	bcc_list = make_list (bcc);

	if (subject == NULL)
		subject = "";
	
	GNOME_Evolution_Composer_setHeaders (composer, to_list, cc_list, bcc_list, subject, &ev);
	GNOME_Evolution_Composer_show (composer, &ev);

	return FALSE;
}

int 
main (int argc, char *argv [])
{
	poptContext ctxt = NULL;
	CORBA_ORB orb;
	
	gnomelib_register_popt_table (oaf_popt_options, _("Oaf options"));
	gnome_init_with_popt_table ("Compose", "1.0", argc, argv,
				    compose_popt_options, 0, &ctxt);

	orb = oaf_init (argc, argv);
	if (bonobo_init (NULL, NULL, NULL) == FALSE)
		error (_("It was not possible to initialize the Bonobo component system"));

	if (ctxt){
		const char **to_args = NULL;
		GString *to_str = g_string_new ("");
		int i;
		
		to_args = poptGetArgs (ctxt);

		if (to_args){
			for (i = 0; to_args [i]; i++) {
				if (i > 1)
					g_string_append_c (to_str, ',');
				
				g_string_append (to_str, to_args [i]);
			}
		}
		to = to_str->str;
	}

	gtk_idle_add (GTK_SIGNAL_FUNC (do_launch), NULL);
	
	bonobo_main ();
	
	return 0;
}
