/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camelMimePart.c : Abstract class for a mime_part */


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
#include <stdio.h>
#include "gmime-content-field.h"
#include "gstring-util.h"
#include "camel-log.h"


typedef enum {
	HEADER_UNKNOWN,
	HEADER_DESCRIPTION,
	HEADER_DISPOSITION,
	HEADER_CONTENT_ID,
	HEADER_ENCODING,
	HEADER_CONTENT_MD5,
	HEADER_CONTENT_LANGUAGES,
	HEADER_CONTENT_TYPE
} CamelHeaderType;


static GHashTable *header_name_table;


static CamelDataWrapperClass *parent_class=NULL;

/* Returns the class for a CamelMimePart */
#define CMP_CLASS(so) CAMEL_MIME_PART_CLASS (GTK_OBJECT(so)->klass)

static void _add_header (CamelMimePart *mime_part, GString *header_name, GString *header_value);
static void _remove_header (CamelMimePart *mime_part, GString *header_name);
static GString *_get_header (CamelMimePart *mime_part, GString *header_name);
static void _set_description (CamelMimePart *mime_part, GString *description);
static GString *_get_description (CamelMimePart *mime_part);
static void _set_disposition (CamelMimePart *mime_part, GString *disposition);
static GString *_get_disposition (CamelMimePart *mime_part);
static void _set_filename (CamelMimePart *mime_part, GString *filename);
static GString *_get_filename (CamelMimePart *mime_part);
static void _set_content_id (CamelMimePart *mime_part, GString *content_id);
static GString *_get_content_id (CamelMimePart *mime_part);
static void _set_content_MD5 (CamelMimePart *mime_part, GString *content_MD5);
static GString *_get_content_MD5 (CamelMimePart *mime_part);
static void _set_encoding (CamelMimePart *mime_part, GString *encoding);
static GString *_get_encoding (CamelMimePart *mime_part);
static void _set_content_languages (CamelMimePart *mime_part, GList *content_languages);
static GList *_get_content_languages (CamelMimePart *mime_part);
static void _set_header_lines (CamelMimePart *mime_part, GList *header_lines);
static GList *_get_header_lines (CamelMimePart *mime_part);

static CamelDataWrapper *_get_content_object(CamelMimePart *mime_part);
static void _write_to_stream (CamelDataWrapper *data_wrapper, CamelStream *stream);

static gboolean _parse_header_pair (CamelMimePart *mime_part, GString *header_name, GString *header_value);


/* loads in a hash table the set of header names we */
/* recognize and associate them with a unique enum  */
/* identifier (see CamelHeaderType above)           */
static void
_init_header_name_table()
{
	header_name_table = g_hash_table_new (g_string_hash, g_string_equal_for_hash);
	g_hash_table_insert (header_name_table, g_string_new ("Content-Description"), (gpointer)HEADER_DESCRIPTION);
	g_hash_table_insert (header_name_table, g_string_new ("Content-Disposition"), (gpointer)HEADER_DISPOSITION);
	g_hash_table_insert (header_name_table, g_string_new ("Content-id"), (gpointer)HEADER_CONTENT_ID);
	g_hash_table_insert (header_name_table, g_string_new ("Content-Transfer-Encoding"), (gpointer)HEADER_ENCODING);
	g_hash_table_insert (header_name_table, g_string_new ("Content-MD5"), (gpointer)HEADER_CONTENT_MD5);
	g_hash_table_insert (header_name_table, g_string_new ("Content-Type"), (gpointer)HEADER_CONTENT_TYPE);

}

static void
camel_mime_part_class_init (CamelMimePartClass *camel_mime_part_class)
{
	CamelDataWrapperClass *camel_data_wrapper_class = CAMEL_DATA_WRAPPER_CLASS (camel_mime_part_class);
	parent_class = gtk_type_class (camel_data_wrapper_get_type ());
	_init_header_name_table();
	
	/* virtual method definition */
	camel_mime_part_class->add_header=_add_header;
	camel_mime_part_class->remove_header=_remove_header;
	camel_mime_part_class->get_header=_get_header;
	camel_mime_part_class->set_description=_set_description;
	camel_mime_part_class->get_description=_get_description;
	camel_mime_part_class->set_disposition=_set_disposition;
	camel_mime_part_class->get_disposition=_get_disposition;
	camel_mime_part_class->set_filename=_set_filename;
	camel_mime_part_class->get_filename=_get_filename;
	camel_mime_part_class->set_content_id=_set_content_id;
	camel_mime_part_class->get_content_id=_get_content_id;
	camel_mime_part_class->set_content_MD5=_set_content_MD5;
	camel_mime_part_class->get_content_MD5=_get_content_MD5;
	camel_mime_part_class->set_encoding=_set_encoding;
	camel_mime_part_class->get_encoding=_get_encoding;
	camel_mime_part_class->set_content_languages=_set_content_languages;
	camel_mime_part_class->get_content_languages=_get_content_languages;
	camel_mime_part_class->set_header_lines=_set_header_lines;
	camel_mime_part_class->get_header_lines=_get_header_lines;
	camel_mime_part_class->parse_header_pair = _parse_header_pair;
	camel_mime_part_class->get_content_object = _get_content_object;

	
	
	/* virtual method overload */
	camel_data_wrapper_class->write_to_stream = _write_to_stream;
}

