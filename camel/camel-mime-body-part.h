/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-mime-body-part.h : class for a mime body part */

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


#ifndef CAMEL_MIME_BODY_PART_H
#define CAMEL_MIME_BODY_PART_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <gtk/gtk.h>
#include "camel-types.h"
#include "camel-mime-part.h"


#define CAMEL_MIME_BODY_PART_TYPE     (camel_mime_body_part_get_type ())
#define CAMEL_MIME_BODY_PART(obj)     (GTK_CHECK_CAST((obj), CAMEL_MIME_BODY_PART_TYPE, CamelMimeBodyPart))
#define CAMEL_MIME_BODY_PART_CLASS(k) (GTK_CHECK_CLASS_CAST ((k), CAMEL_MIME_BODY_PART_TYPE, CamelMimeBodyPartClass))
#define CAMEL_IS_MIME_BODY_PART(o)    (GTK_CHECK_TYPE((o), CAMEL_MIME_BODY_PART_TYPE))


struct _CamelMimeBodyPart
{
	CamelMimePart parent_object;
	CamelMultipart *parent;
      
};



typedef struct {
	CamelMimePartClass parent_class;
	
	/* Virtual methods */	
	void (*set_parent) (CamelMimeBodyPart *mime_body_part, 
			    CamelMultipart *multipart);
	const CamelMultipart * (*get_parent) (CamelMimeBodyPart *mime_body_part);

} CamelMimeBodyPartClass;


/* Standard Gtk function */
GtkType camel_mime_body_part_get_type (void);


/* public methods */
CamelMimeBodyPart *   camel_mime_body_part_new (void);
void                  camel_mime_body_part_set_parent (CamelMimeBodyPart *mime_body_part, 
						       CamelMultipart *multipart);
const CamelMultipart *camel_mime_body_part_get_parent (CamelMimeBodyPart *mime_body_part);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_MIME_BODY_PART_H */

