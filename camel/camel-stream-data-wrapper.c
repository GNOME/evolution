/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* camel-stream-data-wrapper.c
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

#include <gtk/gtk.h>

#include "camel-stream-data-wrapper.h"


static CamelDataWrapperClass *parent_class = NULL;


/* CamelDataWrapper methods.  */

static void
write_to_stream (CamelDataWrapper *data_wrapper,
		 CamelStream *output_stream)
{
#define BUFFER_SIZE 4096
	gchar buffer[BUFFER_SIZE];
	CamelStreamDataWrapper *stream_data_wrapper;
	CamelStream *input_stream;

	stream_data_wrapper = CAMEL_STREAM_DATA_WRAPPER (data_wrapper);
	input_stream = stream_data_wrapper->stream;

	while (TRUE) {
		gchar *p;
		gint read, written;

		read = camel_stream_read (input_stream, buffer, BUFFER_SIZE);
		if (read == 0)
			break;

		p = buffer;
		while (read > 0) {
			written = camel_stream_write (output_stream, p, read);

			/* FIXME no way to report an error?!  */
			if (written == -1)
				break;

			p += written;
			read -= written;
		}
	}
#undef BUFFER_SIZE
}

static CamelStream *
get_stream (CamelDataWrapper *data_wrapper)
{
	CamelStreamDataWrapper *stream_data_wrapper;

	stream_data_wrapper = CAMEL_STREAM_DATA_WRAPPER (data_wrapper);
	return stream_data_wrapper->stream;
}


/* GtkObject methods.  */

static void
destroy (GtkObject *object)
{
	CamelStreamDataWrapper *stream_data_wrapper;
	GtkObject *stream_object;

	stream_data_wrapper = CAMEL_STREAM_DATA_WRAPPER (object);

	stream_object = GTK_OBJECT (object);
	stream_data_wrapper->stream = NULL;

	gtk_object_unref (stream_object);
}


/* This handles destruction of the associated CamelDataWrapper outside
   CamelStreamDataWrapper, for debuggin purposes (this should never happen).  */
static void
stream_destroy_cb (GtkObject *object,
		   gpointer data)
{
	CamelStreamDataWrapper *wrapper;

	wrapper = CAMEL_STREAM_DATA_WRAPPER (data);

	/* Hack: when we destroy the stream ourselves, we set the `stream'
           member to NULL first, so that we can recognize when this is done out
           of our control.  */
	if (wrapper->stream != NULL) {
		g_warning ("CamelSimpleDataWrapperStream: associated CamelSimpleDataWrapper was destroyed.");
		wrapper->stream = NULL;
	}
}


static void
class_init (CamelStreamDataWrapperClass *class)
{
	GtkObjectClass *object_class;
	CamelDataWrapperClass *data_wrapper_class;

	object_class = GTK_OBJECT_CLASS (class);
	object_class->destroy = destroy;

	data_wrapper_class = CAMEL_DATA_WRAPPER_CLASS (class);
	data_wrapper_class->write_to_stream = write_to_stream;
	data_wrapper_class->get_stream = get_stream;

	parent_class = gtk_type_class (camel_data_wrapper_get_type ());
}

static void
init (CamelStreamDataWrapper *wrapper)
{
	wrapper->stream = NULL;
}


GtkType
camel_stream_data_wrapper_get_type (void)
{
	static GtkType type = 0;

	if (type == 0) {
		static const GtkTypeInfo info = {
			"CamelStreamDataWrapper",
			sizeof (CamelStreamDataWrapper),
			sizeof (CamelStreamDataWrapperClass),
			(GtkClassInitFunc) class_init,
			(GtkObjectInitFunc) init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		type = gtk_type_unique (camel_data_wrapper_get_type (), &info);
	}

	return type;
}

/**
 * camel_stream_data_wrapper_construct:
 * @wrapper: A CamelStreamDataWrapper object
 * @stream: A Camel stream object
 * 
 * Construct @wrapper associating @stream to it.  Notice that, after this call,
 * @stream is conceptually owned by @wrapper and will be destroyed when
 * @wrapper is destroyed.
 **/
void
camel_stream_data_wrapper_construct (CamelStreamDataWrapper *wrapper,
				     CamelStream *stream)
{
	g_return_if_fail (wrapper != NULL);
	g_return_if_fail (CAMEL_IS_STREAM_DATA_WRAPPER (wrapper));
	g_return_if_fail (stream != NULL);
	g_return_if_fail (CAMEL_IS_STREAM (stream));

	wrapper->stream = stream;
	gtk_signal_connect (GTK_OBJECT (stream), "destroy",
			    GTK_SIGNAL_FUNC (stream_destroy_cb), wrapper);
}

/**
 * camel_stream_data_wrapper_new:
 * @stream: A Camel stream object
 * 
 * Create a new stream data wrapper object for @stream.  Notice that, after
 * this call, @stream is conceptually owned by the new wrapper and will be
 * destroyed when the wrapper is destroyed.
 * 
 * Return value: A pointer to the new CamelStreamDataWrapper object.
 **/
CamelDataWrapper *
camel_stream_data_wrapper_new (CamelStream *stream)
{
	CamelDataWrapper *wrapper;

	wrapper = gtk_type_new (camel_stream_data_wrapper_get_type ());
	camel_stream_data_wrapper_construct
		(CAMEL_STREAM_DATA_WRAPPER (wrapper), stream);

	return wrapper;
}
