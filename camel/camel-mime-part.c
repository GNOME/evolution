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
#include <config.h>
#include "camel-mime-part.h"
#include <stdio.h>
#include "gmime-content-field.h"
#include "string-utils.h"
#include "camel-log.h"
#include "gmime-utils.h"
#include "camel-simple-data-wrapper.h"


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

static void _add_header (CamelMimePart *mime_part, gchar *header_name, gchar *header_value);
static void _remove_header (CamelMimePart *mime_part, const gchar *header_name);
static const gchar *_get_header (CamelMimePart *mime_part, const gchar *header_name);
static void _set_description (CamelMimePart *mime_part, const gchar *description);
static const gchar *_get_description (CamelMimePart *mime_part);
static void _set_disposition (CamelMimePart *mime_part, const gchar *disposition);
static const gchar *_get_disposition (CamelMimePart *mime_part);
static void _set_filename (CamelMimePart *mime_part, gchar *filename);
static const gchar *_get_filename (CamelMimePart *mime_part);
static void _set_content_id (CamelMimePart *mime_part, gchar *content_id);
static const gchar *_get_content_id (CamelMimePart *mime_part);
static void _set_content_MD5 (CamelMimePart *mime_part, gchar *content_MD5);
static const gchar *_get_content_MD5 (CamelMimePart *mime_part);
static void _set_encoding (CamelMimePart *mime_part, gchar *encoding);
static const gchar *_get_encoding (CamelMimePart *mime_part);
static void _set_content_languages (CamelMimePart *mime_part, GList *content_languages);
static const GList *_get_content_languages (CamelMimePart *mime_part);
static void _set_header_lines (CamelMimePart *mime_part, GList *header_lines);
static const GList *_get_header_lines (CamelMimePart *mime_part);
static void _set_content_type (CamelMimePart *mime_part, const gchar *content_type);
static GMimeContentField  *_get_content_type (CamelMimePart *mime_part);

static const CamelDataWrapper *_get_content_object(CamelMimePart *mime_part);
static void _set_content_object(CamelMimePart *mime_part, CamelDataWrapper *content);

static void _write_to_stream (CamelDataWrapper *data_wrapper, CamelStream *stream);
static gboolean _parse_header_pair (CamelMimePart *mime_part, gchar *header_name, gchar *header_value);
void _construct_from_stream (CamelDataWrapper *data_wrapper, CamelStream *stream);

static void _finalize (GtkObject *object);

/* loads in a hash table the set of header names we */
/* recognize and associate them with a unique enum  */
/* identifier (see CamelHeaderType above)           */
static void
_init_header_name_table()
{
	header_name_table = g_hash_table_new (g_str_hash, g_str_equal);
	g_hash_table_insert (header_name_table, "Content-Description", (gpointer)HEADER_DESCRIPTION);
	g_hash_table_insert (header_name_table, "Content-Disposition", (gpointer)HEADER_DISPOSITION);
	g_hash_table_insert (header_name_table, "Content-id", (gpointer)HEADER_CONTENT_ID);
	g_hash_table_insert (header_name_table, "Content-Transfer-Encoding", (gpointer)HEADER_ENCODING);
	g_hash_table_insert (header_name_table, "Content-MD5", (gpointer)HEADER_CONTENT_MD5);
	g_hash_table_insert (header_name_table, "Content-Type", (gpointer)HEADER_CONTENT_TYPE);
	
}

