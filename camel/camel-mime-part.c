/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camelMimePart.c : Abstract class for a mime_part */

/* 
 * Authors: Bertrand Guiheneuf <bertrand@helixcode.com>
 *	    Michael Zucchi <notzed@helixcode.com>
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
#include <string.h>
#include "camel-mime-part.h"
#include <stdio.h>
#include "gmime-content-field.h"
#include "string-utils.h"
#include "gmime-utils.h"
#include "camel-simple-data-wrapper.h"
#include "hash-table-utils.h"
#include "camel-stream-mem.h"
#include "camel-mime-part-utils.h"
#include "camel-seekable-substream.h"
#include "camel-stream-filter.h"
#include "camel-mime-filter-basic.h"
#include <ctype.h>

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


static CamelMediumClass *parent_class=NULL;

/* Returns the class for a CamelMimePart */
#define CMP_CLASS(so) CAMEL_MIME_PART_CLASS (GTK_OBJECT(so)->klass)
#define CDW_CLASS(so) CAMEL_DATA_WRAPPER_CLASS (GTK_OBJECT(so)->klass)
#define CMD_CLASS(so) CAMEL_MEDIUM_CLASS (GTK_OBJECT(so)->klass)

/* from GtkObject */
static void            my_finalize (GtkObject *object);

/* from CamelDataWrapper */
static void            my_write_to_stream              (CamelDataWrapper *data_wrapper, 
							CamelStream *stream);
static void            my_construct_from_stream        (CamelDataWrapper *data_wrapper, 
							CamelStream *stream);
static void            my_set_input_stream             (CamelDataWrapper *data_wrapper, 
							CamelStream *stream);
static CamelStream *   my_get_output_stream            (CamelDataWrapper *data_wrapper);


/* from CamelMedia */ 
static void            add_header                      (CamelMedium *medium, const char *header_name, const char *header_value);
static void            set_header                      (CamelMedium *medium, const char *header_name, const char *header_value);
static void            remove_header                   (CamelMedium *medium, const char *header_name);

static void            my_set_content_object           (CamelMedium *medium, 
							CamelDataWrapper *content);
static CamelDataWrapper *my_get_content_object         (CamelMedium *medium);

/* forward references */
static void set_disposition (CamelMimePart *mime_part, const gchar *disposition);


/* loads in a hash table the set of header names we */
/* recognize and associate them with a unique enum  */
/* identifier (see CamelHeaderType above)           */
static void
my_init_header_name_table()
{
	header_name_table = g_hash_table_new (g_strcase_hash, g_strcase_equal);
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
	CamelMediumClass *camel_medium_class = CAMEL_MEDIUM_CLASS (camel_mime_part_class);
	CamelDataWrapperClass *camel_data_wrapper_class = CAMEL_DATA_WRAPPER_CLASS (camel_mime_part_class);
	GtkObjectClass *gtk_object_class = GTK_OBJECT_CLASS (camel_data_wrapper_class);

	parent_class = gtk_type_class (camel_medium_get_type ());
	my_init_header_name_table();
	
	/* virtual method overload */	
	camel_medium_class->add_header                = add_header;
	camel_medium_class->set_header                = set_header;
	camel_medium_class->remove_header             = remove_header;
	camel_medium_class->set_content_object        = my_set_content_object;
	camel_medium_class->get_content_object        = my_get_content_object;

	camel_data_wrapper_class->write_to_stream     = my_write_to_stream;
	camel_data_wrapper_class->construct_from_stream = my_construct_from_stream;
	camel_data_wrapper_class->set_input_stream    = my_set_input_stream;
	camel_data_wrapper_class->get_output_stream   = my_get_output_stream;

	gtk_object_class->finalize                    = my_finalize;
}

