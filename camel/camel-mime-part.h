/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camelMimePart.h : class for a mime part */

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
#include "camel-data-wrapper.h"



#define CAMEL_MIME_PART_TYPE     (camel_mime_part_get_type ())
#define CAMEL_MIME_PART(obj)     (GTK_CHECK_CAST((obj), CAMEL_MIME_PART_TYPE, CamelMimePart))
#define CAMEL_MIME_PART_CLASS(k) (GTK_CHECK_CLASS_CAST ((k), CAMEL_MIME_PART_TYPE, CamelMimePartClass))
#define IS_CAMEL_MIME_PART(o)    (GTK_CHECK_TYPE((o), CAMEL_MIME_PART_TYPE))



typedef struct 
{
	CamelDataWrapper parent_object;
	
	GHashTable *headers;
	GString *description;
	GString *disposition;
	GString *content_id;
	GString *content_MD5;
	GList *content_languages;
	GString *encoding;
	GString *filename;
	GList *header_lines;

	CamelDataWrapper *content; /* part real content */

} CamelMimePart;



typedef struct {
	CamelDataWrapperClass parent_class;
	
	/* Virtual methods */	
	void  (*add_header) (CamelMimePart *mime_part, GString *header_name, GString *header_value);
	void  (*remove_header) (CamelMimePart *mime_part, GString *header_name);
	GString * (*get_header) (CamelMimePart *mime_part, GString *header_name);
	void  (*set_description) (CamelMimePart *mime_part, GString *description);
	GString * (*get_description) (CamelMimePart *mime_part);
	void  (*set_disposition) (CamelMimePart *mime_part, GString *disposition);
	GString * (*get_disposition) (CamelMimePart *mime_part);
	void  (*set_filename) (CamelMimePart *mime_part, GString *filename);
	GString * (*get_filename) (CamelMimePart *mime_part);
	void  (*set_content_id) (CamelMimePart *mime_part, GString *content_id);
	GString * (*get_content_id) (CamelMimePart *mime_part);
	void  (*set_content_MD5) (CamelMimePart *mime_part, GString *content_MD5);
	GString * (*get_content_MD5) (CamelMimePart *mime_part);
	void  (*set_encoding) (CamelMimePart *mime_part, GString *encoding);
	GString * (*get_encoding) (CamelMimePart *mime_part);
	void  (*set_content_languages) (CamelMimePart *mime_part, GList *content_languages);
	GList * (*get_content_languages) (CamelMimePart *mime_part);
	void  (*set_header_lines) (CamelMimePart *mime_part, GList *header_lines);
	GList * (*get_header_lines) (CamelMimePart *mime_part);

} CamelMimePartClass;



/* Standard Gtk function */
GtkType camel_mime_part_get_type (void);


/* public methods */
void camel_mime_part_add_header (CamelMimePart *mime_part, GString *header_name, GString *header_value);
void camel_mime_part_remove_header (CamelMimePart *mime_part, GString *header_name);
GString *camel_mime_part_get_header (CamelMimePart *mime_part, GString *header_name);
void camel_mime_part_set_description (CamelMimePart *mime_part, GString *description);
GString *camel_mime_part_get_description (CamelMimePart *mime_part);
void camel_mime_part_set_disposition (CamelMimePart *mime_part, GString *disposition);
GString *camel_mime_part_get_disposition (CamelMimePart *mime_part);
void camel_mime_part_set_filename (CamelMimePart *mime_part, GString *filename);
GString *camel_mime_part_get_filename (CamelMimePart *mime_part);
GString *camel_mime_part_get_content_id (CamelMimePart *mime_part);
GString *camel_mime_part_get_content_MD5 (CamelMimePart *mime_part);
void camel_mime_part_set_encoding (CamelMimePart *mime_part, GString *encoding);
GString *camel_mime_part_get_encoding (CamelMimePart *mime_part);
void camel_mime_part_set_content_languages (CamelMimePart *mime_part, GList *content_languages);
GList *camel_mime_part_get_content_languages (CamelMimePart *mime_part);
void camel_mime_part_set_header_lines (CamelMimePart *mime_part, GList *header_lines);
GList *camel_mime_part_get_header_lines (CamelMimePart *mime_part);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_MIME_PART_H */
