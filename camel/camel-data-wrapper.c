/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camelDataWrapper.c : Abstract class for a data_wrapper */

/** THIS IS MOSTLY AN ABSTRACT CLASS THAT SHOULD HAVE BEEN AN
    INTERFACE. **/

/* 
 *
 * Author : 
 *  Bertrand Guiheneuf <bertrand@helixcode.com>
 *
 * Copyright 1999, 2000 Helix Code, Inc. (http://www.helixcode.com)
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
#include "camel-log.h"

static GtkObjectClass *parent_class=NULL;

/* Returns the class for a CamelDataWrapper */
#define CDW_CLASS(so) CAMEL_DATA_WRAPPER_CLASS (GTK_OBJECT(so)->klass)


static void my_set_input_stream (CamelDataWrapper *data_wrapper, CamelStream *stream);
static CamelStream *my_get_input_stream (CamelDataWrapper *data_wrapper);
static void my_set_output_stream (CamelDataWrapper *data_wrapper, CamelStream *stream);
static CamelStream *my_get_output_stream (CamelDataWrapper *data_wrapper);

static void my_construct_from_stream (CamelDataWrapper *data_wrapper, CamelStream *stream);
static void my_write_to_stream (CamelDataWrapper *data_wrapper, CamelStream *stream);
static void my_set_mime_type (CamelDataWrapper *data_wrapper, const gchar *mime_type);
static gchar *my_get_mime_type (CamelDataWrapper *data_wrapper);
static GMimeContentField *my_get_mime_type_field (CamelDataWrapper *data_wrapper);
static void my_set_mime_type_field (CamelDataWrapper *data_wrapper, GMimeContentField *mime_type);
static CamelStream *my_get_stream (CamelDataWrapper *data_wrapper);
static void my_finalize (GtkObject *object);

static void
camel_data_wrapper_class_init (CamelDataWrapperClass *camel_data_wrapper_class)
{
	GtkObjectClass *gtk_object_class = GTK_OBJECT_CLASS (camel_data_wrapper_class);

	parent_class = gtk_type_class (gtk_object_get_type ());
	
	/* virtual method definition */
	camel_data_wrapper_class->write_to_stream = my_write_to_stream;
	camel_data_wrapper_class->construct_from_stream = my_construct_from_stream;
	camel_data_wrapper_class->set_mime_type = my_set_mime_type;
	camel_data_wrapper_class->get_mime_type = my_get_mime_type;
	camel_data_wrapper_class->get_mime_type_field = my_get_mime_type_field;
	camel_data_wrapper_class->set_mime_type_field = my_set_mime_type_field;
	camel_data_wrapper_class->get_stream = my_get_stream;

	camel_data_wrapper_class->set_input_stream = my_set_input_stream;
	camel_data_wrapper_class->get_input_stream = my_get_input_stream;
	camel_data_wrapper_class->set_output_stream = my_set_output_stream;
	camel_data_wrapper_class->get_output_stream = my_get_output_stream;

	/* virtual method overload */
	gtk_object_class->finalize = my_finalize;
}





static void
camel_data_wrapper_init (gpointer   object,  gpointer   klass)
{
	CamelDataWrapper *camel_data_wrapper = CAMEL_DATA_WRAPPER (object);

	CAMEL_LOG_FULL_DEBUG ( "camel_data_wrapper_init:: Entering\n");
	camel_data_wrapper->mime_type = gmime_content_field_new (NULL, NULL);
	CAMEL_LOG_FULL_DEBUG ( "camel_data_wrapper_init:: Leaving\n");
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


static void           
my_finalize (GtkObject *object)
{
	CamelDataWrapper *camel_data_wrapper = CAMEL_DATA_WRAPPER (object);

	CAMEL_LOG_FULL_DEBUG ("Entering CamelDataWrapper::finalize\n");
	CAMEL_LOG_FULL_DEBUG  ("CamelDataWrapper::finalize, finalizing object %p\n", object);
	if (camel_data_wrapper->mime_type)
		gmime_content_field_unref (camel_data_wrapper->mime_type);

	if (camel_data_wrapper->input_stream)
		gtk_object_unref (GTK_OBJECT (camel_data_wrapper->input_stream));

	if (camel_data_wrapper->output_stream)
		gtk_object_unref (GTK_OBJECT (camel_data_wrapper->output_stream));

	parent_class->finalize (object);
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelDataWrapper::finalize\n");
}






static void 
my_set_input_stream (CamelDataWrapper *data_wrapper, CamelStream *stream)
{
	g_assert (data_wrapper);

	if (data_wrapper->input_stream)
		gtk_object_unref (GTK_OBJECT (data_wrapper->input_stream));

	data_wrapper->input_stream = stream;

	if (!data_wrapper->output_stream && stream)
		my_set_output_stream (data_wrapper, stream);
		
	if (stream)
		gtk_object_ref (GTK_OBJECT (stream));
}


void 
camel_data_wrapper_set_input_stream (CamelDataWrapper *data_wrapper, CamelStream *stream)
{
	g_assert (data_wrapper);
	CDW_CLASS(data_wrapper)->set_input_stream (data_wrapper, stream);
}





static CamelStream * 
my_get_input_stream (CamelDataWrapper *data_wrapper)
{
	g_assert (data_wrapper);
	return (data_wrapper->input_stream);
}

CamelStream * 
camel_data_wrapper_get_input_stream (CamelDataWrapper *data_wrapper)
{
	g_assert (data_wrapper);
	return CDW_CLASS(data_wrapper)->get_input_stream (data_wrapper);
}




static void 
my_set_output_stream (CamelDataWrapper *data_wrapper, CamelStream *stream)
{
	g_assert (data_wrapper);
	
	if (data_wrapper->output_stream)
		gtk_object_unref (GTK_OBJECT (data_wrapper->output_stream));

	data_wrapper->output_stream = stream;
	if (stream)
		gtk_object_ref (GTK_OBJECT (stream));
}

void 
camel_data_wrapper_set_output_stream (CamelDataWrapper *data_wrapper, CamelStream *stream)
{
	g_assert (data_wrapper);
	CDW_CLASS(data_wrapper)->set_output_stream (data_wrapper, stream);
}




static CamelStream * 
my_get_output_stream (CamelDataWrapper *data_wrapper)
{
	g_assert (data_wrapper);
	return (data_wrapper->output_stream);
}

CamelStream * 
camel_data_wrapper_get_output_stream (CamelDataWrapper *data_wrapper)
{
	g_assert (data_wrapper);
	return CDW_CLASS(data_wrapper)->get_output_stream (data_wrapper);
}





/**
 * my_write_to_stream: write data content in a byte stream
 * @data_wrapper: the data wrapper object
 * @stream byte stream where data will be written 
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
my_write_to_stream (CamelDataWrapper *data_wrapper, CamelStream *stream)
{
	gchar tmp_buf[4096];
	gint nb_read;
	gint nb_written;
	CamelStream *output_stream;

	/* 
	 * default implementation that uses the input 
	 * stream and stream it in a blocking way.
	 */
	g_assert (data_wrapper);
	g_assert (stream);
	
	output_stream = camel_data_wrapper_get_output_stream (data_wrapper);

	if (!output_stream) 
		return;

	camel_stream_reset (output_stream);

	while (!camel_stream_eos (output_stream)) {
		nb_read = camel_stream_read (output_stream, tmp_buf, 4096);
		nb_written = 0;
		while (nb_written < nb_read) 
			nb_written += camel_stream_write (stream, tmp_buf + nb_written, nb_read - nb_written);
	}

	CAMEL_LOG_FULL_DEBUG ("CamelDataWrapper::write_to_stream, nmumber of bytes written : %d\n", nb_written);

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
	CAMEL_LOG_FULL_DEBUG ( "camel_data_wrapper_write_to_stream:: Entering\n");
	CDW_CLASS(data_wrapper)->write_to_stream (data_wrapper, stream);
	CAMEL_LOG_FULL_DEBUG ( "camel_data_wrapper_write_to_stream:: Leaving\n");
}