static void
camel_mime_part_init (gpointer   object,  gpointer   klass)
{
	CamelMimePart *camel_mime_part = CAMEL_MIME_PART (object);
	
	camel_mime_part->content_type         = gmime_content_field_new (NULL, NULL);
	camel_mime_part->description          = NULL;
	camel_mime_part->disposition          = NULL;
	camel_mime_part->content_id           = NULL;
	camel_mime_part->content_MD5          = NULL;
	camel_mime_part->content_languages    = NULL;
	camel_mime_part->encoding             = CAMEL_MIME_PART_ENCODING_DEFAULT;

	camel_mime_part->temp_message_buffer  = NULL;	
	camel_mime_part->content_input_stream = NULL;
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
		
		camel_mime_part_type = gtk_type_unique (camel_medium_get_type (), &camel_mime_part_info);
	}
	
	return camel_mime_part_type;
}


static void           
my_finalize (GtkObject *object)
{
	CamelMimePart *mime_part = CAMEL_MIME_PART (object);

	g_free (mime_part->description);
	g_free (mime_part->content_id);
	g_free (mime_part->content_MD5);
	string_list_free (mime_part->content_languages);
	header_disposition_unref(mime_part->disposition);
	
	if (mime_part->content_type) gmime_content_field_unref (mime_part->content_type);
	if (mime_part->temp_message_buffer) g_byte_array_free (mime_part->temp_message_buffer, TRUE);

	if (mime_part->content_input_stream) gtk_object_unref (GTK_OBJECT (mime_part->content_input_stream));

	GTK_OBJECT_CLASS (parent_class)->finalize (object);
}

/* **** */

static gboolean
process_header(CamelMedium *medium, const char *header_name, const char *header_value)
{
	CamelMimePart *mime_part = CAMEL_MIME_PART (medium);
	CamelHeaderType header_type;
	char *text;

	/* Try to parse the header pair. If it corresponds to something   */
	/* known, the job is done in the parsing routine. If not,         */
	/* we simply add the header in a raw fashion                      */

	/* FIXMME: MUST check fields for validity before adding them! */

	header_type = (CamelHeaderType) g_hash_table_lookup (header_name_table, header_name);
	switch (header_type) {
	case HEADER_DESCRIPTION: /* raw header->utf8 conversion */
		text = header_decode_string(header_value);
		g_free(mime_part->description);
		mime_part->description = text;
		break;
	case HEADER_DISPOSITION:
		set_disposition (mime_part, header_value);
		break;
	case HEADER_CONTENT_ID:
		text = header_msgid_decode(header_value);
		g_free(mime_part->content_id);
		mime_part->content_id = text;
		break;
	case HEADER_ENCODING:
		text = header_token_decode(header_value);
		camel_mime_part_set_encoding(mime_part, camel_mime_part_encoding_from_string (text));
		g_free(text);
		break;
	case HEADER_CONTENT_MD5:
		g_free(mime_part->content_MD5);
		mime_part->content_MD5 = g_strdup(header_value);
		break;
	case HEADER_CONTENT_TYPE: 
		gmime_content_field_construct_from_string (mime_part->content_type, header_value);
		break;
	default:
		return FALSE;
	}
	return TRUE;
}


static void
set_header (CamelMedium *medium, const char *header_name, const char *header_value)
{
	process_header(medium, header_name, header_value);
	parent_class->set_header (medium, header_name, header_value);
}

static void
add_header (CamelMedium *medium, const char *header_name, const char *header_value)
{
	/* Try to parse the header pair. If it corresponds to something   */
	/* known, the job is done in the parsing routine. If not,         */
	/* we simply add the header in a raw fashion                      */

	/* FIXMME: MUST check fields for validity before adding them! */

	/* If it was one of the headers we handled, it must be unique, set it instead of add */
	if (process_header(medium, header_name, header_value))
		parent_class->set_header (medium, header_name, header_value);
	else
		parent_class->add_header (medium, header_name, header_value);
}

static void
remove_header (CamelMedium *medium, const char *header_name)
{
	process_header(medium, header_name, NULL);
	parent_class->remove_header (medium, header_name);
}


