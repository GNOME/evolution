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
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "config.h"

#include <stdlib.h>
#include <gtk/gtk.h>
#include "e-contact-print.h"

gint
main (gint argc, gchar *argv[])
{
	GList *shown_fields = NULL;

	gtk_init (&argc, &argv);

	shown_fields = g_list_append (shown_fields, (gpointer) "First field");
	shown_fields = g_list_append (shown_fields, (gpointer) "Second field");
	shown_fields = g_list_append (shown_fields, (gpointer) "Third field");
	shown_fields = g_list_append (shown_fields, (gpointer) "Fourth field");

	/* does nothing */
	e_contact_print (NULL, NULL, NULL, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG);

	gtk_main ();

	/* Not reached. */
	return 0;
}
