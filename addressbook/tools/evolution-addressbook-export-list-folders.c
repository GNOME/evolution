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

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <libebook/e-book.h>

#include "evolution-addressbook-export.h"

guint
action_list_folders_init (ActionContext * p_actctx)
{
	ESourceList *addressbooks = NULL;
	GSList *groups, *group;
	FILE *outputfile = NULL;

	if (!e_book_get_addressbooks (&addressbooks, NULL)) {
		g_warning (_("Couldn't get list of address books"));
		exit (-1);
	}

	if (p_actctx->action_list_folders.output_file != NULL) {
		if (!(outputfile = g_fopen (p_actctx->action_list_folders.output_file, "w"))) {
			g_warning (_("Can not open file"));
			exit (-1);
		}
	}

	groups = e_source_list_peek_groups (addressbooks);
	for (group = groups; group; group = group->next) {
		ESourceGroup *g = group->data;
		GSList *sources, *source;

		sources = e_source_group_peek_sources (g);
		for (source = sources; source; source = source->next) {
			ESource *s = source->data;
			EBook *book;
			EBookQuery *query;
			GList *contacts;
			gchar *uri;
			const gchar *name;

			book = e_book_new (s, NULL);
			if (!book
			    || !e_book_open (book, TRUE, NULL)) {
				g_warning (_("failed to open book"));
				continue;
			}

			query = e_book_query_any_field_contains ("");
			e_book_get_contacts (book, query, &contacts, NULL);
			e_book_query_unref (query);

			uri = e_source_get_uri (s);
			name = e_source_peek_name (s);

			if (outputfile)
				fprintf (outputfile, "\"%s\",\"%s\",%d\n", uri, name, g_list_length (contacts));
			else
				printf ("\"%s\",\"%s\",%d\n", uri, name, g_list_length (contacts));

			g_free (uri);
			g_list_foreach (contacts, (GFunc)g_object_unref, NULL);
			g_list_free (contacts);

			g_object_unref (book);
		}
	}

	if (outputfile)
		fclose (outputfile);

	return SUCCESS;
}
