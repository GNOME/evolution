/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Gilbert Fang <gilbert.fang@sun.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <glib/gi18n.h>

#include <libebook/libebook.h>

#include "evolution-addressbook-export.h"

#ifdef G_OS_WIN32
#ifdef DATADIR
#undef DATADIR
#endif
#include <windows.h>
#include <conio.h>
#ifndef PROCESS_DEP_ENABLE
#define PROCESS_DEP_ENABLE 0x00000001
#endif
#ifndef PROCESS_DEP_DISABLE_ATL_THUNK_EMULATION
#define PROCESS_DEP_DISABLE_ATL_THUNK_EMULATION 0x00000002
#endif
#endif

/* Command-Line Options */
static gchar *opt_output_file = NULL;
static gboolean opt_list_folders_mode = FALSE;
static gchar *opt_output_format = NULL;
static gchar *opt_addressbook_source_uid = NULL;
static gchar **opt_remaining = NULL;

static GOptionEntry entries[] = {
	{ "output", '\0', 0,
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
	{ G_OPTION_REMAINING, '\0', 0,
	  G_OPTION_ARG_STRING_ARRAY, &opt_remaining },
	{ NULL }
};

gint
main (gint argc,
      gchar **argv)
{
	ActionContext actctx;
	GOptionContext *context;
	GError *error = NULL;

	gint current_action = ACTION_NOTHING;
	gint IsCSV = FALSE;
	gint IsVCard = FALSE;

#ifdef G_OS_WIN32
	/* Reduce risks */
	{
		typedef BOOL (WINAPI *t_SetDllDirectoryA) (LPCSTR lpPathName);
		t_SetDllDirectoryA p_SetDllDirectoryA;

		p_SetDllDirectoryA = GetProcAddress (GetModuleHandle ("kernel32.dll"), "SetDllDirectoryA");
		if (p_SetDllDirectoryA)
			(*p_SetDllDirectoryA) ("");
	}
#ifndef _WIN64
	{
		typedef BOOL (WINAPI *t_SetProcessDEPPolicy) (DWORD dwFlags);
		t_SetProcessDEPPolicy p_SetProcessDEPPolicy;

		p_SetProcessDEPPolicy = GetProcAddress (GetModuleHandle ("kernel32.dll"), "SetProcessDEPPolicy");
		if (p_SetProcessDEPPolicy)
			(*p_SetProcessDEPPolicy) (PROCESS_DEP_ENABLE | PROCESS_DEP_DISABLE_ATL_THUNK_EMULATION);
	}
#endif
#endif

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

	actctx.registry = e_source_registry_new_sync (NULL, &error);
	if (error != NULL) {
		g_printerr ("%s\n", error->message);
		g_error_free (error);
		exit (-1);
	}

	/* Parsing Parameter */
	if (opt_remaining && g_strv_length (opt_remaining) > 0)
		opt_addressbook_source_uid = g_strdup (opt_remaining[0]);

	if (opt_list_folders_mode != FALSE) {
		current_action = ACTION_LIST_FOLDERS;
		if (opt_addressbook_source_uid != NULL || opt_output_format != NULL) {
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
	}

	/* do actions */
	if (current_action == ACTION_LIST_FOLDERS) {
		actctx.action_type = current_action;
		if (opt_output_file == NULL) {
			actctx.output_file = NULL;
		} else {
			actctx.output_file = g_strdup (opt_output_file);
		}
		action_list_folders_init (&actctx);

	} else if (current_action == ACTION_LIST_CARDS) {
		actctx.action_type = current_action;
		if (opt_output_file == NULL) {
			actctx.output_file = NULL;
		} else {
			actctx.output_file = g_strdup (opt_output_file);
		}
		actctx.IsCSV = IsCSV;
		actctx.IsVCard = IsVCard;
		actctx.addressbook_source_uid =
			g_strdup (opt_addressbook_source_uid);

		action_list_cards_init (&actctx);

	} else {
		g_warning (_("Unhandled error"));
		exit (-1);
	}

	g_object_unref (actctx.registry);

	/*FIXME:should free actctx's some gchar * field, such as output_file! but since the program will end, so that will not cause mem leak.  */

	return 0;
}
