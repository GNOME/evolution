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





static void
camel_mime_part_class_init (CamelMimePartClass *camel_mime_part_class)
{
	parent_class = gtk_type_class (camel_data_wrapper_get_type ());
	
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
_add_header (CamelMimePart *mime_part, GString *header_name, GString *header_value)
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
	if (mime_part->disposition) g_free(mime_part->disposition);
	mime_part->disposition = disposition;
}


void
camel_mime_part_set_disposition (CamelMimePart *mime_part, GString *disposition)
{
	CMP_CLASS(mime_part)->set_disposition (mime_part, disposition);
}



static GString *
_get_disposition (CamelMimePart *mime_part)
{
	return mime_part->disposition;
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



