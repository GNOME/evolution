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
	GString *content_md5;
	GString *encoding;
	GList *languages;

} CamelMimePart;



typedef struct {
	CamelDataWrapperClass parent_class;
	
	/* Virtual methods */	

} CamelMimePartClass;



/* Standard Gtk function */
GtkType camel_mime_part_get_type (void);


/* public methods */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_MIME_PART_H */
