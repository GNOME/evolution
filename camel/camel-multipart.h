/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-multipart.h : class for a multipart */

/* 
 *
 * Author : 
 *  Bertrand Guiheneuf <bertrand@helixcode.com>
 *
 * Copyright 1999, 2000 HelixCode (http://www.helixcode.com) .
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


#ifndef CAMEL_MULTIPART_H
#define CAMEL_MULTIPART_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

typedef struct _CamelMultipart CamelMultipart;

#include <gtk/gtk.h>
#include "camel-mime-part.h"
#include "camel-mime-body-part.h"


#define CAMEL_MULTIPART_TYPE     (camel_multipart_get_type ())
#define CAMEL_MULTIPART(obj)     (GTK_CHECK_CAST((obj), CAMEL_MULTIPART_TYPE, CamelMultipart))
#define CAMEL_MULTIPART_CLASS(k) (GTK_CHECK_CLASS_CAST ((k), CAMEL_MULTIPART_TYPE, CamelMultipartClass))
#define CAMEL_IS_MULTIPART(o)    (GTK_CHECK_TYPE((o), CAMEL_MULTIPART_TYPE))


struct _CamelMultipart
{
	CamelDataWrapper parent_object;

	CamelMimePart *parent;
	GList *parts;
	gchar *boundary;
	gchar *preface;
	gchar *postface;

};



typedef struct {
	CamelDataWrapperClass parent_class;
	
	/* Virtual methods */	
	void (*add_part) (CamelMultipart *multipart, CamelMimeBodyPart *part);
	void (*add_part_at) (CamelMultipart *multipart, CamelMimeBodyPart *part, guint index);
	void (*remove_part) (CamelMultipart *multipart, CamelMimeBodyPart *part);
	CamelMimeBodyPart * (*remove_part_at) (CamelMultipart *multipart, guint index);
	CamelMimeBodyPart * (*get_part) (CamelMultipart *multipart, guint index);
	guint (*get_number) (CamelMultipart *multipart);
	void (*set_parent) (CamelMultipart *multipart, CamelMimePart *parent);
	CamelMimePart * (*get_parent) (CamelMultipart *multipart);
	void (*set_boundary) (CamelMultipart *multipart, gchar *boundary);
	const gchar * (*get_boundary) (CamelMultipart *multipart);

} CamelMultipartClass;


/* Standard Gtk function */
GtkType camel_multipart_get_type (void);


/* public methods */
CamelMultipart *           camel_multipart_new             (void);
void                       camel_multipart_add_part        (CamelMultipart *multipart, 
							    CamelMimeBodyPart *part);
void                       camel_multipart_add_part_at     (CamelMultipart *multipart, 
							    CamelMimeBodyPart *part, 
							    guint index);
void                       camel_multipart_remove_part     (CamelMultipart *multipart, 
							    CamelMimeBodyPart *part);
CamelMimeBodyPart *        camel_multipart_remove_part_at  (CamelMultipart *multipart, 
							    guint index);
CamelMimeBodyPart *        camel_multipart_get_part        (CamelMultipart *multipart, 
							    guint index);
guint                      camel_multipart_get_number      (CamelMultipart *multipart);
void                       camel_multipart_set_parent      (CamelMultipart *multipart, 
							    CamelMimePart *parent);
CamelMimePart *            camel_multipart_get_parent      (CamelMultipart *multipart);
void                       camel_multipart_set_boundary    (CamelMultipart *multipart, 
							    gchar *boundary);
const gchar *              camel_multipart_get_boundary    (CamelMultipart *multipart);





#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_MULTIPART_H */