static void
my_construct_from_stream (CamelDataWrapper *data_wrapper, CamelStream *stream)
{
	/* nothing */
}

void 
camel_data_wrapper_construct_from_stream (CamelDataWrapper *data_wrapper, CamelStream *stream)
{
	CAMEL_LOG_FULL_DEBUG ( "camel_data_wrapper_construct_from_stream:: Entering\n");
	CDW_CLASS(data_wrapper)->construct_from_stream (data_wrapper, stream);
	CAMEL_LOG_FULL_DEBUG ( "camel_data_wrapper_construct_from_stream:: Leaving\n");
}



static void
my_set_mime_type (CamelDataWrapper *data_wrapper, const gchar *mime_type)
{
	CAMEL_LOG_FULL_DEBUG ( "CamelDataWrapper::set_mime_type Entering\n");
	g_assert (mime_type);
	gmime_content_field_construct_from_string (data_wrapper->mime_type, mime_type);
	CAMEL_LOG_FULL_DEBUG ( "CamelDataWrapper::set_mime_type Leaving\n");
}

void 
camel_data_wrapper_set_mime_type (CamelDataWrapper *data_wrapper, const gchar *mime_type)
{
	CDW_CLASS(data_wrapper)->set_mime_type (data_wrapper, mime_type);
}

static gchar *
my_get_mime_type (CamelDataWrapper *data_wrapper)
{
	gchar *mime_type;

	mime_type = gmime_content_field_get_mime_type (data_wrapper->mime_type);
	return mime_type;
}

gchar *
camel_data_wrapper_get_mime_type (CamelDataWrapper *data_wrapper)
{
	CAMEL_LOG_FULL_DEBUG ( "camel_data_wrapper_get_mime_type:: Entering before returning\n");
	return CDW_CLASS(data_wrapper)->get_mime_type (data_wrapper);
}


static GMimeContentField *
my_get_mime_type_field (CamelDataWrapper *data_wrapper)
{
	return data_wrapper->mime_type;
}

GMimeContentField *
camel_data_wrapper_get_mime_type_field (CamelDataWrapper *data_wrapper)
{
	return CDW_CLASS(data_wrapper)->get_mime_type_field (data_wrapper);
}


static void
my_set_mime_type_field (CamelDataWrapper *data_wrapper, GMimeContentField *mime_type)
{
	if (data_wrapper->mime_type) gmime_content_field_unref (data_wrapper->mime_type);
	data_wrapper->mime_type = mime_type;
}

void
camel_data_wrapper_set_mime_type_field (CamelDataWrapper *data_wrapper, GMimeContentField *mime_type)
{
	CDW_CLASS(data_wrapper)->set_mime_type_field (data_wrapper, mime_type);
}

static CamelStream *
my_get_stream (CamelDataWrapper *data_wrapper)
{
	/* This needs to be implemented in subclasses.  */
	return NULL;
}

CamelStream *
camel_data_wrapper_get_stream (CamelDataWrapper *data_wrapper)
{
	return CDW_CLASS(data_wrapper)->get_stream (data_wrapper);
}
