/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-object.c: Base class for Camel */

/*
 * Author:
 *  Dan Winship <danw@helixcode.com>
 *
 * Copyright 2000 Helix Code, Inc. (http://www.helixcode.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <config.h>
#include "camel-object.h"

static void
camel_object_init (gpointer object, gpointer klass)
{
	GTK_OBJECT_UNSET_FLAGS (object, GTK_FLOATING);
}

GtkType
camel_object_get_type (void)
{
	static GtkType camel_object_type = 0;

	if (!camel_object_type) {
		GtkTypeInfo camel_object_info =
		{
			"CamelObject",
			sizeof (CamelObject),
			sizeof (CamelObjectClass),
			(GtkClassInitFunc) NULL,
			(GtkObjectInitFunc) camel_object_init,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		camel_object_type = gtk_type_unique (gtk_object_get_type (), &camel_object_info);
	}

	return camel_object_type;
}