/* **** Content-Description */
void
camel_mime_part_set_description (CamelMimePart *mime_part, const gchar *description)
{
	char *text;

	/* FIXME: convert header, internationalise, etc. */
	text = g_strdup(description);
	/* text = header_encode_string(description); */

	g_free(mime_part->description);
	mime_part->description = text;

	parent_class->set_header ((CamelMedium *)mime_part, "Content-Description", text);
}

const gchar *
camel_mime_part_get_description (CamelMimePart *mime_part)
{
	return mime_part->description;
}

/* **** Content-Disposition */

static void
set_disposition (CamelMimePart *mime_part, const gchar *disposition)
{
	header_disposition_unref(mime_part->disposition);
	if (disposition)
		mime_part->disposition = header_disposition_decode(disposition);
	else
		mime_part->disposition = NULL;
}


void
camel_mime_part_set_disposition (CamelMimePart *mime_part, const gchar *disposition)
{
	char *text;

	/* we poke in a new disposition (so we dont lose 'filename', etc) */
	if (mime_part->disposition == NULL) {
		set_disposition(mime_part, disposition);
	}
	if (mime_part->disposition != NULL) {
		g_free(mime_part->disposition->disposition);
		mime_part->disposition->disposition = g_strdup(disposition);
	}
	text = header_disposition_format(mime_part->disposition);

	parent_class->set_header ((CamelMedium *)mime_part, "Content-Description", text);

	g_free(text);
}

const gchar *
camel_mime_part_get_disposition (CamelMimePart *mime_part)
{
	if (mime_part->disposition)
		return (mime_part->disposition)->disposition;
	else
		return NULL;
}


/* **** Content-Disposition: filename="xxx" */

void
camel_mime_part_set_filename (CamelMimePart *mime_part, gchar *filename)
{
	char *str;
	if (mime_part->disposition == NULL)
		mime_part->disposition = header_disposition_decode("attachment");

	header_set_param(&mime_part->disposition->params, "filename", filename);
	str = header_disposition_format(mime_part->disposition);

	/* we dont want to override what we just created ... */
	parent_class->set_header ((CamelMedium *)mime_part, "Content-Disposition", str);
	g_free(str);
}

const gchar *
camel_mime_part_get_filename (CamelMimePart *mime_part)
{
	if (mime_part->disposition)
		return header_param(mime_part->disposition->params, "filename");
	return NULL;
}


/* **** Content-ID: */

void
camel_mime_part_set_content_id (CamelMimePart *mime_part, const char *contentid)
{
	char *text;

	/* perform a syntax check, just 'cause we can */
	text = header_msgid_decode(contentid);
	if (text == NULL) {
		g_warning("Invalid content id being set: '%s'", contentid);
	} else {
		g_free(text);
	}
	g_free(mime_part->content_id);
	mime_part->content_id = g_strdup(contentid);
	parent_class->set_header ((CamelMedium *)mime_part, "Content-ID", contentid);
}

const gchar *
camel_mime_part_get_content_id (CamelMimePart *mime_part)
{
	return mime_part->content_id;
}

/* **** Content-MD5: */

void
camel_mime_part_set_content_MD5 (CamelMimePart *mime_part, const char *md5)
{
	g_free(mime_part->content_MD5);
	mime_part->content_MD5 = g_strdup(md5);
	parent_class->set_header ((CamelMedium *)mime_part, "Content-MD5", md5);
}

const gchar *
camel_mime_part_get_content_MD5 (CamelMimePart *mime_part)
{
	return mime_part->content_MD5;
}

/* **** Content-Transfer-Encoding: */

void
camel_mime_part_set_encoding (CamelMimePart *mime_part,
			      CamelMimePartEncodingType encoding)
{
	const char *text;

	mime_part->encoding = encoding;
	text = camel_mime_part_encoding_to_string (encoding);
	if (text[0])
		text = g_strdup(text);
	else
		text = NULL;

	parent_class->set_header ((CamelMedium *)mime_part, "Content-Transfer-Encoding", text);
}

