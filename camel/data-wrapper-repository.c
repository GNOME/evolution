/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 *
 * Author : 
 *  Bertrand Guiheneuf <bertrand@helixcode.com>
 *
 * Copyright 1999, 2000 Helix Code, Inc. (http://www.helixcode.com)
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


#include "data-wrapper-repository.h"
#include "camel-simple-data-wrapper.h"
#include "camel-mime-message.h"
#include "camel-multipart.h"
#include <string.h>
#include "hash-table-utils.h"

static DataWrapperRepository _repository;
static int _initialized = -1;
GMimeContentField *_content_field;


/**
 * data_wrapper_repository_init: initialize data wrapper repository 
 * 
 * initialize the data wrapper repository. When the repository has
 * already been initialized, returns -1. 
 * 
 * Return value: 1 if correctly initialized returns -1.
 **/
gint 
data_wrapper_repository_init ()
{
	if (_initialized != -1) return -1;
	_repository.mime_links = g_hash_table_new (g_strcase_hash, g_strcase_equal);
	data_wrapper_repository_set_data_wrapper_type ("multipart", camel_multipart_get_type());

	/* this is a temporary default so that Michael can use get_stream on text messages  */
	data_wrapper_repository_set_data_wrapper_type ("text", camel_simple_data_wrapper_get_type());

	/* this is for matt the great lopper */
	data_wrapper_repository_set_data_wrapper_type ("message/rfc822", camel_mime_message_get_type());
	_content_field = gmime_content_field_new (NULL, NULL);
	_initialized = 1;
	return 1;
}

/**
 * data_wrapper_repository_set_data_wrapper_type: associate a data wrapper object type to a mime type
 * @mime_type: mime type
 * @object_type: object type
 * 
 * Associate an object type to a mime type. 
 **/
void
data_wrapper_repository_set_data_wrapper_type (const gchar *mime_type, GtkType object_type)
{
	gboolean already_exists;
	gchar *old_mime_type;
	GtkType old_gtk_type;
	
	already_exists = g_hash_table_lookup_extended (_repository.mime_links, (gpointer)mime_type,
						       (gpointer)&old_mime_type, (gpointer)&old_gtk_type);
	if (already_exists) 
		g_hash_table_insert (_repository.mime_links, (gpointer)old_mime_type, (gpointer)object_type);
	else
		g_hash_table_insert (_repository.mime_links, (gpointer)g_strdup (mime_type), (gpointer)object_type);
}



/**
 * data_wrapper_repository_get_data_wrapper_type: get the gtk type object associated to a mime type
 * @mime_type: mime type
 * 
 * returns the GtkType of the data wrapper object associated to 
 * a particular mime type. The mime type must be a character string
 * of the form "type/subtype" or simply "type". When the complete
 * mime type ("type/subtype") is not associated to any particular 
 * data wrapper object, this routine looks for a default data wrapper
 * for the main mime type ("type"). When no particular association is
 * found for this mime type, the type of the SimpleDataWrapper is 
 * returned. 
 * 
 * 
 * Return value: the associated data wrapper object type.
 **/
GtkType 
data_wrapper_repository_get_data_wrapper_type (const gchar *mime_type)
{
	gboolean exists;
	gchar *old_mime_type;
	GtkType gtk_type;

	printf("looking up type '%s'\n", mime_type);

	/* find if the complete mime type exists */
	exists = g_hash_table_lookup_extended (_repository.mime_links, (gpointer)mime_type,
						       (gpointer)&old_mime_type, (gpointer)&gtk_type);
	if (exists) { /* the complete mime type exists, return it */
		printf( "exists!\n");
		return gtk_type;
	} else { 
		/* the complete mime type association does not exists */
		/* is there an association for the main mime type ?   */
		gmime_content_field_construct_from_string (_content_field, mime_type);
		exists = g_hash_table_lookup_extended (_repository.mime_links, (gpointer)(_content_field->type),
						       (gpointer)&old_mime_type, (gpointer)&gtk_type);
		
		if (exists) /* the main mime type association exists */
			return gtk_type;
		else return camel_simple_data_wrapper_get_type();
	}

			
		

}

