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

#include <ebook/e-book.h>
#include <ebook/e-book-util.h>

#include "evolution-addressbook-export.h"

static void
action_list_folders_get_cursor_cb (EBook * book, EBookStatus status, ECardCursor * cursor, ActionContext * p_actctx)
{
	FILE *outputfile;
	long length;
	const char *uri;
	char *name;

	uri = e_book_get_default_book_uri ();
	length = e_card_cursor_get_length (cursor);

	/*Fix me *
	   can not get name, should be a bug of e-book.Anyway, should set a default name.
	 */
	/*name = e_book_get_name (book); */
	name = g_strdup (_("Contacts"));

	if (p_actctx->action_list_folders.output_file == NULL) {
		printf ("\"%s\",\"%s\",%d\n", uri, name, (int) length);
	} else {
		/*output to a file */
		if (!(outputfile = fopen (p_actctx->action_list_folders.output_file, "w"))) {
			g_warning (_("Can not open file"));
			exit (-1);
		}
		fprintf (outputfile, "\"%s\",\"%s\",%d\n", uri, name, (int) length);
		fclose (outputfile);
	}

	g_free (name);
	g_object_unref (G_OBJECT (book));
	bonobo_main_quit ();
}

static void
action_list_folders_open_cb (EBook * book, EBookStatus status, ActionContext * p_actctx)
{
	if (E_BOOK_STATUS_SUCCESS ==  status) {
		e_book_get_cursor (book, "(contains \"full_name\" \"\")", 
				   (EBookCursorCallback)action_list_folders_get_cursor_cb, p_actctx);
	} else {
		g_object_unref (G_OBJECT (book));
		g_warning (_("Can not load URI"));
		exit (-1);
	}
}

static guint
action_list_folders_run (ActionContext * p_actctx)
{
	EBook *book;
	book = e_book_new ();

	e_book_load_default_book (book, (EBookCallback)action_list_folders_open_cb, p_actctx);
	return SUCCESS;
}

guint
action_list_folders_init (ActionContext * p_actctx)
{
	g_idle_add ((GSourceFunc) action_list_folders_run, p_actctx);

	return SUCCESS;
}
