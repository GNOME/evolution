/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-mime-part.h : class for a mime part */

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


#ifndef CAMEL_MIME_PART_H
#define CAMEL_MIME_PART_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <gtk/gtk.h>
#include "camel-medium.h"
#include "camel-stream.h"


#define CAMEL_MIME_PART_TYPE     (camel_mime_part_get_type ())
#define CAMEL_MIME_PART(obj)     (GTK_CHECK_CAST((obj), CAMEL_MIME_PART_TYPE, CamelMimePart))
#define CAMEL_MIME_PART_CLASS(k) (GTK_CHECK_CLASS_CAST ((k), CAMEL_MIME_PART_TYPE, CamelMimePartClass))
#define IS_CAMEL_MIME_PART(o)    (GTK_CHECK_TYPE((o), CAMEL_MIME_PART_TYPE))


/* Do not change these values directly, you
   would regret it one day */
typedef struct 
{
	CamelMedium parent_object;
	
	/* All fields here are -** PRIVATE **- */ 
	gchar *description;
	GMimeContentField *disposition;
	gchar *content_id;
	gchar *content_MD5;
	GList *content_languages;
	gchar *encoding;
	gchar *filename;
	GList *header_lines;
	
	GByteArray *temp_message_buffer;
	GMimeContentField *content_type;
	
} CamelMimePart;



typedef struct {
	CamelMediumClass parent_class;
	
	/* Virtual methods */	
	void  (*set_description) (CamelMimePart *mime_part, const gchar *description);
	const gchar * (*get_description) (CamelMimePart *mime_part);
	void  (*set_disposition) (CamelMimePart *mime_part, const gchar *disposition);
	const gchar * (*get_disposition) (CamelMimePart *mime_part);
	void  (*set_filename) (CamelMimePart *mime_part, gchar *filename);
	const gchar * (*get_filename) (CamelMimePart *mime_part);
	void  (*set_content_id) (CamelMimePart *mime_part, gchar *content_id);
	const gchar * (*get_content_id) (CamelMimePart *mime_part);
	void  (*set_content_MD5) (CamelMimePart *mime_part, gchar *content_MD5);
	const gchar * (*get_content_MD5) (CamelMimePart *mime_part);
	void  (*set_encoding) (CamelMimePart *mime_part, gchar *encoding);
	const gchar * (*get_encoding) (CamelMimePart *mime_part);
	void  (*set_content_languages) (CamelMimePart *mime_part, GList *content_languages);
	const GList * (*get_content_languages) (CamelMimePart *mime_part);
	void  (*set_header_lines) (CamelMimePart *mime_part, GList *header_lines);
	const GList * (*get_header_lines) (CamelMimePart *mime_part);

	void  (*set_content_type) (CamelMimePart *mime_part, const gchar *content_type);
	GMimeContentField * (*get_content_type) (CamelMimePart *mime_part);
	
	gboolean (*parse_header_pair) (CamelMimePart *mime_part, gchar *header_name, gchar *header_value);
	

} CamelMimePartClass;



/* Standard Gtk function */
GtkType camel_mime_part_get_type (void);


/* public methods */
void camel_mime_part_set_description (CamelMimePart *mime_part,	
				      const gchar *description);
const gchar *camel_mime_part_get_description (CamelMimePart *mime_part);
void camel_mime_part_set_disposition (CamelMimePart *mime_part, 
				      const gchar *disposition);
const gchar *camel_mime_part_get_disposition (CamelMimePart *mime_part);
void camel_mime_part_set_filename (CamelMimePart *mime_part, 
				   gchar *filename);
const gchar *camel_mime_part_get_filename (CamelMimePart *mime_part);
const gchar *camel_mime_part_get_content_id (CamelMimePart *mime_part);
const gchar *camel_mime_part_get_content_MD5 (CamelMimePart *mime_part);
void camel_mime_part_set_encoding (CamelMimePart *mime_part, 
				   gchar *encoding);
const gchar *camel_mime_part_get_encoding (CamelMimePart *mime_part);
void camel_mime_part_set_content_languages (CamelMimePart *mime_part, 
					    GList *content_languages);
const GList *camel_mime_part_get_content_languages (CamelMimePart *mime_part);
void camel_mime_part_set_header_lines (CamelMimePart *mime_part, 
				       GList *header_lines);
const GList *camel_mime_part_get_header_lines (CamelMimePart *mime_part);

GMimeContentField *camel_mime_part_get_content_type (CamelMimePart *mime_part);

/* utility functions */
void camel_mime_part_set_text (CamelMimePart *camel_mime_part, gchar *text);




#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_MIME_PART_H */

