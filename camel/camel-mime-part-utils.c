/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-mime-part-utils : Utility for mime parsing and so on */


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
#include <config.h>
#include "gmime-content-field.h"
#include "string-utils.h"
#include "camel-log.h"
#include "gmime-utils.h"
#include "camel-simple-data-wrapper.h"
#include "data-wrapper-repository.h" 
 
#include "camel-mime-part-utils.h"



/* declare this function because it is public
   but it must not be called except here */
void        camel_mime_part_set_content_type              (CamelMimePart *mime_part, 
							   gchar *content_type);


void
camel_mime_part_construct_headers_from_stream (CamelMimePart *mime_part, 
					       CamelStream *stream)
{
	GArray *header_array;
	Rfc822Header *cur_header;
	int i;

	CAMEL_LOG_FULL_DEBUG ("CamelMimePartUtils:: "
			      "Entering _construct_headers_from_stream\n");
	g_assert (stream);
	CAMEL_LOG_FULL_DEBUG ("CamelMimePartUtils::construct_headers_from_stream "
			      "parsing headers\n");
	/* 
	 * parse all header lines 
	 */
	header_array = get_header_array_from_stream (stream);
	if (header_array) {
		for (i=0; i<header_array->len; i++) {
			cur_header = (Rfc822Header *)header_array->data + i;
			camel_medium_add_header ( CAMEL_MEDIUM (mime_part), 
						  cur_header->name, 
						  cur_header->value);
			g_free (cur_header->name);
			g_free (cur_header->value);
		}

		g_array_free (header_array, TRUE);

		CAMEL_LOG_FULL_DEBUG ("CamelMimePartUtils::construct_headers_from_stream headers parsed. Leaving\n");
	}
}





void
camel_mime_part_construct_content_from_stream (CamelMimePart *mime_part, 
					       CamelStream *stream)
{
	GMimeContentField *content_type = NULL;
	gchar *mime_type = NULL;
	GtkType content_object_type;
	CamelDataWrapper *content_object = NULL;
	
	
	/* 
	 * find content mime type 
	 */
	CAMEL_LOG_FULL_DEBUG ("CamelMimePartUtils::construct_content_from_stream "
			      "parsing content\n");

	content_type = camel_mime_part_get_content_type (mime_part);
	/* here we should have a mime type */
	if (content_type)
		mime_type = gmime_content_field_get_mime_type (content_type);

	/* 
	 * no mime type found for the content, 
	 * using text/plain is the default 
	 */
	if (!mime_type) {
		CAMEL_LOG_FULL_DEBUG ("CamelMimePartUtils::construct_content_from_stream "
				      "content type field not found " 
				      "using default \"text/plain\"\n");
		mime_type = g_strdup ("text/plain");
		camel_mime_part_set_content_type (mime_part, mime_type);
	}
	
	/* 
	 * find in the repository what particular data wrapper is 
	 * associated to this mime type 
	 */
	content_object_type = 
		data_wrapper_repository_get_data_wrapper_type (mime_type);
	CAMEL_LOG_FULL_DEBUG ("CamelMimePartUtils::construct_content_from_stream content"
			      " type object type used: %s\n", 
			      gtk_type_name (content_object_type));

	g_free (mime_type);

	/* 
	 * create the content object data wrapper with the type 
	 * returned by the data wrapper repository 
	 */
	content_object = CAMEL_DATA_WRAPPER (gtk_type_new (content_object_type));
	camel_data_wrapper_set_mime_type_field (content_object, 
						camel_mime_part_get_content_type (mime_part));
	camel_medium_set_content_object ( CAMEL_MEDIUM (mime_part), content_object);

	/* set the input stream for the content object */
	camel_data_wrapper_set_input_stream (content_object, stream);

	/* 
	 * the object is referenced in the set_content_object method, 
	 * so unref it here 
	 */
	gtk_object_unref (GTK_OBJECT (content_object));
	
	
	CAMEL_LOG_FULL_DEBUG ("CamelMimePartUtils::construct_content_from_stream "
			      "content parsed\n");
		
	CAMEL_LOG_FULL_DEBUG ("CamelMimePartUtils:: Leaving _construct_content_from_stream\n");
}



void
camel_mime_part_store_stream_in_buffer (CamelMimePart *mime_part, 
					CamelStream *stream)
{
	gint nb_bytes_read_total = 0;
	gint nb_bytes_read_chunk;
	GByteArray *buffer;
#define STREAM_READ_CHUNK_SZ  100

	CAMEL_LOG_FULL_DEBUG ("CamelMimePartUtils::store_stream_in_buffer entering\n");

	if (mime_part->temp_message_buffer == NULL)
		mime_part->temp_message_buffer = g_byte_array_new ();
	
	buffer = mime_part->temp_message_buffer;

	g_byte_array_set_size (buffer, nb_bytes_read_total + STREAM_READ_CHUNK_SZ);
	nb_bytes_read_chunk = camel_stream_read (stream,
						 buffer->data + nb_bytes_read_total, 
						 STREAM_READ_CHUNK_SZ);

	if (nb_bytes_read_chunk>0) {
		nb_bytes_read_total += nb_bytes_read_chunk;
		
		while (nb_bytes_read_chunk >0) {
			g_byte_array_set_size (buffer, nb_bytes_read_total + STREAM_READ_CHUNK_SZ);
			nb_bytes_read_chunk = camel_stream_read (stream,
								 buffer->data + nb_bytes_read_total, 
								 STREAM_READ_CHUNK_SZ);
			nb_bytes_read_total += nb_bytes_read_chunk;
		}
	}

	g_byte_array_set_size (buffer, nb_bytes_read_total);
	CAMEL_LOG_FULL_DEBUG ("CamelMimePartUtils::store_stream_in_buffer entering\n");

}
