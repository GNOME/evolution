/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-xml-utils.c
 * Copyright (C) 2000  Helix Code, Inc.
 * Author: Chris Lahey <clahey@helixcode.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <glib.h>
#include <gtk/gtkobject.h>

#include "e-util.h"

int
g_str_compare(const void *x, const void *y)
{
  return strcmp(x, y);
}

int
g_int_compare(const void *x, const void *y)
{
  if ( GPOINTER_TO_INT(x) < GPOINTER_TO_INT(y) )
    return -1;
  else if ( GPOINTER_TO_INT(x) == GPOINTER_TO_INT(y) )
    return 0;
  else
    return -1;
}

void
e_free_object_list (GList *list)
{
	GList *p;

	for (p = list; p != NULL; p = p->next)
		gtk_object_unref (GTK_OBJECT (p->data));

	g_list_free (list);
}
