/*
 *
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

#include <config.h>
#include <stdlib.h>

#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <libebook/libebook.h>

#include "evolution-addressbook-export.h"

void
action_list_folders_init (ActionContext *p_actctx)
{
	ESourceRegistry *registry;
	GList *list, *iter;
	FILE *outputfile = NULL;
	const gchar *extension_name;

	registry = p_actctx->registry;

	if (p_actctx->output_file != NULL) {
		if (!(outputfile = g_fopen (p_actctx->output_file, "w"))) {
			g_warning (_("Can not open file"));
			exit (-1);
		}
	}

	extension_name = E_SOURCE_EXTENSION_ADDRESS_BOOK;
	list = e_source_registry_list_sources (registry, extension_name);

	for (iter = list; iter != NULL; iter = g_list_next (iter)) {
		EClient *client;
		EBookClient *book_client;
		EBookQuery *query;
		ESource *source;
		GSList *contacts;
		const gchar *display_name;
		const gchar *uid;
		gchar *query_str;
		GError *error = NULL;

		source = E_SOURCE (iter->data);

		client = e_book_client_connect_sync (source, 30, NULL, &error);

		/* Sanity check. */
		g_warn_if_fail (
			((client != NULL) && (error == NULL)) ||
			((client == NULL) && (error != NULL)));

		if (error != NULL) {
			g_warning (
				_("Failed to open client '%s': %s"),
				e_source_get_display_name (source),
				error->message);
			g_error_free (error);
			continue;
		}

		book_client = E_BOOK_CLIENT (client);

		query = e_book_query_any_field_contains ("");
		query_str = e_book_query_to_string (query);
		e_book_query_unref (query);

		e_book_client_get_contacts_sync (
			book_client, query_str, &contacts, NULL, NULL);

		display_name = e_source_get_display_name (source);
		uid = e_source_get_uid (source);

		if (outputfile)
			fprintf (
				outputfile, "\"%s\",\"%s\",%d\n",
				uid, display_name, g_slist_length (contacts));
		else
			printf (
				"\"%s\",\"%s\",%d\n",
				uid, display_name, g_slist_length (contacts));

		g_slist_foreach (contacts, (GFunc) g_object_unref, NULL);
		g_slist_free (contacts);

		g_object_unref (book_client);
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	if (outputfile)
		fclose (outputfile);
}
