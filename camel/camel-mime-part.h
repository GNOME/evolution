/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; fill-column: 160 -*- */
/* camel-mime-part.h : class for a mime part */

/* 
 *
 * Authors: Bertrand Guiheneuf <bertrand@helixcode.com>
 *	    Michael Zucchi <notzed@ximian.com>
 *
 * Copyright 1999, 2000 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of version 2 of the GNU General Public 
 * License as published by the Free Software Foundation.
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
#endif /* __cplusplus */

#include <camel/camel-medium.h>
#include <camel/camel-mime-utils.h>
#include <camel/camel-mime-parser.h>

#define CAMEL_MIME_PART_TYPE     (camel_mime_part_get_type ())
#define CAMEL_MIME_PART(obj)     (CAMEL_CHECK_CAST((obj), CAMEL_MIME_PART_TYPE, CamelMimePart))
#define CAMEL_MIME_PART_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), CAMEL_MIME_PART_TYPE, CamelMimePartClass))
#define CAMEL_IS_MIME_PART(o)    (CAMEL_CHECK_TYPE((o), CAMEL_MIME_PART_TYPE))

/* Do not change these values directly, you would regret it one day */
struct _CamelMimePart {
	CamelMedium parent_object;
	
	struct _camel_header_raw *headers; /* mime headers */
	
	/* All fields here are -** PRIVATE **- */
	/* TODO: these should be in a camelcontentinfo */
	char *description;
	CamelContentDisposition *disposition;
	char *content_id;
	char *content_MD5;
	char *content_location;
	GList *content_languages;
	CamelTransferEncoding encoding;
};

typedef struct _CamelMimePartClass {
	CamelMediumClass parent_class;
	
	/* Virtual methods */
	int (*construct_from_parser) (CamelMimePart *, CamelMimeParser *);
} CamelMimePartClass;

/* Standard Camel function */
CamelType camel_mime_part_get_type (void);

/* public methods */
CamelMimePart *  camel_mime_part_new                    (void);

void	         camel_mime_part_set_description	(CamelMimePart *mime_part, const char *description);
const     char  *camel_mime_part_get_description	(CamelMimePart *mime_part);

void	         camel_mime_part_set_disposition	(CamelMimePart *mime_part, const char *disposition);
const     char  *camel_mime_part_get_disposition	(CamelMimePart *mime_part);

void	         camel_mime_part_set_filename		(CamelMimePart *mime_part, const char *filename);
const	  char  *camel_mime_part_get_filename		(CamelMimePart *mime_part);

void             camel_mime_part_set_content_id		(CamelMimePart *mime_part, const char *contentid);
const	  char  *camel_mime_part_get_content_id		(CamelMimePart *mime_part);

void		 camel_mime_part_set_content_MD5	(CamelMimePart *mime_part, const char *);
const	  char  *camel_mime_part_get_content_MD5	(CamelMimePart *mime_part);

void		 camel_mime_part_set_content_location	(CamelMimePart *mime_part, const char *);
const	  char  *camel_mime_part_get_content_location	(CamelMimePart *mime_part);

void	         camel_mime_part_set_encoding		(CamelMimePart *mime_part, CamelTransferEncoding type);
CamelTransferEncoding camel_mime_part_get_encoding	(CamelMimePart *mime_part);

void	 	 camel_mime_part_set_content_languages	(CamelMimePart *mime_part, GList *content_languages);
const	  GList *camel_mime_part_get_content_languages	(CamelMimePart *mime_part);

/* FIXME: what about content-type parameters?   what about major/minor parts? */
void               camel_mime_part_set_content_type 	(CamelMimePart *mime_part, const char *content_type);
CamelContentType  *camel_mime_part_get_content_type	(CamelMimePart *mime_part);

/* construction */
int		camel_mime_part_construct_from_parser  (CamelMimePart *, CamelMimeParser *);

/* utility functions */
void      	camel_mime_part_set_content 	       (CamelMimePart *camel_mime_part,
							const char *content, int length, const char *type);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_MIME_PART_H */

