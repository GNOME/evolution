/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camelMimePart.c : Abstract class for a mime_part */

/** THIS IS MOSTLY AN ABSTRACT CLASS THAT SHOULD HAVE BEEN AN
    INTERFACE. **/

/* 
 *
 * Copyright (C) 1999 Bertrand Guiheneuf <Bertrand.Guiheneuf@inria.fr> .
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

#include "camel-mime-part.h"

static CamelDataWrapperClass *parent_class=NULL;

/* Returns the class for a CamelMimePart */
#define CMP_CLASS(so) CAMEL_MIME_PART_CLASS (GTK_OBJECT(so)->klass)

static void
camel_mime_part_class_init (CamelMimePartClass *camel_mime_part_class)
{
	parent_class = gtk_type_class (camel_data_wrapper_get_type ());
	
	/* virtual method definition */

	/* virtual method overload */
}







GtkType
camel_mime_part_get_type (void)
{
	static GtkType camel_mime_part_type = 0;
	
	if (!camel_mime_part_type)	{
		GtkTypeInfo camel_mime_part_info =	
		{
			"CamelMimePart",
			sizeof (CamelMimePart),
			sizeof (CamelMimePartClass),
			(GtkClassInitFunc) camel_mime_part_class_init,
			(GtkObjectInitFunc) NULL,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		camel_mime_part_type = gtk_type_unique (camel_data_wrapper_get_type (), &camel_mime_part_info);
	}
	
	return camel_mime_part_type;
}




static void
__camel_mime_part_add_header (CamelMimePart *mime_part, GString *header_name, GString *header_value)
{
	gboolean header_exists;
	GString *old_header_name;
	GString *old_header_value;

	header_exists = g_hash_table_lookup_extended (mime_part->headers, header_name, 
						      (gpointer *) &old_header_name,
						      (gpointer *) &old_header_value);
	if (header_exists) {
		g_string_free (old_header_name, TRUE);
		g_string_free (old_header_value, TRUE);
	}
	
	g_hash_table_insert (mime_part->headers, header_name, header_value);
}