static void
camel_mime_part_init (gpointer   object,  gpointer   klass)
{
	CamelMimePart *camel_mime_part = CAMEL_MIME_PART (object);

	camel_mime_part->headers =  g_hash_table_new (g_string_hash, g_string_equal_for_hash);

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
			(GtkObjectInitFunc) camel_mime_part_init,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		camel_mime_part_type = gtk_type_unique (camel_data_wrapper_get_type (), &camel_mime_part_info);
	}
	
	return camel_mime_part_type;
}




static void
_add_header (CamelMimePart *mime_part, GString *header_name, GString *header_value)
{
	gboolean header_exists;
	GString *old_header_name;
	GString *old_header_value;

	/* Try to parse the header pair. If it corresponds to something   */
	/* known, the job is done in the parsing routine. If not,         */
	/* we simply add the header in a raw fashion                      */
	if (CMP_CLASS(mime_part)->parse_header_pair (mime_part, header_name, header_value)) 
		return;
	header_exists = g_hash_table_lookup_extended (mime_part->headers, header_name, 
						      (gpointer *) &old_header_name,
						      (gpointer *) &old_header_value);
	if (header_exists) {
		g_string_free (old_header_name, TRUE);
		g_string_free (old_header_value, TRUE);
	}
	
	g_hash_table_insert (mime_part->headers, header_name, header_value);
}


void
camel_mime_part_add_header (CamelMimePart *mime_part, GString *header_name, GString *header_value)
{
	CMP_CLASS(mime_part)->add_header(mime_part, header_name, header_value);
}



static void
_remove_header (CamelMimePart *mime_part, GString *header_name)
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
	
	g_hash_table_remove (mime_part->headers, header_name);
	
}

void
camel_mime_part_remove_header (CamelMimePart *mime_part, GString *header_name)
{
	CMP_CLASS(mime_part)->remove_header(mime_part, header_name);
}



static GString *
_get_header (CamelMimePart *mime_part, GString *header_name)
{
	
	GString *old_header_name;
	GString *old_header_value;
	GString *header_value;

	header_value = (GString *)g_hash_table_lookup (mime_part->headers, header_name);
	return header_value;
}

GString *
camel_mime_part_get_header (CamelMimePart *mime_part, GString *header_name)
{
	return CMP_CLASS(mime_part)->get_header (mime_part, header_name);
}



static void
_set_description (CamelMimePart *mime_part, GString *description)
{
	if (mime_part->description) g_free(mime_part->description);
	mime_part->description = description;
}

void
camel_mime_part_set_description (CamelMimePart *mime_part, GString *description)
{
	CMP_CLASS(mime_part)->set_description (mime_part, description);
}




static GString *
_get_description (CamelMimePart *mime_part)
{
	return mime_part->description;
}

GString *
camel_mime_part_get_description (CamelMimePart *mime_part)
{
	return CMP_CLASS(mime_part)->get_description (mime_part);
}



static void
_set_disposition (CamelMimePart *mime_part, GString *disposition)
{
	//if (mime_part->disposition) g_free(mime_part->disposition);
	if (!mime_part->disposition) 
		mime_part->disposition = g_new (GMimeContentField,1);
	if ((mime_part->disposition)->type) g_free ((mime_part->disposition)->type);
	(mime_part->disposition)->type = disposition;
}


void
camel_mime_part_set_disposition (CamelMimePart *mime_part, GString *disposition)
{
	CMP_CLASS(mime_part)->set_disposition (mime_part, disposition);
}



static GString *
_get_disposition (CamelMimePart *mime_part)
{
	if (!mime_part->disposition) return NULL;
	return (mime_part->disposition)->type;
}


GString *
camel_mime_part_get_disposition (CamelMimePart *mime_part)
{
	return CMP_CLASS(mime_part)->get_disposition (mime_part);
}



static void
_set_filename (CamelMimePart *mime_part, GString *filename)
{
	if (mime_part->filename) g_free(mime_part->filename);
	mime_part->filename = filename;
}


