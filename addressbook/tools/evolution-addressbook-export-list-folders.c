/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-addressbook-export-list-folders.c
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
#include <libbonobo.h>
#include <libgnome/libgnome.h>

#include <libebook/e-book.h>

#include "evolution-addressbook-export.h"

guint
action_list_folders_init (ActionContext * p_actctx)
{
	ESourceList *addressbooks = NULL;
	GSList *groups, *group;
	FILE *outputfile = NULL;

	if (!e_book_get_addressbooks (&addressbooks, NULL)) {
		g_warning (_("Couldn't get list of addressbooks"));
		exit (-1);
	}

	if (p_actctx->action_list_folders.output_file != NULL) {
		if (!(outputfile = fopen (p_actctx->action_list_folders.output_file, "w"))) {
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
			char *uri;
			const char *name;

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
