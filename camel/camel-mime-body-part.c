/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-mime-body-part.c : Abstract class for a mime body part */


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
#include <config.h>
#include "camel-mime-body-part.h"
#include "camel-log.h"


static void _set_parent (CamelMimeBodyPart *mime_body_part, CamelMultipart *multipart);
static const CamelMultipart *_get_parent (CamelMimeBodyPart *mime_body_part);


static CamelMimePartClass *parent_class=NULL;

/* Returns the class for a CamelMimeBodyPart */
#define CMBP_CLASS(so) CAMEL_MIME_BODY_PART_CLASS (GTK_OBJECT(so)->klass)



static void
camel_mime_body_part_class_init (CamelMimeBodyPartClass *camel_mime_body_part_class)
{
	CamelMimePartClass *camel_mime_part_class = CAMEL_MIME_PART_CLASS (camel_mime_body_part_class);
	parent_class = gtk_type_class (camel_mime_part_get_type ());
		
	/* virtual method definition */
	camel_mime_body_part_class->set_parent = _set_parent;
	camel_mime_body_part_class->get_parent = _get_parent;
}

static void
camel_mime_body_part_init (gpointer   object,  gpointer   klass)
{
	CamelMimeBodyPart *camel_mime_body_part = CAMEL_MIME_BODY_PART (object);
}




GtkType
camel_mime_body_part_get_type (void)
{
	static GtkType camel_mime_body_part_type = 0;
	
	if (!camel_mime_body_part_type)	{
		GtkTypeInfo camel_mime_body_part_info =	
		{
			"CamelMimeBodyPart",
			sizeof (CamelMimeBodyPart),
			sizeof (CamelMimeBodyPartClass),
			(GtkClassInitFunc) camel_mime_body_part_class_init,
			(GtkObjectInitFunc) camel_mime_body_part_init,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		camel_mime_body_part_type = gtk_type_unique (camel_mime_part_get_type (), &camel_mime_body_part_info);
	}
	
	return camel_mime_body_part_type;
}

CamelMimeBodyPart *
camel_mime_body_part_new ()
{
	CamelMimeBodyPart *mime_body_part;
	CAMEL_LOG_FULL_DEBUG ("CamelMimeBodyPart:: Entering new()\n");
	
	mime_body_part = (CamelMimeBodyPart *)gtk_type_new (CAMEL_MIME_BODY_PART_TYPE);
	CAMEL_LOG_FULL_DEBUG ("CamelMimeBodyPart:: Leaving new()\n");
	return mime_body_part;
}


static void 
_set_parent (CamelMimeBodyPart *mime_body_part, CamelMultipart *multipart)
{
	if (mime_body_part->parent) gtk_object_unref (GTK_OBJECT (mime_body_part->parent));
	mime_body_part->parent = multipart;
	if (multipart) gtk_object_ref (GTK_OBJECT (multipart));
}


void 
camel_mime_body_part_set_parent (CamelMimeBodyPart *mime_body_part, CamelMultipart *multipart)
{
	CMBP_CLASS (mime_body_part)->set_parent (mime_body_part, multipart);
}


static const CamelMultipart *
_get_parent (CamelMimeBodyPart *mime_body_part)
{
	return mime_body_part->parent;
}


const CamelMultipart *
camel_mime_body_part_get_parent (CamelMimeBodyPart *mime_body_part)
{
	return CMBP_CLASS (mime_body_part)->get_parent (mime_body_part);
}


