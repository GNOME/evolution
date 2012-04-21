/*
 *
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

#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <libebook/e-book-client.h>
#include <libebook/e-book-query.h>

#include "evolution-addressbook-export.h"

guint
action_list_folders_init (ActionContext *p_actctx)
{
	ESourceList *addressbooks = NULL;
	GSList *groups, *group;
	FILE *outputfile = NULL;
	GError *error = NULL;
	EBookQuery *query;
	gchar *query_str;

	if (!e_book_client_get_sources (&addressbooks, &error)) {
		g_warning (_("Couldn't get list of address books: %s"), error ? error->message : _("Unknown error"));
		if (error)
			g_error_free (error);
		exit (-1);
	}

	if (p_actctx->action_list_folders.output_file != NULL) {
		if (!(outputfile = g_fopen (p_actctx->action_list_folders.output_file, "w"))) {
			g_warning (_("Can not open file"));
			exit (-1);
		}
	}

	query = e_book_query_any_field_contains ("");
	query_str = e_book_query_to_string (query);
	e_book_query_unref (query);

	groups = e_source_list_peek_groups (addressbooks);
	for (group = groups; group; group = group->next) {
		ESourceGroup *g = group->data;
		GSList *sources, *source;

		sources = e_source_group_peek_sources (g);
		for (source = sources; source; source = source->next) {
			ESource *s = source->data;
			EBookClient *book_client;
			GSList *contacts;
			gchar *uri;
			const gchar *name;

			error = NULL;
			book_client = e_book_client_new (s, &error);
			if (!book_client
			    || !e_client_open_sync (E_CLIENT (book_client), TRUE, NULL, &error)) {
				g_warning (_("Failed to open client '%s': %s"), e_source_get_display_name (s), error ? error->message : _("Unknown error"));
				if (error)
					g_error_free (error);
				continue;
			}

			if (!e_book_client_get_contacts_sync (book_client, query_str, &contacts, NULL, &error))
				contacts = NULL;

			uri = e_source_get_uri (s);
			name = e_source_get_display_name (s);

			if (outputfile)
				fprintf (
					outputfile, "\"%s\",\"%s\",%d\n",
					uri, name, g_slist_length (contacts));
			else
				printf ("\"%s\",\"%s\",%d\n", uri, name, g_slist_length (contacts));

			g_free (uri);
			g_slist_foreach (contacts, (GFunc) g_object_unref, NULL);
			g_slist_free (contacts);

			g_object_unref (book_client);
		}
	}

	g_free (query_str);

	if (outputfile)
		fclose (outputfile);

	return SUCCESS;
}
