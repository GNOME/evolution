/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-simple-data-wrapper.c : simple implementation of a data wrapper */
/* store the data in a glib byte array                                   */

/*
 *
 * Authors: Bertrand Guiheneuf <bertrand@helixcode.com>
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

#include "camel-simple-data-wrapper.h"
#include "camel-simple-data-wrapper-stream.h"
#include <camel/camel-stream-mem.h>
#include "camel-mime-utils.h"
#include <camel/camel-mime-filter.h>
#include <camel/camel-mime-filter-basic.h>
#include <camel/camel-mime-filter-charset.h>
#include <camel/camel-stream-filter.h>
#include <camel/camel-seekable-substream.h>

#include <string.h>

#define d(x)

static CamelDataWrapperClass *parent_class = NULL;

/* Returns the class for a CamelDataWrapper */
#define CSDW_CLASS(so) CAMEL_SIMPLE_DATA_WRAPPER_CLASS (GTK_OBJECT (so)->klass)

static void            write_to_stream           (CamelDataWrapper *data_wrapper,
						     CamelStream *stream);
static void            finalize                  (GtkObject *object);
static CamelStream *   get_output_stream         (CamelDataWrapper *data_wrapper);
static void            construct_from_stream     (CamelDataWrapper *dw, CamelStream *stream);

static void
camel_simple_data_wrapper_class_init (CamelSimpleDataWrapperClass *camel_simple_data_wrapper_class)
{
	CamelDataWrapperClass *camel_data_wrapper_class =
		CAMEL_DATA_WRAPPER_CLASS (camel_simple_data_wrapper_class);
	GtkObjectClass *gtk_object_class =
		GTK_OBJECT_CLASS (camel_data_wrapper_class);

	parent_class = gtk_type_class (camel_data_wrapper_get_type ());


	/* virtual method overload */
	camel_data_wrapper_class->write_to_stream = write_to_stream;
	camel_data_wrapper_class->get_output_stream = get_output_stream;
	camel_data_wrapper_class->construct_from_stream = construct_from_stream;

	gtk_object_class->finalize = finalize;
}


static void
camel_simple_data_wrapper_init (CamelSimpleDataWrapper *wrapper)
{
	wrapper->byte_array = NULL;
	wrapper->has_byte_array_stream = FALSE;
}


GtkType
camel_simple_data_wrapper_get_type (void)
{
	static GtkType camel_simple_data_wrapper_type = 0;

	if (!camel_simple_data_wrapper_type) {
		GtkTypeInfo camel_simple_data_wrapper_info =
		{
			"CamelSimpleDataWrapper",
			sizeof (CamelSimpleDataWrapper),
			sizeof (CamelSimpleDataWrapperClass),
			(GtkClassInitFunc) camel_simple_data_wrapper_class_init,
			(GtkObjectInitFunc) camel_simple_data_wrapper_init,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		camel_simple_data_wrapper_type =
			gtk_type_unique (camel_data_wrapper_get_type (),
					 &camel_simple_data_wrapper_info);
	}

	return camel_simple_data_wrapper_type;
}


static void
finalize (GtkObject *object)
{
	CamelSimpleDataWrapper *simple_data_wrapper =
		CAMEL_SIMPLE_DATA_WRAPPER (object);

	if (simple_data_wrapper->byte_array)
		g_byte_array_free (simple_data_wrapper->byte_array, TRUE);

	GTK_OBJECT_CLASS (parent_class)->finalize (object);
}


/**
 * camel_simple_data_wrapper_new:
 *
 * Return value: a new CamelSimpleDataWrapper object
 **/
CamelSimpleDataWrapper *
camel_simple_data_wrapper_new (void)
{
	return (CamelSimpleDataWrapper *)
		gtk_type_new (CAMEL_SIMPLE_DATA_WRAPPER_TYPE);
}


static void
write_to_stream (CamelDataWrapper *data_wrapper, CamelStream *stream)
{
	CamelSimpleDataWrapper *simple_data_wrapper =
		CAMEL_SIMPLE_DATA_WRAPPER (data_wrapper);
	GByteArray *array;

	array = simple_data_wrapper->byte_array;
	if ( array && array->len)
		camel_stream_write (stream, (gchar *)array->data, array->len);
	else
		parent_class->write_to_stream (data_wrapper, stream);
}

/**
 * camel_simple_data_wrapper_set_text: set some text as data wrapper content
 * @simple_data_wrapper: SimpleDataWrapper object
 * @text: the text to use
 *
 * Utility routine used to set up the content of a SimpleDataWrapper object
 * to be a character string.
 **/
void
camel_simple_data_wrapper_set_text (CamelSimpleDataWrapper *simple_data_wrapper, const gchar *text)
{
	GByteArray *array;

	array = simple_data_wrapper->byte_array;
	if (array)
		g_byte_array_free (array, FALSE);

	array = g_byte_array_new ();
	simple_data_wrapper->byte_array = array;

	g_byte_array_append (array, text, strlen (text));
}


static CamelStream *
get_output_stream (CamelDataWrapper *data_wrapper)
{
	CamelSimpleDataWrapper *simple_data_wrapper;
	CamelStream *output_stream = NULL;

	simple_data_wrapper = CAMEL_SIMPLE_DATA_WRAPPER (data_wrapper);

	if (simple_data_wrapper->byte_array &&
	    !(simple_data_wrapper->has_byte_array_stream)) {
		output_stream = camel_simple_data_wrapper_stream_new (simple_data_wrapper);
		camel_data_wrapper_set_output_stream (data_wrapper, output_stream);
	}

	return parent_class->get_output_stream (data_wrapper);
}

static void
construct_from_stream(CamelDataWrapper *dw, CamelStream *stream)
{
	camel_data_wrapper_set_output_stream (dw, stream);
}

