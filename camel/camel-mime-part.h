/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-mime-part.h : class for a mime part */

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


#ifndef CAMEL_MIME_PART_H
#define CAMEL_MIME_PART_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <gtk/gtk.h>
#include "camel-types.h"
#include "camel-medium.h"
#include <camel/camel-mime-utils.h>

#define CAMEL_MIME_PART_TYPE     (camel_mime_part_get_type ())
#define CAMEL_MIME_PART(obj)     (GTK_CHECK_CAST((obj), CAMEL_MIME_PART_TYPE, CamelMimePart))
#define CAMEL_MIME_PART_CLASS(k) (GTK_CHECK_CLASS_CAST ((k), CAMEL_MIME_PART_TYPE, CamelMimePartClass))
#define CAMEL_IS_MIME_PART(o)    (GTK_CHECK_TYPE((o), CAMEL_MIME_PART_TYPE))


enum _CamelMimePartEncodingType {
        CAMEL_MIME_PART_ENCODING_DEFAULT,
        CAMEL_MIME_PART_ENCODING_7BIT,
        CAMEL_MIME_PART_ENCODING_8BIT,
        CAMEL_MIME_PART_ENCODING_BASE64,
        CAMEL_MIME_PART_ENCODING_QUOTEDPRINTABLE,
        CAMEL_MIME_PART_NUM_ENCODINGS
};
typedef enum _CamelMimePartEncodingType CamelMimePartEncodingType;


/* Do not change these values directly, you
   would regret it one day */
struct _CamelMimePart
{
	CamelMedium parent_object;
	
	/* All fields here are -** PRIVATE **- */ 
	gchar *description;
	CamelMimeDisposition *disposition;
	gchar *content_id;
	gchar *content_MD5;
	GList *content_languages;
	CamelMimePartEncodingType encoding;
	
	GByteArray *temp_message_buffer;
	GMimeContentField *content_type;
	CamelStream *content_input_stream;
	
};



typedef struct {
	CamelMediumClass parent_class;
	
	/* Virtual methods */	
} CamelMimePartClass;



/* Standard Gtk function */
GtkType camel_mime_part_get_type (void);


/* public methods */
void	         camel_mime_part_set_description	(CamelMimePart *mime_part, const gchar *description);
const     gchar *camel_mime_part_get_description	(CamelMimePart *mime_part);

void	         camel_mime_part_set_disposition	(CamelMimePart *mime_part, const gchar *disposition);
const     gchar *camel_mime_part_get_disposition	(CamelMimePart *mime_part);

void	         camel_mime_part_set_filename		(CamelMimePart *mime_part, gchar *filename);
const	  gchar *camel_mime_part_get_filename		(CamelMimePart *mime_part);

void             camel_mime_part_set_content_id		(CamelMimePart *mime_part, const char *contentid);
const	  gchar *camel_mime_part_get_content_id		(CamelMimePart *mime_part);

void		 camel_mime_part_set_content_MD5	(CamelMimePart *mime_part, const char *);
const	  gchar *camel_mime_part_get_content_MD5	(CamelMimePart *mime_part);

void	         camel_mime_part_set_encoding		(CamelMimePart *mime_part, CamelMimePartEncodingType type);
CamelMimePartEncodingType camel_mime_part_get_encoding	(CamelMimePart *mime_part);

void	 	 camel_mime_part_set_content_languages	(CamelMimePart *mime_part, GList *content_languages);
const	  GList *camel_mime_part_get_content_languages	(CamelMimePart *mime_part);

/* FIXME: what about content-type parameters?   what about major/minor parts? */
void               camel_mime_part_set_content_type 	(CamelMimePart *mime_part, gchar *content_type);
GMimeContentField *camel_mime_part_get_content_type	(CamelMimePart *mime_part);

const     gchar *         camel_mime_part_encoding_to_string   (CamelMimePartEncodingType encoding);
CamelMimePartEncodingType camel_mime_part_encoding_from_string (const gchar *string);

/* utility functions */
void      camel_mime_part_set_text			(CamelMimePart *camel_mime_part, 
							 const gchar *text);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_MIME_PART_H */