static void
camel_mime_part_class_init (CamelMimePartClass *camel_mime_part_class)
{
	CamelDataWrapperClass *camel_data_wrapper_class = CAMEL_DATA_WRAPPER_CLASS (camel_mime_part_class);
	GtkObjectClass *gtk_object_class = GTK_OBJECT_CLASS (camel_data_wrapper_class);

	parent_class = gtk_type_class (camel_data_wrapper_get_type ());
	_init_header_name_table();
	
	/* virtual method definition */
	camel_mime_part_class->add_header = _add_header;
	camel_mime_part_class->remove_header = _remove_header;
	camel_mime_part_class->get_header = _get_header;
	camel_mime_part_class->set_description = _set_description;
	camel_mime_part_class->get_description = _get_description;
	camel_mime_part_class->set_disposition = _set_disposition;
	camel_mime_part_class->get_disposition = _get_disposition;
	camel_mime_part_class->set_filename = _set_filename;
	camel_mime_part_class->get_filename = _get_filename;
	camel_mime_part_class->set_content_id = _set_content_id;
	camel_mime_part_class->get_content_id = _get_content_id;
	camel_mime_part_class->set_content_MD5 =_set_content_MD5;
	camel_mime_part_class->get_content_MD5 = _get_content_MD5;
	camel_mime_part_class->set_encoding = _set_encoding;
	camel_mime_part_class->get_encoding = _get_encoding;
	camel_mime_part_class->set_content_languages = _set_content_languages;
	camel_mime_part_class->get_content_languages = _get_content_languages;
	camel_mime_part_class->set_header_lines =_set_header_lines;
	camel_mime_part_class->get_header_lines =_get_header_lines;
	camel_mime_part_class->set_content_type = _set_content_type;
	camel_mime_part_class->get_content_type = _get_content_type;
	
	camel_mime_part_class->parse_header_pair = _parse_header_pair;
	camel_mime_part_class->get_content_object = _get_content_object;
	camel_mime_part_class->set_content_object = _set_content_object;
	
	
	
	/* virtual method overload */
	camel_data_wrapper_class->write_to_stream = _write_to_stream;
	camel_data_wrapper_class->construct_from_stream = _construct_from_stream;

	gtk_object_class->finalize = _finalize;
}

