/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camelMimeMessage.c : Abstract class for a mime_message */


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

#include "camel-mime-message.h"
#include <stdio.h>
#include "gmime-content-field.h"



static CamelMimePartClass *parent_class=NULL;

/* Returns the class for a CamelMimeMessage */
#define CMP_CLASS(so) CAMEL_MIME_MESSAGE_CLASS (GTK_OBJECT(so)->klass)



static void
camel_mime_message_class_init (CamelMimeMessageClass *camel_mime_message_class)
{
	parent_class = gtk_type_class (camel_mime_part_get_type ());
	
	/* virtual method definition */

	/* virtual method overload */
}







GtkType
camel_mime_message_get_type (void)
{
	static GtkType camel_mime_message_type = 0;
	
	if (!camel_mime_message_type)	{
		GtkTypeInfo camel_mime_message_info =	
		{
			"CamelMimeMessage",
			sizeof (CamelMimeMessage),
			sizeof (CamelMimeMessageClass),
			(GtkClassInitFunc) camel_mime_message_class_init,
			(GtkObjectInitFunc) NULL,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		camel_mime_message_type = gtk_type_unique (camel_mime_part_get_type (), &camel_mime_message_info);
	}
	
	return camel_mime_message_type;
}