void
camel_mime_part_set_filename (CamelMimePart *mime_part, GString *filename)
{
	CMP_CLASS(mime_part)->set_filename (mime_part, filename);
}



static GString *
_get_filename (CamelMimePart *mime_part)
{
	return mime_part->filename;
}


GString *
camel_mime_part_get_filename (CamelMimePart *mime_part)
{
	return CMP_CLASS(mime_part)->get_filename (mime_part);
}


/* this routine must not be public */
static void
_set_content_id (CamelMimePart *mime_part, GString *content_id)
{
	if (mime_part->content_id) g_free(mime_part->content_id);
	mime_part->content_id = content_id;
}


static GString *
_get_content_id (CamelMimePart *mime_part)
{
	return mime_part->content_id;
}


GString *
camel_mime_part_get_content_id (CamelMimePart *mime_part)
{
	return CMP_CLASS(mime_part)->get_content_id (mime_part);
}


/* this routine must not be public */
static void
_set_content_MD5 (CamelMimePart *mime_part, GString *content_MD5)
{
	if (mime_part->content_MD5) g_free(mime_part->content_MD5);
	mime_part->content_MD5 = content_MD5;
}


static GString *
_get_content_MD5 (CamelMimePart *mime_part)
{
	return mime_part->content_MD5;
}

GString *
camel_mime_part_get_content_MD5 (CamelMimePart *mime_part)
{
	return CMP_CLASS(mime_part)->get_content_MD5 (mime_part);
}



static void
_set_encoding (CamelMimePart *mime_part, GString *encoding)
{
	if (mime_part->encoding) g_free(mime_part->encoding);
	mime_part->encoding = encoding;
}

void
camel_mime_part_set_encoding (CamelMimePart *mime_part, GString *encoding)
{
	CMP_CLASS(mime_part)->set_encoding (mime_part, encoding);
}



static GString *
_get_encoding (CamelMimePart *mime_part)
{
	return mime_part->encoding;
}

GString *
camel_mime_part_get_encoding (CamelMimePart *mime_part)
{
	return CMP_CLASS(mime_part)->get_encoding (mime_part);
}




static void
_set_content_languages (CamelMimePart *mime_part, GList *content_languages)
{
	if (mime_part->content_languages) g_string_list_free(mime_part->content_languages);
	mime_part->content_languages = content_languages;
}

void
camel_mime_part_set_content_languages (CamelMimePart *mime_part, GList *content_languages)
{
	CMP_CLASS(mime_part)->set_content_languages (mime_part, content_languages);
}



static GList *
_get_content_languages (CamelMimePart *mime_part)
{
	return mime_part->content_languages;
}


GList *
camel_mime_part_get_content_languages (CamelMimePart *mime_part)
{
	return CMP_CLASS(mime_part)->get_content_languages (mime_part);
}



static void
_set_header_lines (CamelMimePart *mime_part, GList *header_lines)
{
	if (mime_part->header_lines) g_string_list_free(mime_part->header_lines);
	mime_part->header_lines = header_lines;
}

void
camel_mime_part_set_header_lines (CamelMimePart *mime_part, GList *header_lines)
{
	CMP_CLASS(mime_part)->set_header_lines (mime_part, header_lines);
}



static GList *
_get_header_lines (CamelMimePart *mime_part)
{
	return mime_part->header_lines;
}



GList *
camel_mime_part_get_header_lines (CamelMimePart *mime_part)
{
	return CMP_CLASS(mime_part)->get_header_lines (mime_part);
}




static CamelDataWrapper *
_get_content_object(CamelMimePart *mime_part)
{
	return mime_part->content;

}
CamelDataWrapper *
camel_mime_part_get_content_object(CamelMimePart *mime_part)
{
	return CMP_CLASS(mime_part)->get_content_object (mime_part);
}




/**********************************************************************/
#ifdef WHPT
#warning : WHPT is already defined !!!!!!
#endif
#define WHPT gmime_write_header_pair_to_stream


/* This is not used for the moment */
static void
_write_content_to_stream (CamelMimePart *mime_part, CamelStream *stream)
{
	guint buffer_size;
	gchar *buffer;
	gchar *encoded_buffer;

	CamelDataWrapper *content = mime_part->content;
	//	buffer_size = camel_data_wrapper_size (content);
	buffer = g_malloc (buffer_size);
	camel_data_wrapper_write_to_stream (content, stream);
	
	if (mime_part->encoding) {
		// encoded_buffer_size = gmime_encoded_size(buffer, buffer_size, encoding);
		// encoded_buffer = g_malloc (encoded_buffer_size);
		// gmime_encode_buffer (buffer, encoded_buffer, encoding);
		// camel_stream_write (stream, encoded_buffer, encoded_buffer_size);
		// g_free (encoded_buffer);
	} else 
		//fwrite (buffer, buffer_size, 1, file);
		camel_stream_write (stream, buffer, buffer_size);
	g_free (buffer);
}





