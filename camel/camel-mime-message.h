/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camelMimeMessage.h : class for a mime message */

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
 * MERCHANTABILITY or FITNESS FOR A MESSAGEICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */


#ifndef CAMEL_MIME_MESSAGE_H
#define CAMEL_MIME_MESSAGE_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <gtk/gtk.h>
#include "camel-mime-part.h"



#define CAMEL_MIME_MESSAGE_TYPE     (camel_mime_message_get_type ())
#define CAMEL_MIME_MESSAGE(obj)     (GTK_CHECK_CAST((obj), CAMEL_MIME_MESSAGE_TYPE, CamelMimeMessage))
#define CAMEL_MIME_MESSAGE_CLASS(k) (GTK_CHECK_CLASS_CAST ((k), CAMEL_MIME_MESSAGE_TYPE, CamelMimeMessageClass))
#define IS_CAMEL_MIME_MESSAGE(o)    (GTK_CHECK_TYPE((o), CAMEL_MIME_MESSAGE_TYPE))



typedef struct 
{

} CamelMimeMessage;



typedef struct {
	CamelDataWrapperClass parent_class;
	
	/* Virtual methods */	
} CamelMimeMessageClass;



/* Standard Gtk function */
GtkType camel_mime_message_get_type (void);


/* public methods */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_MIME_MESSAGE_H */
