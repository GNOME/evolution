/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* camel-simple-data-wrapper-stream.c
 *
 * Copyright 1999, 2000 HelixCode (http://www.helixcode.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ettore Perazzoli
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "camel-simple-data-wrapper-stream.h"


static CamelStreamClass *parent_class = NULL;


/* CamelStream methods.  */

static	gint 
read (CamelStream *stream,
      gchar *buffer,
      gint n)
{
	CamelSimpleDataWrapperStream *wrapper_stream;
	CamelSimpleDataWrapper *wrapper;
	GByteArray *array;
	gint len;

	wrapper_stream = CAMEL_SIMPLE_DATA_WRAPPER_STREAM (stream);
	wrapper = wrapper_stream->wrapper;
	g_return_val_if_fail (wrapper != NULL, -1);
	array = wrapper->byte_array;

	len = MIN (n, array->len - wrapper_stream->current_position);
	if (len > 0) {
		memcpy (buffer, array->data, len);
		wrapper_stream->current_position += len;
		return len;
	} else {
		return 0;
	}
}

static	gint 
write (CamelStream *stream,
       const gchar *buffer,
       gint n)
{
	CamelSimpleDataWrapperStream *wrapper_stream;
	CamelSimpleDataWrapper *wrapper;
	GByteArray *array;
	gint len;
	const gchar *buffer_next;
	gint left;

	wrapper_stream = CAMEL_SIMPLE_DATA_WRAPPER_STREAM (stream);
	wrapper = wrapper_stream->wrapper;
	g_return_val_if_fail (wrapper != NULL, -1);
	array = wrapper->byte_array;

	len = MIN (n, array->len - wrapper_stream->current_position);
	if (len > 0) {
		memcpy (array->data, buffer, len);
		buffer_next = buffer + len;
		left = n - len;
	} else {
		/* If we are past the end of the array, fill with zeros.  */
		if (wrapper_stream->current_position > array->len) {
			gint saved_length;

			saved_length = array->len;
			g_byte_array_set_size
				(array, wrapper_stream->current_position);
			memset (array->data + saved_length,
				0,
				(wrapper_stream->current_position
				 - saved_length));
		}

		buffer_next = buffer;
		left = n;
	}

	if (n > 0)
		g_byte_array_append (array, buffer_next, left);

	wrapper_stream->current_position += n;
	return n;
}

static void 
flush (CamelStream *stream)
{
	/* No op, as we don't do any buffering.  */
}

static gint 
available (CamelStream *stream)
{
	CamelSimpleDataWrapperStream *wrapper_stream;
	CamelSimpleDataWrapper *wrapper;
	GByteArray *array;
	gint available;

	wrapper_stream = CAMEL_SIMPLE_DATA_WRAPPER_STREAM (stream);
	wrapper = wrapper_stream->wrapper;
	g_return_val_if_fail (wrapper != NULL, -1);
	array = wrapper->byte_array;

	available = array->len - wrapper_stream->current_position;
	return MAX (available, 0);
}

static gboolean
eos (CamelStream *stream)
{
	if (available (stream) > 0)
		return TRUE;
	else
		return FALSE;
}

static void 
close (CamelStream *stream)
{
	/* Nothing to do, we have no associated file descriptor.  */
}

static gint
seek (CamelSeekableStream *stream,
      gint offset,
      CamelStreamSeekPolicy policy)
{
	CamelSimpleDataWrapperStream *wrapper_stream;
	gint new_position;

	wrapper_stream = CAMEL_SIMPLE_DATA_WRAPPER_STREAM (stream);

	switch (policy) {
	case CAMEL_STREAM_SET:
		new_position = offset;
		break;
	case CAMEL_STREAM_CUR:
		new_position = wrapper_stream->current_position + offset;
		break;
	case CAMEL_STREAM_END:
		new_position = wrapper_stream->wrapper->byte_array->len;
		break;
	default:
		g_warning ("Unknown CamelStreamSeekPolicy %d.", policy);
		return -1;
	}

	wrapper_stream->current_position = new_position;
	return new_position;
}


/* This handles destruction of the associated CamelDataWrapper.  */
/* Hm, this should never happen though, because we gtk_object_ref() the
   wrapper.  */
static void
wrapper_destroy_cb (GtkObject *object,
		    gpointer data)
{
	CamelSimpleDataWrapperStream *stream;

	g_warning ("CamelSimpleDataWrapperStream: associated CamelSimpleDataWrapper was destroyed.");
	stream = CAMEL_SIMPLE_DATA_WRAPPER_STREAM (object);
	stream->wrapper = NULL;
}


/* GtkObject methods.  */

static void
destroy (GtkObject *object)
{
	CamelSimpleDataWrapperStream *stream;

	stream = CAMEL_SIMPLE_DATA_WRAPPER_STREAM (object);

	gtk_object_unref (GTK_OBJECT (stream->wrapper));

	if (GTK_OBJECT_CLASS (parent_class)->destroy != NULL)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
class_init (CamelSimpleDataWrapperStreamClass *klass)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass*) klass;

	parent_class = gtk_type_class (camel_stream_get_type ());
}

static void
init (CamelSimpleDataWrapperStream *simple_data_wrapper_stream)
{
	simple_data_wrapper_stream->current_position = 0;
}


GtkType
camel_simple_data_wrapper_stream_get_type (void)
{
	static GtkType type = 0;

	if (type == 0) {
		static const GtkTypeInfo info = {
			"CamelSimpleDataWrapperStream",
			sizeof (CamelSimpleDataWrapperStream),
			sizeof (CamelSimpleDataWrapperStreamClass),
			(GtkClassInitFunc) class_init,
			(GtkObjectInitFunc) init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		type = gtk_type_unique (camel_stream_get_type (), &info);
	}

	return type;
}

void
camel_simple_data_wrapper_stream_construct (CamelSimpleDataWrapperStream *stream,
					    CamelSimpleDataWrapper *wrapper)
{
	g_return_if_fail (stream != NULL);
	g_return_if_fail (CAMEL_IS_SIMPLE_DATA_WRAPPER_STREAM (stream));
	g_return_if_fail (wrapper != NULL);
	g_return_if_fail (CAMEL_IS_SIMPLE_DATA_WRAPPER (wrapper));

	gtk_object_ref (GTK_OBJECT (wrapper));
	stream->wrapper = wrapper;
	gtk_signal_connect (GTK_OBJECT (wrapper), "destroy",
			    wrapper_destroy_cb, stream);
}

CamelStream *
camel_simple_data_wrapper_stream_new (CamelSimpleDataWrapper *wrapper)
{
	CamelStream *stream;

	g_return_val_if_fail (wrapper != NULL, NULL);
	g_return_val_if_fail (CAMEL_IS_SIMPLE_DATA_WRAPPER (wrapper), NULL);

	stream = gtk_type_new (camel_simple_data_wrapper_stream_get_type ());

	camel_simple_data_wrapper_stream_construct
		(CAMEL_SIMPLE_DATA_WRAPPER_STREAM (stream), wrapper);

	return stream;
}
