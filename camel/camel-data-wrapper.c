/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camelDataWrapper.c : Abstract class for a data_wrapper */

/** THIS IS MOSTLY AN ABSTRACT CLASS THAT SHOULD HAVE BEEN AN
    INTERFACE. **/

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
#include <config.h>
#include "camel-data-wrapper.h"

static GtkObjectClass *parent_class=NULL;

/* Returns the class for a CamelDataWrapper */
#define CDW_CLASS(so) CAMEL_DATA_WRAPPER_CLASS (GTK_OBJECT(so)->klass)

static void _construct_from_stream (CamelDataWrapper *data_wrapper, CamelStream *stream);
static void _write_to_stream (CamelDataWrapper *data_wrapper, CamelStream *stream);
static void _set_mime_type (CamelDataWrapper *data_wrapper, GString *mime_type);
static GString *_get_mime_type (CamelDataWrapper *data_wrapper);

static void
camel_data_wrapper_class_init (CamelDataWrapperClass *camel_data_wrapper_class)
{
	parent_class = gtk_type_class (gtk_object_get_type ());
	
	/* virtual method definition */
	camel_data_wrapper_class->write_to_stream = _write_to_stream;
	camel_data_wrapper_class->construct_from_stream = _construct_from_stream;
	camel_data_wrapper_class->set_mime_type = _set_mime_type;
	camel_data_wrapper_class->get_mime_type = _get_mime_type;

	/* virtual method overload */
}





static void
camel_data_wrapper_init (gpointer   object,  gpointer   klass)
{
	CamelDataWrapper *camel_data_wrapper = CAMEL_DATA_WRAPPER (object);

	camel_data_wrapper->mime_type = gmime_content_field_new (NULL, NULL);
}



GtkType
camel_data_wrapper_get_type (void)
{
	static GtkType camel_data_wrapper_type = 0;
	
	if (!camel_data_wrapper_type)	{
		GtkTypeInfo camel_data_wrapper_info =	
		{
			"CamelDataWrapper",
			sizeof (CamelDataWrapper),
			sizeof (CamelDataWrapperClass),
			(GtkClassInitFunc) camel_data_wrapper_class_init,
			(GtkObjectInitFunc) camel_data_wrapper_init,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		camel_data_wrapper_type = gtk_type_unique (gtk_object_get_type (), &camel_data_wrapper_info);
	}
	
	return camel_data_wrapper_type;
}



/**
 * _write_to_stream: write data content in a byte stream
 * @data_wrapper: the data wrapper object
 * @stre:m byte stream where data will be written 
 * 
 * This method must be overriden by subclasses
 * Data must be written in the bytes stream
 * in a architecture independant fashion. 
 * If data is a standard data (for example an jpg image)
 * it must be serialized in the strea exactly as it 
 * would be saved on disk. A simple dump of the stream in
 * a file should be sufficient for the data to be 
 * re-read by a foreign application.  
 * 
 **/
static void
_write_to_stream (CamelDataWrapper *data_wrapper, CamelStream *stream)
{
	/* nothing */
}


/**
 * camel_data_wrapper_write_to_stream: write data in a stream
 * @data_wrapper: the data wrapper object
 * @stream: byte stream where data will be written 
 *
 * Write data content in a stream. Data is stored in a machine
 * independant format. 
 * 
 **/
void 
camel_data_wrapper_write_to_stream (CamelDataWrapper *data_wrapper, CamelStream *stream)
{
	CDW_CLASS(data_wrapper)->write_to_stream (data_wrapper, stream);
}






static void
_construct_from_stream (CamelDataWrapper *data_wrapper, CamelStream *stream)
{
	/* nothing */
}

void 
camel_data_wrapper_construct_from_stream (CamelDataWrapper *data_wrapper, CamelStream *stream)
{
	CDW_CLASS(data_wrapper)->construct_from_stream (data_wrapper, stream);
}



static void
_set_mime_type (CamelDataWrapper *data_wrapper, GString *mime_type)
{
	g_assert (mime_type);
	gmime_content_field_construct_from_string (data_wrapper->mime_type, mime_type);
}

void 
camel_data_wrapper_set_mime_type (CamelDataWrapper *data_wrapper, GString *mime_type)
{
	CDW_CLASS(data_wrapper)->set_mime_type (data_wrapper, mime_type);
}

static GString *
_get_mime_type (CamelDataWrapper *data_wrapper)
{
	GString *mime_type;

	mime_type = gmime_content_field_get_mime_type (data_wrapper->mime_type);
	return mime_type;
}

static GString *
camel_data_wrapper_get_mime_type (CamelDataWrapper *data_wrapper)
{
	return CDW_CLASS(data_wrapper)->get_mime_type (data_wrapper);
}
