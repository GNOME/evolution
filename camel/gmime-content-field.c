/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* mime-content_field.c : mime content type field utilities  */

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




#include "gmime-content-field.h"
#include "gstring-util.h"

GMimeContentField *
gmime_content_field_new (GString *type, GString *subtype)
{
	GMimeContentField *ct;

	ct = g_new (GMimeContentField,1);
	ct->type = type;
	ct->subtype = subtype;
	ct->parameters =  g_hash_table_new(g_string_hash, g_string_equal_for_hash);
	
} 


void 
gmime_content_field_set_parameter(GMimeContentField *content_field, GString *attribute, GString *value)
{
	gboolean attribute_exists;
	GString *old_attribute;
	GString *old_value;

	attribute_exists = g_hash_table_lookup_extended (content_field->parameters, 
							 attribute, 
							 (gpointer *) &old_attribute,
							 (gpointer *) &old_value);
	if (attribute_exists) {
		g_string_assign(old_value, value->str);
		g_string_free (value, TRUE);
		g_string_free (attribute, TRUE);
	} else
		g_hash_table_insert (content_field->parameters, attribute, value);
}


static void
_print_parameter (gpointer name, gpointer value, gpointer user_data)
{
	FILE *file = (FILE *)user_data;
	
	fprintf (file, "; \n    %s=%s", ((GString *)name)->str, ((GString *)value)->str);
}

/**
 * gmime_content_field_write_to_file: write a mime content type to a file
 * @content_field: content type object
 * @file: file to write the content type field
 * 
 * 
 **/
void
gmime_content_field_write_to_file(GMimeContentField *content_field, FILE *file)
{
	g_assert(content_field);
	if ((content_field->type) && ((content_field->type)->str)) {
		fprintf (file, "Content-Type: %s", content_field->type->str);
		if ((content_field->subtype) && ((content_field->subtype)->str)) {
			fprintf (file, "/%s", content_field->subtype->str);
		}
		/* print all parameters */
		g_hash_table_foreach (content_field->parameters, _print_parameter, file);
		fprintf (file, "\n");
	}
}
