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

#include "camel-data-wrapper.h"

static GtkObjectClass *parent_class=NULL;

/* Returns the class for a CamelDataWrapper */
#define CDH_CLASS(so) CAMEL_DATA_WRAPPER_CLASS (GTK_OBJECT(so)->klass)

static void __camel_data_wrapper_write_to_buffer(CamelDataWrapper *data_wrapper, gchar *buffer);
static void __camel_data_wrapper_write_to_file(CamelDataWrapper *data_wrapper, FILE *file);
static void __camel_data_wrapper_construct_from_buffer(CamelDataWrapper *data_wrapper, gchar *buffer, guint size);
static void __camel_data_wrapper_construct_from_file (CamelDataWrapper *data_wrapper, FILE *file, guint size);

static void
camel_data_wrapper_class_init (CamelDataWrapperClass *camel_data_wrapper_class)
{
	parent_class = gtk_type_class (gtk_object_get_type ());
	
	/* virtual method definition */
	camel_data_wrapper_class->write_to_buffer = __camel_data_wrapper_write_to_buffer;
	camel_data_wrapper_class->write_to_file = __camel_data_wrapper_write_to_file;
	camel_data_wrapper_class->construct_from_buffer = __camel_data_wrapper_construct_from_buffer;
	camel_data_wrapper_class->construct_from_file = __camel_data_wrapper_construct_from_file;

	/* virtual method overload */
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
			(GtkObjectInitFunc) NULL,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		camel_data_wrapper_type = gtk_type_unique (gtk_object_get_type (), &camel_data_wrapper_info);
	}
	
	return camel_data_wrapper_type;
}




/**
 * __camel_data_wrapper_write_to_buffer: write data content in a byte buffer
 * @data_wrapper: the data wrapper object
 * @buffer: byte buffer where data will be written 
 * 
 * This method must be overriden by subclasses
 * Data must be written in the bytes buffer
 * in a architecture independant fashion. 
 * If data is a standard data (for example an jpg image)
 * it must be serialized in the buffer exactly as it 
 * would be saved on disk. A simple dump of the buffer in
 * a file should be sufficient for the data to be 
 * re-read by a foreign application.  
 * 
 **/
static void
__camel_data_wrapper_write_to_buffer(CamelDataWrapper *data_wrapper, gchar *buffer)
{
	/* nothing */
}


/**
 * camel_data_wrapper_write_to_buffer: write data in a memory buffer
 * @data_wrapper: the data wrapper object
 * @buffer: byte buffer where data will be written 
 *
 * Write data content in a buffer. Data is stored in a machine
 * independant format. 
 * 
 **/
void 
camel_data_wrapper_write_to_buffer(CamelDataWrapper *data_wrapper, gchar *buffer)
{
	CDH_CLASS(data_wrapper)->write_to_buffer (data_wrapper, buffer);
}



static void
__camel_data_wrapper_write_to_file(CamelDataWrapper *data_wrapper, FILE *file)
{
	/* nothing */
}


/**
 * camel_data_wrapper_write_to_file: write data in a binary file
 * @data_wrapper: the data wrapper object
 * @file: file descriptoe where to write data
 * 
 * Write data content in a binary file.  
 *
 **/
void 
camel_data_wrapper_write_to_file(CamelDataWrapper *data_wrapper, FILE *file)
{
	CDH_CLASS(data_wrapper)->write_to_file (data_wrapper, file);
}


static void
__camel_data_wrapper_construct_from_buffer(CamelDataWrapper *data_wrapper, gchar *buffer, guint size)
{
	/* nothing */
}

void 
camel_data_wrapper_construct_from_buffer(CamelDataWrapper *data_wrapper, gchar *buffer, guint size)
{
	CDH_CLASS(data_wrapper)->construct_from_buffer (data_wrapper, buffer, size);
}



static void
__camel_data_wrapper_construct_from_file (CamelDataWrapper *data_wrapper, FILE *file, guint size)
{
	/* nothing */
}

void 
camel_data_wrapper_construct_from_file (CamelDataWrapper *data_wrapper, FILE *file, guint size)
{
	CDH_CLASS(data_wrapper)->construct_from_file (data_wrapper, file, size);
}