const CamelMimePartEncodingType
camel_mime_part_get_encoding (CamelMimePart *mime_part)
{
	return mime_part->encoding;
}

/* FIXME: do something with this stuff ... */

void
camel_mime_part_set_content_languages (CamelMimePart *mime_part, GList *content_languages)
{
	if (mime_part->content_languages) string_list_free (mime_part->content_languages);
	mime_part->content_languages = content_languages;

	/* FIXME: translate to a header and set it */
}

const GList *
camel_mime_part_get_content_languages (CamelMimePart *mime_part)
{
	return mime_part->content_languages;
}


/* **** */

/* **** Content-Type: */

void 
camel_mime_part_set_content_type (CamelMimePart *mime_part, gchar *content_type)
{
	/* FIXME: need a way to specify content-type parameters without putting them
	   in a string ... */
	gmime_content_field_construct_from_string (mime_part->content_type, content_type);
	parent_class->set_header ((CamelMedium *)mime_part, "Content-Type", content_type);
}

GMimeContentField *
camel_mime_part_get_content_type (CamelMimePart *mime_part)
{
	return mime_part->content_type;
}

/*********/



static void
my_set_content_object (CamelMedium *medium, CamelDataWrapper *content)
{
	CamelMimePart *mime_part = CAMEL_MIME_PART (medium);
	GMimeContentField *object_content_field;

	parent_class->set_content_object (medium, content);

	object_content_field = camel_data_wrapper_get_mime_type_field (content);
	if (mime_part->content_type && (mime_part->content_type != object_content_field)) {
		char *txt;

		gmime_content_field_unref (mime_part->content_type);
		txt = header_content_type_format(object_content_field?object_content_field->content_type:NULL);
		parent_class->set_header ((CamelMedium *)mime_part, "Content-Type", txt);
	}
	mime_part->content_type = object_content_field;

	gmime_content_field_ref (object_content_field);
}

static CamelDataWrapper *
my_get_content_object (CamelMedium *medium)
{
	CamelMimePart *mime_part = CAMEL_MIME_PART (medium);
	CamelStream *stream;
	CamelStream *decoded_stream;
	CamelMimeFilter *mf = NULL;

	if (!medium->content ) {
		stream = mime_part->content_input_stream; 
		decoded_stream = stream;

		switch (mime_part->encoding) {
		case CAMEL_MIME_PART_ENCODING_QUOTEDPRINTABLE:
			mf = (CamelMimeFilter *)camel_mime_filter_basic_new_type(CAMEL_MIME_FILTER_BASIC_QP_DEC);
			break;
		case CAMEL_MIME_PART_ENCODING_BASE64:
			mf = (CamelMimeFilter *)camel_mime_filter_basic_new_type(CAMEL_MIME_FILTER_BASIC_BASE64_DEC);
			break;
		default:
			break;
		}

		if (mf) {
			decoded_stream = (CamelStream *)camel_stream_filter_new_with_stream (stream);
			camel_stream_filter_add((CamelStreamFilter *)decoded_stream, mf);
			gtk_object_unref((GtkObject *)mf);
		}

		camel_mime_part_construct_content_from_stream (mime_part, decoded_stream);
		
	}

	return parent_class->get_content_object (medium);
		
}


/* **** */




/**********************************************************************/
#ifdef WHPT
#warning : WHPT is already defined !!!!!!
#endif
#define WHPT gmime_write_header_pair_to_stream