static void
camel_mime_part_init (gpointer   object,  gpointer   klass)
{
	CamelMimePart *camel_mime_part = CAMEL_MIME_PART (object);
	
	camel_mime_part->headers =  g_hash_table_new (g_str_hash, g_str_equal);
	camel_mime_part->content_type = gmime_content_field_new (NULL, NULL);
	camel_mime_part->description = NULL;
	camel_mime_part->disposition = NULL;
	camel_mime_part->content_id = NULL;
	camel_mime_part->content_MD5 = NULL;
	camel_mime_part->content_languages = NULL;
	camel_mime_part->encoding = NULL;
	camel_mime_part->filename = NULL;
	camel_mime_part->header_lines = NULL;
	camel_mime_part->content = NULL;
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
_finalize (GtkObject *object)
{
	CamelMimePart *mime_part = CAMEL_MIME_PART (object);

#warning do something for (mime_part->disposition) which should not be a GMimeContentField

	CAMEL_LOG_FULL_DEBUG ("Entering CamelMimePart::finalize\n");

	if (mime_part->headers) {
#warning Free hash table elements
		g_hash_table_destroy (mime_part->headers);
	}

	if (mime_part->description) g_free (mime_part->description);
	if (mime_part->disposition) gmime_content_field_unref (mime_part->disposition);
	if (mime_part->content_id) g_free (mime_part->content_id);
	if (mime_part->content_MD5) g_free (mime_part->content_MD5);
	if (mime_part->content_languages) string_list_free (mime_part->content_languages);
	if (mime_part->encoding) g_free (mime_part->encoding);
	if (mime_part->filename) g_free (mime_part->filename);
	if (mime_part->header_lines) string_list_free (mime_part->header_lines);

	if (mime_part->content_type) gmime_content_field_unref (mime_part->content_type);
	if (mime_part->content) {
		CAMEL_LOG_FULL_DEBUG ("CamelMimePart::finalize, unreferencing content object\n");
		CAMEL_LOG_FULL_DEBUG ("CamelMimePart::finalize,Destroying  object %p\n", mime_part->content);
		CAMEL_LOG_FULL_DEBUG ("CamelMimePart::finalize, object type : %s\n", gtk_type_name (GTK_OBJECT_TYPE (mime_part->content)));
		gtk_object_unref (GTK_OBJECT (mime_part->content));
	}
	GTK_OBJECT_CLASS (parent_class)->finalize (object);
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelMimePart::finalize\n");
}


static void
_add_header (CamelMimePart *mime_part, gchar *header_name, gchar *header_value)
{
	gboolean header_exists;
	gchar *old_header_name;
	gchar *old_header_value;
	
	/* Try to parse the header pair. If it corresponds to something   */
	/* known, the job is done in the parsing routine. If not,         */
	/* we simply add the header in a raw fashion                      */
	if (CMP_CLASS(mime_part)->parse_header_pair (mime_part, header_name, header_value)) 
		return;
	header_exists = g_hash_table_lookup_extended (mime_part->headers, header_name, 
						      (gpointer *) &old_header_name,
						      (gpointer *) &old_header_value);
	if (header_exists) {
		g_free (old_header_name);
		g_free (old_header_value);
	}
	
	g_hash_table_insert (mime_part->headers, header_name, header_value);
}


void
camel_mime_part_add_header (CamelMimePart *mime_part, gchar *header_name, gchar *header_value)
{
	CMP_CLASS(mime_part)->add_header(mime_part, header_name, header_value);
}



static void
_remove_header (CamelMimePart *mime_part, const gchar *header_name)
{
	
	gboolean header_exists;
	gchar *old_header_name;
	gchar *old_header_value;
	
	header_exists = g_hash_table_lookup_extended (mime_part->headers, header_name, 
						      (gpointer *) &old_header_name,
						      (gpointer *) &old_header_value);
	if (header_exists) {
		g_free (old_header_name);
		g_free (old_header_value);
	}
	
	g_hash_table_remove (mime_part->headers, header_name);
	
}

void
camel_mime_part_remove_header (CamelMimePart *mime_part, const gchar *header_name)
{
	CMP_CLASS(mime_part)->remove_header(mime_part, header_name);
}



static const gchar *
_get_header (CamelMimePart *mime_part, const gchar *header_name)
{
	
	gchar *old_header_name;
	gchar *old_header_value;
	gchar *header_value;
	
	header_value = (gchar *)g_hash_table_lookup (mime_part->headers, header_name);
	return header_value;
}

const gchar *
camel_mime_part_get_header (CamelMimePart *mime_part, const gchar *header_name)
{
	return CMP_CLASS(mime_part)->get_header (mime_part, header_name);
}



static void
_set_description (CamelMimePart *mime_part, const gchar *description)
{
	if (mime_part->description) g_free (mime_part->description);
	mime_part->description = g_strdup (description);
}

void
camel_mime_part_set_description (CamelMimePart *mime_part, const gchar *description)
{
	CMP_CLASS(mime_part)->set_description (mime_part, description);
}




static const gchar *
_get_description (CamelMimePart *mime_part)
{
	return mime_part->description;
}

const gchar *
camel_mime_part_get_description (CamelMimePart *mime_part)
{
	return CMP_CLASS(mime_part)->get_description (mime_part);
}



static void
_set_disposition (CamelMimePart *mime_part, const gchar *disposition)
{
#warning Do not use MimeContentfield here !!!
	if (mime_part->disposition) g_free(mime_part->disposition);
	if (!mime_part->disposition) 
		mime_part->disposition = g_new0 (GMimeContentField,1);
	if ((mime_part->disposition)->type) g_free ((mime_part->disposition)->type);
	(mime_part->disposition)->type = g_strdup (disposition);
}


void
camel_mime_part_set_disposition (CamelMimePart *mime_part, const gchar *disposition)
{
	CMP_CLASS(mime_part)->set_disposition (mime_part, disposition);
}



static const gchar *
_get_disposition (CamelMimePart *mime_part)
{
	if (!mime_part->disposition) return NULL;
	return (mime_part->disposition)->type;
}


const gchar *
camel_mime_part_get_disposition (CamelMimePart *mime_part)
{
	return CMP_CLASS(mime_part)->get_disposition (mime_part);
}



static void
_set_filename (CamelMimePart *mime_part, gchar *filename)
{
	if (mime_part->filename) g_free(mime_part->filename);
	mime_part->filename = filename;
}


void
camel_mime_part_set_filename (CamelMimePart *mime_part, gchar *filename)
{
	CMP_CLASS(mime_part)->set_filename (mime_part, filename);
}



static const gchar *
_get_filename (CamelMimePart *mime_part)
{
	return mime_part->filename;
}


const gchar *
camel_mime_part_get_filename (CamelMimePart *mime_part)
{
	return CMP_CLASS(mime_part)->get_filename (mime_part);
}


/* this routine must not be public */
static void
_set_content_id (CamelMimePart *mime_part, gchar *content_id)
{
	if (mime_part->content_id) g_free(mime_part->content_id);
	mime_part->content_id = content_id;
}


static const gchar *
_get_content_id (CamelMimePart *mime_part)
{
	return mime_part->content_id;
}


const gchar *
camel_mime_part_get_content_id (CamelMimePart *mime_part)
{
	return CMP_CLASS(mime_part)->get_content_id (mime_part);
}


/* this routine must not be public */
static void
_set_content_MD5 (CamelMimePart *mime_part, gchar *content_MD5)
{
	if (mime_part->content_MD5) g_free(mime_part->content_MD5);
	mime_part->content_MD5 = content_MD5;
}


static const gchar *
_get_content_MD5 (CamelMimePart *mime_part)
{
	return mime_part->content_MD5;
}

const gchar *
camel_mime_part_get_content_MD5 (CamelMimePart *mime_part)
{
	return CMP_CLASS(mime_part)->get_content_MD5 (mime_part);
}



static void
_set_encoding (CamelMimePart *mime_part, gchar *encoding)
{
	if (mime_part->encoding) g_free(mime_part->encoding);
	mime_part->encoding = encoding;
}

void
camel_mime_part_set_encoding (CamelMimePart *mime_part, gchar *encoding)
{
	CMP_CLASS(mime_part)->set_encoding (mime_part, encoding);
}



static const gchar *
_get_encoding (CamelMimePart *mime_part)
{
	return mime_part->encoding;
}

const gchar *
camel_mime_part_get_encoding (CamelMimePart *mime_part)
{
	return CMP_CLASS(mime_part)->get_encoding (mime_part);
}




static void
_set_content_languages (CamelMimePart *mime_part, GList *content_languages)
{
	if (mime_part->content_languages) string_list_free (mime_part->content_languages);
	mime_part->content_languages = content_languages;
}

void
camel_mime_part_set_content_languages (CamelMimePart *mime_part, GList *content_languages)
{
	CMP_CLASS(mime_part)->set_content_languages (mime_part, content_languages);
}



static const GList *
_get_content_languages (CamelMimePart *mime_part)
{
	return mime_part->content_languages;
}


const GList *
camel_mime_part_get_content_languages (CamelMimePart *mime_part)
{
	return CMP_CLASS(mime_part)->get_content_languages (mime_part);
}



static void
_set_header_lines (CamelMimePart *mime_part, GList *header_lines)
{
	if (mime_part->header_lines) string_list_free (mime_part->header_lines);
	mime_part->header_lines = header_lines;
}

void
camel_mime_part_set_header_lines (CamelMimePart *mime_part, GList *header_lines)
{
	CMP_CLASS(mime_part)->set_header_lines (mime_part, header_lines);
}



static const GList *
_get_header_lines (CamelMimePart *mime_part)
{
	return mime_part->header_lines;
}



const GList *
camel_mime_part_get_header_lines (CamelMimePart *mime_part)
{
	return CMP_CLASS(mime_part)->get_header_lines (mime_part);
}



/*********/
static void
_set_content_type (CamelMimePart *mime_part, const gchar *content_type)
{
	g_assert (content_type);
	gmime_content_field_construct_from_string (mime_part->content_type, content_type);
}

void 
camel_mime_part_set_content_type (CamelMimePart *mime_part, gchar *content_type)
{
	CMP_CLASS(mime_part)->set_content_type (mime_part, content_type);
}

static GMimeContentField *
_get_content_type (CamelMimePart *mime_part)
{
	return mime_part->content_type;
}

GMimeContentField *
camel_mime_part_get_content_type (CamelMimePart *mime_part)
{
	return CMP_CLASS(mime_part)->get_content_type (mime_part);
}

/*********/


static const CamelDataWrapper *
_get_content_object(CamelMimePart *mime_part)
{
	return mime_part->content;
	
}


const CamelDataWrapper *
camel_mime_part_get_content_object(CamelMimePart *mime_part)
{
	return CMP_CLASS(mime_part)->get_content_object (mime_part);
}




static void
_set_content_object(CamelMimePart *mime_part, CamelDataWrapper *content)
{
	GMimeContentField *object_content_field;

	CAMEL_LOG_FULL_DEBUG ("Entering CamelMimePart::set_content_object\n");
	if (mime_part->content) {
		CAMEL_LOG_FULL_DEBUG ("CamelMimePart::set_content_object unreferencing old content object\n");
		gtk_object_unref (GTK_OBJECT (mime_part->content));
	}
	gtk_object_ref (GTK_OBJECT (content));
	mime_part->content = content;
	object_content_field = camel_data_wrapper_get_mime_type_field (content);
	if (mime_part->content_type && (mime_part->content_type != object_content_field)) 
		gmime_content_field_unref (mime_part->content_type);
	mime_part->content_type = object_content_field;
	gmime_content_field_ref (object_content_field);
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelMimePart::set_content_object\n");
	
}

void 
camel_mime_part_set_content_object(CamelMimePart *mime_part, CamelDataWrapper *content)
{
	CMP_CLASS(mime_part)->set_content_object (mime_part, content);
}




/**********************************************************************/
#ifdef WHPT
#warning : WHPT is already defined !!!!!!
#endif
#define WHPT gmime_write_header_pair_to_stream


static void
_write_content_to_stream (CamelMimePart *mime_part, CamelStream *stream)
{
	guint buffer_size;
	gchar *buffer;
	gchar *encoded_buffer;

	CamelDataWrapper *content = mime_part->content;
	CAMEL_LOG_FULL_DEBUG ( "Entering CamelMimePart::_write_content_to_stream\n");
	CAMEL_LOG_FULL_DEBUG ( "CamelMimePart::_write_content_to_stream, content=%p\n", content);
	if (!content) return;
	//	buffer_size = camel_data_wrapper_size (content);
	//buffer = g_malloc (buffer_size);
	camel_data_wrapper_write_to_stream (content, stream);
	
	//if (mime_part->encoding) {
		// encoded_buffer_size = gmime_encoded_size(buffer, buffer_size, encoding);
		// encoded_buffer = g_malloc (encoded_buffer_size);
		// gmime_encode_buffer (buffer, encoded_buffer, encoding);
		// camel_stream_write (stream, encoded_buffer, encoded_buffer_size);
		// g_free (encoded_buffer);
	//} else 
		//fwrite (buffer, buffer_size, 1, file);
		//camel_stream_write (stream, buffer, buffer_size);
	//g_free (buffer);
	CAMEL_LOG_FULL_DEBUG ( "Leaving CamelMimePart::_write_content_to_stream\n");
}





static void
_write_to_stream (CamelDataWrapper *data_wrapper, CamelStream *stream)
{
	CamelMimePart *mp = CAMEL_MIME_PART (data_wrapper);
	
	CAMEL_LOG_FULL_DEBUG ( "Entering CamelMimePart::write_to_stream\n");
	
	CAMEL_LOG_FULL_DEBUG ( "CamelMimePart::write_to_stream writing content-disposition\n");
	gmime_content_field_write_to_stream(mp->disposition, stream);
	CAMEL_LOG_FULL_DEBUG ( "CamelMimePart::write_to_stream writing content-transfer-encoding\n");
	WHPT (stream, "Content-Transfer-Encoding", mp->encoding);
	CAMEL_LOG_FULL_DEBUG ( "CamelMimePart::write_to_stream writing content-description\n");
	WHPT (stream, "Content-Description", mp->description);
	CAMEL_LOG_FULL_DEBUG ( "CamelMimePart::write_to_stream writing content-MD5\n");
	WHPT (stream, "Content-MD5", mp->content_MD5);
	CAMEL_LOG_FULL_DEBUG ( "CamelMimePart::write_to_stream writing content-id\n");
	WHPT (stream, "Content-id", mp->content_id);
	CAMEL_LOG_FULL_DEBUG ( "CamelMimePart::write_to_stream writing content-languages\n");
	write_header_with_glist_to_stream (stream, "Content-Language", mp->content_languages,", ");
	
	CAMEL_LOG_FULL_DEBUG ( "CamelMimePart::write_to_stream writing other headers\n");
	write_header_table_to_stream (stream, mp->headers);
	
	CAMEL_LOG_FULL_DEBUG ( "CamelMimePart::write_to_stream writing content-type\n");
	gmime_content_field_write_to_stream (mp->content_type, stream);
	
	camel_stream_write_string(stream,"\n");
	_write_content_to_stream (mp, stream);
	
}



/*******************************/
/* mime part parsing           */

static gboolean
_parse_header_pair (CamelMimePart *mime_part, gchar *header_name, gchar *header_value)
{
	CamelHeaderType header_type;
	gboolean header_handled = FALSE;
	
	
	header_type = (CamelHeaderType) g_hash_table_lookup (header_name_table, header_name);
	switch (header_type) {
		
	case HEADER_DESCRIPTION:
		CAMEL_LOG_FULL_DEBUG (
			   "CamelMimePart::parse_header_pair found HEADER_DESCRIPTION: %s\n",
			   header_value );
		
		camel_mime_part_set_description (mime_part, header_value);
		header_handled = TRUE;
		break;
		
	case HEADER_DISPOSITION:
		CAMEL_LOG_FULL_DEBUG (
			   "CamelMimePart::parse_header_pair found HEADER_DISPOSITION: %s\n",
			   header_value);
		
		camel_mime_part_set_disposition (mime_part, header_value);
		header_handled = TRUE;
		break;
		
	case HEADER_CONTENT_ID:
		CAMEL_LOG_FULL_DEBUG (
			   "CamelMimePart::parse_header_pair found HEADER_CONTENT_ID: %s\n",
			   header_value);
		
		CMP_CLASS(mime_part)->set_content_id (mime_part, header_value);
		header_handled = TRUE;
		break;
		
	case HEADER_ENCODING:
		CAMEL_LOG_FULL_DEBUG (
			   "CamelMimePart::parse_header_pair found HEADER_ENCODING: %s\n",
			   header_value);
		
		camel_mime_part_set_encoding (mime_part, header_value);
		header_handled = TRUE;
		break;
		
	case HEADER_CONTENT_MD5:
		CAMEL_LOG_FULL_DEBUG (
			   "CamelMimePart::parse_header_pair found HEADER_CONTENT_MD5: %s\n",
			   header_value );
		
		CMP_CLASS(mime_part)->set_content_MD5 (mime_part, header_value);		
		header_handled = TRUE;
		break;
		
	case HEADER_CONTENT_TYPE: 
		CAMEL_LOG_FULL_DEBUG (
			   "CamelMimePart::parse_header_pair found HEADER_CONTENT_TYPE: %s\n",
			   header_value );
		
		gmime_content_field_construct_from_string (mime_part->content_type, header_value);
		header_handled = TRUE;
		break;
		
		
	}
	
	
	if (header_handled) {
		g_free (header_name);
		return TRUE;
	} else return FALSE;
	
}

/* calls _parse_header_pair, but can be called 
   in a g_hash_table_for_each */
void 
_parse_hash_table_pair (gpointer key, gpointer value, gpointer user_data)
{
	gchar *header_name = (gchar *)key;
	gchar *header_value = (gchar *)value;
	CamelMimePart *mime_part = (CamelMimePart *) user_data;
	
	
	CAMEL_LOG_FULL_DEBUG ("\n--------- New Header ----------\n");
	if  (header_name)
		CAMEL_LOG_FULL_DEBUG ( "header name :%s\n", header_name);
	if  (header_value)
		CAMEL_LOG_FULL_DEBUG ( "header value :%s\n", header_value);
	
	camel_mime_part_add_header (mime_part, header_name, header_value);
	CAMEL_LOG_FULL_DEBUG ( "--------- End -----------------\n"); 
}

void
_construct_from_stream (CamelDataWrapper *data_wrapper, CamelStream *stream)
{
	GHashTable *header_table;
	CamelMimePart *mime_part = CAMEL_MIME_PART (data_wrapper);
	GMimeContentField *content_type;
	gchar *mime_type;
	GtkType content_object_type;
	CamelDataWrapper *content_object;

	CAMEL_LOG_FULL_DEBUG ("CamelMimePart:: Entering _construct_from_stream\n");
	g_assert (stream);
	CAMEL_LOG_FULL_DEBUG ("CamelMimePart::construct_from_stream parsing headers\n");
	/* parse all header lines */
	header_table = get_header_table_from_stream (stream);
	if (header_table) {
		g_hash_table_foreach (header_table, _parse_hash_table_pair, (gpointer)mime_part);
	}	
	g_hash_table_destroy (header_table);
	CAMEL_LOG_FULL_DEBUG ("CamelMimePart::construct_from_stream headers parsed \n");

	/* now parse the content */
	/* find its mime type */
	CAMEL_LOG_FULL_DEBUG ("CamelMimePart::construct_from_stream parsing content\n");
	content_type = camel_mime_part_get_content_type (mime_part);
	mime_type = gmime_content_field_get_mime_type (content_type);
	
	if (!mime_type) {
		CAMEL_LOG_FULL_DEBUG ("CamelMimePart::construct_from_stream content type field not found " 
				      "using default \"text/plain\"\n");
		mime_type = g_strdup ("text/plain");
		camel_mime_part_set_content_type (mime_part, mime_type);
	}
	content_object_type = data_wrapper_repository_get_data_wrapper_type (mime_type);
	
	CAMEL_LOG_FULL_DEBUG ("CamelMimePart::construct_from_stream content type object type used: %s\n", gtk_type_name (content_object_type));
	g_free (mime_type);
	content_object = CAMEL_DATA_WRAPPER (gtk_type_new (content_object_type));
	camel_data_wrapper_set_mime_type_field (content_object, camel_mime_part_get_content_type (mime_part));
	camel_mime_part_set_content_object (mime_part, content_object);
	camel_data_wrapper_construct_from_stream (content_object, stream);

	/* the object is referenced in the set_content_object method, so unref it here */
	gtk_object_unref (GTK_OBJECT (content_object));
	
	
	CAMEL_LOG_FULL_DEBUG ("CamelMimePart::construct_from_stream content parsed\n");
	
	
	CAMEL_LOG_FULL_DEBUG ("CamelMimePart:: Leaving _construct_from_stream\n");
}


/******************************/
/**  Misc utility functions  **/


/**
 * camel_mime_part_set_text: set the content to be some text
 * @camel_mime_part: Mime part 
 * @text: the text
 * 
 * Utility function used to set the content of a mime part object to 
 * be a text string. When @text is NULL, this routine can be used as
 * a way to remove old text content.
 * 
 **/
void 
camel_mime_part_set_text (CamelMimePart *camel_mime_part, gchar *text)
{
	CamelSimpleDataWrapper *simple_data_wrapper;
	CAMEL_LOG_FULL_DEBUG ("CamelMimePart:: Entering set_text\n");
	CAMEL_LOG_TRACE ("CamelMimePart::set_text, setting text as a mime part content\n");
	if (camel_mime_part->content) {
		CAMEL_LOG_FULL_DEBUG ("CamelMimePart::set_text unreferencing old content object\n");
		gtk_object_unref (GTK_OBJECT (camel_mime_part->content));
	}
	if (text) {
		simple_data_wrapper = camel_simple_data_wrapper_new ();
		CAMEL_LOG_FULL_DEBUG ("CamelMimePart::set_text calling CamelSimpleDataWrapper:set_text with %d chars\n", strlen (text));
		camel_data_wrapper_set_mime_type (CAMEL_DATA_WRAPPER (simple_data_wrapper), "text/plain");
		camel_simple_data_wrapper_set_text ( simple_data_wrapper, text);
		camel_mime_part_set_content_object (camel_mime_part, CAMEL_DATA_WRAPPER (simple_data_wrapper));
		gtk_object_unref (GTK_OBJECT (simple_data_wrapper));
	} else camel_mime_part->content = NULL;
		
		CAMEL_LOG_FULL_DEBUG ("CamelMimePart:: Leaving camel_mime_part_set_text\n");
}

 
