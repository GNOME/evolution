/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-addressbook-export.c
 *
 * Copyright (C) 2003 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Gilbert Fang <gilbert.fang@sun.com>
 *
 */

#include <config.h>

#include <glib.h>
#include <bonobo-activation/bonobo-activation.h>
#include <bonobo/bonobo-main.h>
#include <gnome.h>

#include <libebook/e-book.h>

#include "evolution-addressbook-export.h"

int
main (int argc, char **argv)
{
	ActionContext actctx;
	GnomeProgram *program;
	poptContext context;
	const gchar **argvn;

	int current_action = ACTION_NOTHING;
	int IsCSV = FALSE;
	int IsVCard = FALSE;

	/*** popttable */
	char *output_file = NULL;
	int list_folders_mode = FALSE;
	char *output_format = NULL;
	char *addressbook_folder_uri = NULL;
	int async_mode = FALSE;
	int file_size = 0;

	struct poptOption options[] = {
		{"output", '\0', POPT_ARG_STRING, &output_file, 0, N_("Specify the output file instead of standard output"),
		 N_("OUTPUTFILE")},
		{"list-addressbook-folders", 'l', POPT_ARG_NONE, &list_folders_mode, 0, N_("List local addressbook folders"),
		 NULL},
		{"format", '\0', POPT_ARG_STRING, &output_format, 0, N_("Show cards as vcard or csv file"), N_("[vcard|csv]")},
		{"async", 'a', POPT_ARG_NONE, &async_mode, 0, N_("Export in asynchronous mode "), NULL},
		{"size", '\0', POPT_ARG_INT, &file_size, 0,
		 N_("The number of cards in one output file in asychronous mode,default size 100."), N_("NUMBER")},
		{NULL, '\0', 0, NULL, 0, NULL, NULL}
	};
	/* popttable end ** */

	/*i18n-lize */
	bindtextdomain (GETTEXT_PACKAGE, EVOLUTION_LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	program =
		gnome_program_init (PACKAGE, VERSION, GNOME_BONOBO_MODULE, argc, argv, GNOME_PARAM_POPT_TABLE, options,
				    GNOME_PARAM_NONE);

	/* Parsing Parameter */
	g_object_get (program, "popt-context", &context, NULL);
	argvn = poptGetArgs (context);
	if (!argvn) {
		addressbook_folder_uri = NULL;
	} else { /* there at lease is a one argument, and that should be addressbook folder uri */
		addressbook_folder_uri = g_strdup (*argvn);
	}
        poptFreeContext (context);

	if (list_folders_mode != FALSE) {
		current_action = ACTION_LIST_FOLDERS;
		/* check there should not be addressbook-folder-uri , and async and size , output_format */
		if (addressbook_folder_uri != NULL || async_mode != FALSE || output_format != NULL || file_size != 0) {
			g_warning (_("Command line arguments error, please use --help option to see the usage."));
			exit (-1);
		}
	} else {

		current_action = ACTION_LIST_CARDS;

		/* check the output format */
		if (output_format == NULL) {
			IsVCard = TRUE;
		} else {
			IsCSV = !strcmp (output_format, "csv");
			IsVCard = !strcmp (output_format, "vcard");
			if (IsCSV == FALSE && IsVCard == FALSE) {
				g_warning (_("Only support csv or vcard format."));
				exit (-1);
			}
		}

		/*check async and output file */
		if (async_mode == TRUE) {
			/* check have to output file , set default file_size */
			if (output_file == NULL) {
				g_warning (_("In async mode, output must be file."));
				exit (-1);
			}
			if (file_size == 0)
				file_size = DEFAULT_SIZE_NUMBER;
		} else {
			/*check no file_size */
			if (file_size != 0) {
				g_warning (_("In normal mode, there should not need size option."));
				exit (-1);
			}
		}
	}

	/* do actions */
	if (current_action == ACTION_LIST_FOLDERS) {
		actctx.action_type = current_action;
		if (output_file == NULL) {
			actctx.action_list_folders.output_file = NULL;
		} else {
			actctx.action_list_folders.output_file = g_strdup (output_file);
		}
		action_list_folders_init (&actctx);
	} else if (current_action == ACTION_LIST_CARDS) {
		actctx.action_type = current_action;
		if (output_file == NULL) {
			actctx.action_list_cards.output_file = NULL;
		} else {
			actctx.action_list_cards.output_file = g_strdup (output_file);
		}
		actctx.action_list_cards.IsCSV = IsCSV;
		actctx.action_list_cards.IsVCard = IsVCard;
		actctx.action_list_cards.addressbook_folder_uri = g_strdup (addressbook_folder_uri);
		actctx.action_list_cards.async_mode = async_mode;
		actctx.action_list_cards.file_size = file_size;

		action_list_cards_init (&actctx);

	} else {
		g_warning (_("Impossible internal error."));
		exit (-1);
	}

	/*FIXME:should free actctx's some char* field, such as output_file! but since the program will end, so that will not cause mem leak.  */

	exit (0);
}
