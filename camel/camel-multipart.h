/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-multipart.h : class for a multipart */

/*
 *
 * Author :
 *  Bertrand Guiheneuf <bertrand@helixcode.com>
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


#ifndef CAMEL_MULTIPART_H
#define CAMEL_MULTIPART_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <camel/camel-data-wrapper.h>

#define CAMEL_MULTIPART_TYPE     (camel_multipart_get_type ())
#define CAMEL_MULTIPART(obj)     (CAMEL_CHECK_CAST((obj), CAMEL_MULTIPART_TYPE, CamelMultipart))
#define CAMEL_MULTIPART_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), CAMEL_MULTIPART_TYPE, CamelMultipartClass))
#define CAMEL_IS_MULTIPART(o)    (CAMEL_CHECK_TYPE((o), CAMEL_MULTIPART_TYPE))

struct _CamelMimeParser;

struct _CamelMultipart
{
	CamelDataWrapper parent_object;

	GList *parts;
	gchar *preface;
	gchar *postface;
};

typedef struct {
	CamelDataWrapperClass parent_class;

	/* Virtual methods */
	void (*add_part) (CamelMultipart *multipart, CamelMimePart *part);
	void (*add_part_at) (CamelMultipart *multipart, CamelMimePart *part, guint index);
	void (*remove_part) (CamelMultipart *multipart, CamelMimePart *part);
	CamelMimePart * (*remove_part_at) (CamelMultipart *multipart, guint index);
	CamelMimePart * (*get_part) (CamelMultipart *multipart, guint index);
	guint (*get_number) (CamelMultipart *multipart);
	void (*set_boundary) (CamelMultipart *multipart, const char *boundary);
	const gchar * (*get_boundary) (CamelMultipart *multipart);

	int (*construct_from_parser)(CamelMultipart *, struct _CamelMimeParser *);
	/*int (*construct_from_stream)(CamelMultipart *, CamelStream *);*/

} CamelMultipartClass;

/* Standard Camel function */
CamelType camel_multipart_get_type (void);


/* public methods */
CamelMultipart *    camel_multipart_new            (void);
void                camel_multipart_add_part       (CamelMultipart *multipart,
						    CamelMimePart *part);
void                camel_multipart_add_part_at    (CamelMultipart *multipart,
						    CamelMimePart *part,
						    guint index);
void                camel_multipart_remove_part    (CamelMultipart *multipart,
						    CamelMimePart *part);
CamelMimePart *     camel_multipart_remove_part_at (CamelMultipart *multipart,
						    guint index);
CamelMimePart *     camel_multipart_get_part       (CamelMultipart *multipart,
						    guint index);
guint               camel_multipart_get_number     (CamelMultipart *multipart);
void                camel_multipart_set_boundary   (CamelMultipart *multipart,
						    const char *boundary);
const gchar *       camel_multipart_get_boundary   (CamelMultipart *multipart);

void		    camel_multipart_set_preface	   (CamelMultipart *multipart, const char *preface);
void		    camel_multipart_set_postface   (CamelMultipart *multipart, const char *postface);

int		    camel_multipart_construct_from_parser(CamelMultipart *multipart, struct _CamelMimeParser *mp);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_MULTIPART_H */

