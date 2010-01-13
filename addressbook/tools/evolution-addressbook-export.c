/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Gilbert Fang <gilbert.fang@sun.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>

#include <string.h>
#include <glib.h>
#include <glib/gi18n.h>

#include <libebook/e-book.h>

#include "evolution-addressbook-export.h"

/* Command-Line Options */
static gchar *opt_output_file = NULL;
static gboolean opt_list_folders_mode = FALSE;
static gchar *opt_output_format = NULL;
static gchar *opt_addressbook_folder_uri = NULL;
static gboolean opt_async_mode = FALSE;
static gint opt_file_size = 0;
static gchar **opt_remaining = NULL;

static GOptionEntry entries[] = {
	{ "output", '\0', G_OPTION_FLAG_FILENAME,
	  G_OPTION_ARG_STRING, &opt_output_file,
	  N_("Specify the output file instead of standard output"),
	  N_("OUTPUTFILE") },
	{ "list-addressbook-folders", 'l', 0,
	  G_OPTION_ARG_NONE, &opt_list_folders_mode,
	  N_("List local address book folders") },
	{ "format", '\0', 0,
	  G_OPTION_ARG_STRING, &opt_output_format,
	  N_("Show cards as vcard or csv file"),
	  N_("[vcard|csv]") },
	{ "async", 'a', 0,
	  G_OPTION_ARG_NONE, &opt_async_mode,
	  N_("Export in asynchronous mode") },
	{ "size", '\0', 0,
	  G_OPTION_ARG_INT, &opt_file_size,
	  N_("The number of cards in one output file in asynchronous mode, "
	     "default size 100."),
	  N_("NUMBER") },
	{ G_OPTION_REMAINING, '\0', 0,
	  G_OPTION_ARG_STRING_ARRAY, &opt_remaining },
	{ NULL }
};

gint
main (gint argc, gchar **argv)
{
	ActionContext actctx;
	GOptionContext *context;
	GError *error = NULL;

	gint current_action = ACTION_NOTHING;
	gint IsCSV = FALSE;
	gint IsVCard = FALSE;

	g_type_init ();

	/*i18n-lize */
	bindtextdomain (GETTEXT_PACKAGE, EVOLUTION_LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	context = g_option_context_new (NULL);
	g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
	if (!g_option_context_parse (context, &argc, &argv, &error)) {
		g_printerr ("%s\n", error->message);
		g_error_free (error);
		exit (-1);
	}

	/* Parsing Parameter */
	if (opt_remaining && g_strv_length (opt_remaining) > 0)
		opt_addressbook_folder_uri = g_strdup (opt_remaining[0]);

	if (opt_list_folders_mode != FALSE) {
		current_action = ACTION_LIST_FOLDERS;
		/* check there should not be addressbook-folder-uri , and async and size , output_format */
		if (opt_addressbook_folder_uri != NULL || opt_async_mode != FALSE || opt_output_format != NULL || opt_file_size != 0) {
			g_warning (_("Command line arguments error, please use --help option to see the usage."));
			exit (-1);
		}
	} else {

		current_action = ACTION_LIST_CARDS;

		/* check the output format */
		if (opt_output_format == NULL) {
			IsVCard = TRUE;
		} else {
			IsCSV = !strcmp (opt_output_format, "csv");
			IsVCard = !strcmp (opt_output_format, "vcard");
			if (IsCSV == FALSE && IsVCard == FALSE) {
				g_warning (_("Only support csv or vcard format."));
				exit (-1);
			}
		}

		/*check async and output file */
		if (opt_async_mode == TRUE) {
			/* check have to output file , set default file_size */
			if (opt_output_file == NULL) {
				g_warning (_("In async mode, output must be file."));
				exit (-1);
			}
			if (opt_file_size == 0)
				opt_file_size = DEFAULT_SIZE_NUMBER;
		} else {
			/*check no file_size */
			if (opt_file_size != 0) {
				g_warning (_("In normal mode, there is no need for the size option."));
				exit (-1);
			}
		}
	}

	/* do actions */
	if (current_action == ACTION_LIST_FOLDERS) {
		actctx.action_type = current_action;
		if (opt_output_file == NULL) {
			actctx.action_list_folders.output_file = NULL;
		} else {
			actctx.action_list_folders.output_file = g_strdup (opt_output_file);
		}
		action_list_folders_init (&actctx);
	} else if (current_action == ACTION_LIST_CARDS) {
		actctx.action_type = current_action;
		if (opt_output_file == NULL) {
			actctx.action_list_cards.output_file = NULL;
		} else {
			actctx.action_list_cards.output_file = g_strdup (opt_output_file);
		}
		actctx.action_list_cards.IsCSV = IsCSV;
		actctx.action_list_cards.IsVCard = IsVCard;
		actctx.action_list_cards.addressbook_folder_uri = g_strdup (opt_addressbook_folder_uri);
		actctx.action_list_cards.async_mode = opt_async_mode;
		actctx.action_list_cards.file_size = opt_file_size;

		action_list_cards_init (&actctx);

	} else {
		g_warning (_("Unhandled error"));
		exit (-1);
	}

	/*FIXME:should free actctx's some gchar * field, such as output_file! but since the program will end, so that will not cause mem leak.  */

	exit (0);
}