static void
my_write_content_to_stream (CamelMimePart *mime_part, CamelStream *stream)
{
	CamelMedium *medium;
	CamelStream *wrapper_stream;
	CamelStream *stream_encode;
	CamelMimeFilter *mf = NULL;
	CamelDataWrapper *content;

	g_assert (mime_part);

	medium = CAMEL_MEDIUM (mime_part);
	content = medium->content;
	
	if (!content) {
		content = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));
		if (!content)
			return;
	}

	switch (mime_part->encoding) {
	case CAMEL_MIME_PART_ENCODING_DEFAULT:
	case CAMEL_MIME_PART_ENCODING_7BIT:
	case CAMEL_MIME_PART_ENCODING_8BIT:
		camel_data_wrapper_write_to_stream (content, stream);
		break;
	case CAMEL_MIME_PART_ENCODING_QUOTEDPRINTABLE:
		mf = (CamelMimeFilter *)camel_mime_filter_basic_new_type(CAMEL_MIME_FILTER_BASIC_QP_ENC);
		break;
	case CAMEL_MIME_PART_ENCODING_BASE64:
		mf = (CamelMimeFilter *)camel_mime_filter_basic_new_type(CAMEL_MIME_FILTER_BASIC_BASE64_ENC);
		break;
	default:
		camel_data_wrapper_write_to_stream (content, stream);
		g_warning ("Encoding type `%s' not supported.",
			   camel_mime_part_encoding_to_string
			   (mime_part->encoding));
	}

	if (mf) {
		/* encode the data wrapper output stream in the filtered encoding */
		wrapper_stream = camel_data_wrapper_get_output_stream (content);
		camel_stream_reset (wrapper_stream);
		stream_encode = (CamelStream *)camel_stream_filter_new_with_stream (wrapper_stream);
		camel_stream_filter_add((CamelStreamFilter *)stream_encode, mf);

		/*  ... and write it to the output stream in a blocking way */
		camel_stream_write_to_stream (stream_encode, stream);
		
		/* now free the intermediate b64 stream */
		gtk_object_unref (GTK_OBJECT (stream_encode));
		gtk_object_unref((GtkObject *)mf);
	}
}




/* FIXME: this is just totally broken broken broken broken */

static void
my_write_to_stream (CamelDataWrapper *data_wrapper, CamelStream *stream)
{
	CamelMimePart *mp = CAMEL_MIME_PART (data_wrapper);
	CamelMedium *medium = CAMEL_MEDIUM (data_wrapper);

	/* FIXME: something needs to be done about this ... */
	gmime_write_header_with_glist_to_stream (stream, "Content-Language", mp->content_languages,", ");

#warning This class should NOT BE WRITING the headers out
	if (medium->headers) {
		struct _header_raw *h = medium->headers;
		while (h) {
			camel_stream_write_strings (stream, h->name, isspace(h->value[0])?":":": ", h->value, "\n", NULL);
			h = h->next;
		}
	}

	camel_stream_write_string(stream,"\n");
	my_write_content_to_stream (mp, stream);
}



static void
my_construct_from_stream (CamelDataWrapper *data_wrapper, CamelStream *stream)
{

	CamelMimePart *mime_part = CAMEL_MIME_PART (data_wrapper);
	
	camel_mime_part_construct_headers_from_stream (mime_part, stream);
	
	camel_mime_part_store_stream_in_buffer (mime_part, stream);
}




static void 
my_set_input_stream (CamelDataWrapper *data_wrapper, CamelStream *stream)
{
	CamelMimePart *mime_part = CAMEL_MIME_PART (data_wrapper);
	CamelSeekableStream *seekable_stream;
	guint32 content_stream_inf_bound;
	

	g_assert (CAMEL_IS_SEEKABLE_STREAM (stream));
	seekable_stream = CAMEL_SEEKABLE_STREAM (stream);

	/* call parent class implementation */
	CAMEL_DATA_WRAPPER_CLASS (parent_class)->set_input_stream (data_wrapper, stream);


	camel_mime_part_construct_headers_from_stream (mime_part, stream);
	
	/* set the input stream for the content object */
	content_stream_inf_bound = camel_seekable_stream_get_current_position (seekable_stream);
	
	if (mime_part->content_input_stream)
		gtk_object_unref (GTK_OBJECT (mime_part->content_input_stream));
	mime_part->content_input_stream = camel_seekable_substream_new_with_seekable_stream_and_bounds (seekable_stream,
													content_stream_inf_bound, 
													-1);
	gtk_object_ref (GTK_OBJECT (mime_part->content_input_stream));
	gtk_object_sink (GTK_OBJECT (mime_part->content_input_stream));
}


