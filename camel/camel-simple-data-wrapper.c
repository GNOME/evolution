/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-simple-data-wrapper.c : simple implementation of a data wrapper */
/* store the data in a glib byte array                                   */

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

#include "camel-simple-data-wrapper.h"

static  CamelDataWrapperClass *parent_class=NULL;

/* Returns the class for a CamelDataWrapper */
#define CSDW_CLASS(so) CAMEL_SIMPLE_DATA_WRAPPER_CLASS (GTK_OBJECT(so)->klass)

static void _construct_from_stream (CamelDataWrapper *data_wrapper, CamelStream *stream, guint size);
static void _write_to_stream (CamelDataWrapper *data_wrapper, CamelStream *stream);

static void
camel_simple_data_wrapper_class_init (CamelSimpleDataWrapperClass *camel_simple_data_wrapper_class)
{
	CamelDataWrapperClass *camel_data_wrapper_class = CAMEL_DATA_WRAPPER_CLASS (camel_simple_data_wrapper_class);
	
	parent_class = gtk_type_class (camel_data_wrapper_get_type ());
	/* virtual method definition */

	/* virtual method overload */
	camel_data_wrapper_class->write_to_stream = _write_to_stream;
	camel_data_wrapper_class->construct_from_stream = _construct_from_stream;
}






GtkType
camel_simple_data_wrapper_get_type (void)
{
	static GtkType camel_simple_data_wrapper_type = 0;
	
	if (!camel_simple_data_wrapper_type)	{
		GtkTypeInfo camel_simple_data_wrapper_info =	
		{
			"CamelDataWrapper",
			sizeof (CamelDataWrapper),
			sizeof (CamelDataWrapperClass),
			(GtkClassInitFunc) camel_simple_data_wrapper_class_init,
			(GtkObjectInitFunc) NULL,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		camel_simple_data_wrapper_type = gtk_type_unique (camel_data_wrapper_get_type (), &camel_simple_data_wrapper_info);
	}
	
	return camel_simple_data_wrapper_type;
}



static void
_write_to_stream (CamelDataWrapper *data_wrapper, CamelStream *stream)
{
	CamelSimpleDataWrapper *simple_data_wrapper = CAMEL_SIMPLE_DATA_WRAPPER (data_wrapper);
	GByteArray *array;

	g_assert (data_wrapper);
	g_assert (stream);
	g_assert (simple_data_wrapper->byte_array);

	array = simple_data_wrapper->byte_array;
	if (array->len)
		camel_stream_write (stream, (gchar *)array->data, array->len);

	return;
}




#define _CMSDW_TMP_BUF_SIZE 100
static void
_construct_from_stream (CamelDataWrapper *data_wrapper, CamelStream *stream, guint size)
{
	CamelSimpleDataWrapper *simple_data_wrapper = CAMEL_SIMPLE_DATA_WRAPPER (data_wrapper);
	guint current_index;
	guint nb_bytes_read;
	guint nb_bytes_left;
	static gchar *tmp_buf;
	GByteArray *array;
		

	g_assert (data_wrapper);
	g_assert (stream);
	
	if (!size) return;
	if (!tmp_buf) tmp_buf = g_new (gchar, _CMSDW_TMP_BUF_SIZE);

	array = simple_data_wrapper->byte_array;
	if (array)
		g_byte_array_free (array, FALSE);
	
	array = g_byte_array_new();
	nb_bytes_left = size;
	do {
		
		nb_bytes_read = camel_stream_read (stream, tmp_buf, 
						   MIN (_CMSDW_TMP_BUF_SIZE, nb_bytes_left));
		nb_bytes_left -= nb_bytes_read;
		if (nb_bytes_read) g_byte_array_append (array, tmp_buf,	nb_bytes_read);
		
	} while (nb_bytes_read && (nb_bytes_left>0));
	
}