static void
_write_to_stream (CamelDataWrapper *data_wrapper, CamelStream *stream)
{
	CamelMimePart *mp = CAMEL_MIME_PART (data_wrapper);
	
	CAMEL_LOG (FULL_DEBUG, "Entering CamelMimePart::write_to_stream\n");

	CAMEL_LOG (FULL_DEBUG, "CamelMimePart::write_to_stream writing content-disposition\n");
	gmime_content_field_write_to_stream(mp->disposition, stream);
	CAMEL_LOG (FULL_DEBUG, "CamelMimePart::write_to_stream writing content-transfer-encoding\n");
	WHPT (stream, "Content-Transfer-Encoding", mp->encoding);
	CAMEL_LOG (FULL_DEBUG, "CamelMimePart::write_to_stream writing content-description\n");
	WHPT (stream, "Content-Description", mp->description);
	CAMEL_LOG (FULL_DEBUG, "CamelMimePart::write_to_stream writing content-MD5\n");
	WHPT (stream, "Content-MD5", mp->content_MD5);
	CAMEL_LOG (FULL_DEBUG, "CamelMimePart::write_to_stream writing content-id\n");
	WHPT (stream, "Content-id", mp->content_id);
	CAMEL_LOG (FULL_DEBUG, "CamelMimePart::write_to_stream writing content-languages\n");
	write_header_with_glist_to_stream (stream, "Content-Language", mp->content_languages,", ");

	CAMEL_LOG (FULL_DEBUG, "CamelMimePart::write_to_stream writing other headers\n");
	write_header_table_to_stream (stream, mp->headers);

	CAMEL_LOG (FULL_DEBUG, "CamelMimePart::write_to_stream writing content-type\n");
	gmime_content_field_write_to_stream(data_wrapper->content_type, stream);

	camel_stream_write_string(stream,"\n");
	if (mp->content) camel_data_wrapper_write_to_stream (mp->content, stream);
	
}



/*******************************/
/* mime part parsing           */

static gboolean
_parse_header_pair (CamelMimePart *mime_part, GString *header_name, GString *header_value)
{
	CamelHeaderType header_type;
	gboolean header_handled = FALSE;


	header_type = (CamelHeaderType) g_hash_table_lookup (header_name_table, header_name);
	switch (header_type) {
	
	case HEADER_DESCRIPTION:
		CAMEL_LOG (FULL_DEBUG,
			   "CamelMimePart::parse_header_pair found HEADER_DESCRIPTION: %s\n",
			   header_value->str );
		
		camel_mime_part_set_description (mime_part, header_value);
		header_handled = TRUE;
		break;

	case HEADER_DISPOSITION:
		CAMEL_LOG (FULL_DEBUG,
			   "CamelMimePart::parse_header_pair found HEADER_DISPOSITION: %s\n",
			   header_value->str );
		
		camel_mime_part_set_disposition (mime_part, header_value);
		header_handled = TRUE;
		break;

	case HEADER_CONTENT_ID:
		CAMEL_LOG (FULL_DEBUG,
			   "CamelMimePart::parse_header_pair found HEADER_CONTENT_ID: %s\n",
			   header_value->str );
		
		CMP_CLASS(mime_part)->set_content_id (mime_part, header_value);
		header_handled = TRUE;
		break;
		
	case HEADER_ENCODING:
		CAMEL_LOG (FULL_DEBUG,
			   "CamelMimePart::parse_header_pair found HEADER_ENCODING: %s\n",
			   header_value->str );
		
		camel_mime_part_set_encoding (mime_part, header_value);
		header_handled = TRUE;
		break;
		
	case HEADER_CONTENT_MD5:
		CAMEL_LOG (FULL_DEBUG,
			   "CamelMimePart::parse_header_pair found HEADER_CONTENT_MD5: %s\n",
			   header_value->str );

		CMP_CLASS(mime_part)->set_content_MD5 (mime_part, header_value);		
		header_handled = TRUE;
		break;
		
	case HEADER_CONTENT_TYPE: /**** *  WARNING THIS IS BROKEN  * *****/
		CAMEL_LOG (FULL_DEBUG,
			   "CamelMimePart::parse_header_pair found HEADER_CONTENT_TYPE: %s\n",
			   header_value->str );
		
		gmime_content_field_construct_from_string (CAMEL_DATA_WRAPPER(mime_part)->content_type, header_value);
		header_handled = TRUE;
		break;
		
		
	}


	if (header_handled) {
		g_string_free (header_name, TRUE);
		return TRUE;
	} else return FALSE;
		
}