static CamelStream *
my_get_output_stream (CamelDataWrapper *data_wrapper)
{
	CamelMimePart *mime_part = CAMEL_MIME_PART (data_wrapper);
	CamelStream *input_stream;
	CamelStream *output_stream;
	/* ** FIXME : bogus bogus bogus - test test test */

	return NULL; 

	/* 
	 * For the moment, we do not use this routine on 
	 * mime parts. Maybe later.
	 */
	input_stream = camel_data_wrapper_get_input_stream (data_wrapper);
	
	if (input_stream == NULL)
		return NULL;

	switch (mime_part->encoding) {
		
	case CAMEL_MIME_PART_ENCODING_DEFAULT:
	case CAMEL_MIME_PART_ENCODING_7BIT:
	case CAMEL_MIME_PART_ENCODING_8BIT:
		return input_stream;
		
	case CAMEL_MIME_PART_ENCODING_BASE64:
		return output_stream;

	case CAMEL_MIME_PART_ENCODING_QUOTEDPRINTABLE:
		return input_stream;
	default:
		break;
	}

	return NULL;
}


const gchar *
camel_mime_part_encoding_to_string (CamelMimePartEncodingType encoding)
{
	switch (encoding) {
	case CAMEL_MIME_PART_ENCODING_DEFAULT:
	case CAMEL_MIME_PART_ENCODING_7BIT:
		return "7bit";
	case CAMEL_MIME_PART_ENCODING_8BIT:
		return "8bit";
	case CAMEL_MIME_PART_ENCODING_BASE64:
		return "base64";
	case CAMEL_MIME_PART_ENCODING_QUOTEDPRINTABLE:
		return "quoted-printable";
	default:
		break;
	}
	return "";
}



/* FIXME I am not sure this is the correct way to do this.  */
CamelMimePartEncodingType
camel_mime_part_encoding_from_string (const gchar *string)
{
	if (string == NULL)
		return CAMEL_MIME_PART_ENCODING_DEFAULT;
	else if (strcmp (string, "7bit") == 0)
		return CAMEL_MIME_PART_ENCODING_7BIT;
	else if (strcmp (string, "8bit") == 0)
		return CAMEL_MIME_PART_ENCODING_8BIT;
	else if (strcmp (string, "base64") == 0)
		return CAMEL_MIME_PART_ENCODING_BASE64;
	else if (strcmp (string, "quoted-printable") == 0)
		return CAMEL_MIME_PART_ENCODING_QUOTEDPRINTABLE;
	else
		/* FIXME?  Spit a warning?  */
		return CAMEL_MIME_PART_ENCODING_DEFAULT;
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
camel_mime_part_set_text (CamelMimePart *camel_mime_part, const gchar *text)
{
	CamelSimpleDataWrapper *simple_data_wrapper;
	CamelMedium *medium = CAMEL_MEDIUM (camel_mime_part);

	if (medium->content)
		gtk_object_unref (GTK_OBJECT (medium->content));
	if (text) {
		simple_data_wrapper = camel_simple_data_wrapper_new ();
		camel_data_wrapper_set_mime_type (CAMEL_DATA_WRAPPER (simple_data_wrapper), "text/plain");
		camel_simple_data_wrapper_set_text ( simple_data_wrapper, text);
		camel_medium_set_content_object ( CAMEL_MEDIUM (camel_mime_part), CAMEL_DATA_WRAPPER (simple_data_wrapper));
		gtk_object_unref (GTK_OBJECT (simple_data_wrapper));
	} else medium->content = NULL;
}

 
