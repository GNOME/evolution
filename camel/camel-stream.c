/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-stream.c : abstract class for a stream */

/*
 * Author:
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
#include "camel-stream.h"

static CamelObjectClass *parent_class = NULL;

/* Returns the class for a CamelStream */
#define CS_CLASS(so) CAMEL_STREAM_CLASS (GTK_OBJECT(so)->klass)

static void stream_flush   (CamelStream *stream, CamelException *ex);
static gboolean stream_eos (CamelStream *stream);


static void
camel_stream_class_init (CamelStreamClass *camel_stream_class)
{
	parent_class = gtk_type_class (camel_object_get_type ());

	/* virtual method definition */
	camel_stream_class->flush = stream_flush;
	camel_stream_class->eos = stream_eos;
}

GtkType
camel_stream_get_type (void)
{
	static GtkType camel_stream_type = 0;

	if (!camel_stream_type)	{
		GtkTypeInfo camel_stream_info =
		{
			"CamelStream",
			sizeof (CamelStream),
			sizeof (CamelStreamClass),
			(GtkClassInitFunc) camel_stream_class_init,
			(GtkObjectInitFunc) NULL,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		camel_stream_type = gtk_type_unique (camel_object_get_type (),
						     &camel_stream_info);
	}

	return camel_stream_type;
}

/**
 * camel_stream_read:
 * @stream: a CamelStream.
 * @buffer: buffer where bytes pulled from the stream are stored.
 * @n: max number of bytes to read.
 * @ex: a CamelException
 *
 * Read at most @n bytes from the @stream object and stores them
 * in the buffer pointed at by @buffer.
 *
 * Return value: number of bytes actually read. If an error occurs,
 * @ex will contain a description of the error.
 **/
int
camel_stream_read (CamelStream *stream, char *buffer, unsigned int n,
		   CamelException *ex)
{
	g_return_val_if_fail (CAMEL_IS_STREAM (stream), -1);
	g_return_val_if_fail (n == 0 || buffer, -1);

	return CS_CLASS (stream)->read (stream, buffer, n, ex);
}

/**
 * camel_stream_write:
 * @stream: a CamelStream object.
 * @buffer: buffer to write.
 * @n: number of bytes to write
 * @ex: a CamelException
 *
 * Write @n bytes from the buffer pointed at by @buffer into @stream.
 *
 * Return value: the number of bytes actually written to the stream. If
 * an error occurs, @ex will contain a description of the error.
 **/
int
camel_stream_write (CamelStream *stream, const char *buffer, unsigned int n,
		    CamelException *ex)
{
	g_return_val_if_fail (CAMEL_IS_STREAM (stream), -1);
	g_return_val_if_fail (n == 0 || buffer, -1);

	return CS_CLASS (stream)->write (stream, buffer, n, ex);
}


static void
stream_flush (CamelStream *stream, CamelException *ex)
{
	/* nothing */
}

/**
 * camel_stream_flush:
 * @stream: a CamelStream object
 * @ex: a CamelException
 *
 * Flushes the contents of the stream to its backing store. Only meaningful
 * on writable streams. If an error occurs, @ex will be set.
 **/
void
camel_stream_flush (CamelStream *stream, CamelException *ex)
{
	g_return_if_fail (CAMEL_IS_STREAM (stream));

	CS_CLASS (stream)->flush (stream, ex);
}


static gboolean
stream_eos (CamelStream *stream)
{
	return stream->eos;
}

/**
 * camel_stream_eos:
 * @stream: a CamelStream object
 *
 * Test if there are bytes left to read on the @stream object.
 *
 * Return value: %TRUE if all the contents on the stream has been read, or
 * %FALSE if information is still available.
 **/
gboolean
camel_stream_eos (CamelStream *stream)
{
	g_return_val_if_fail (CAMEL_IS_STREAM (stream), TRUE);

	return CS_CLASS (stream)->eos (stream);
}


/**
 * camel_stream_reset: reset a stream
 * @stream: the stream object
 * @ex: a CamelException
 *
 * Reset a stream. That is, put it in a state where it can be read
 * from the beginning again. Not all streams in Camel are seekable,
 * but they must all be resettable.
 **/
void
camel_stream_reset (CamelStream *stream, CamelException *ex)
{
	g_return_if_fail (CAMEL_IS_STREAM (stream));

	CS_CLASS (stream)->reset (stream, ex);
}

/***************** Utility functions ********************/

/**
 * camel_stream_write_string:
 * @stream: a stream object
 * @string: a string
 * @ex: a CamelException
 *
 * Writes the string to the stream.
 *
 * Return value: the number of characters output.
 **/
int
camel_stream_write_string (CamelStream *stream, const char *string,
			   CamelException *ex)
{
	return camel_stream_write (stream, string, strlen (string), ex);
}

/**
 * camel_stream_printf:
 * @stream: a stream object
 * @ex: a CamelException
 * @fmt: a printf-style format string
 *
 * This printfs the given data to @stream. If an error occurs, @ex
 * will be set.
 *
 * Return value: the number of characters output.
 **/
int
camel_stream_printf (CamelStream *stream, CamelException *ex,
		     const char *fmt, ... )
{
	va_list args;
	char *string;
	int ret;

	g_return_val_if_fail (CAMEL_IS_STREAM (stream), -1);

	va_start (args, fmt);
	string = g_strdup_vprintf (fmt, args);
	va_end (args);

	if (!string)
		return -1;

	ret = camel_stream_write (stream, string, strlen (string), ex);
	g_free (string);
	return ret;
}

/**
 * camel_stream_write_to_stream:
 * @stream: Source CamelStream.
 * @output_stream: Destination CamelStream.
 * @ex: a CamelException.
 *
 * Write all of a stream (until eos) into another stream, in a blocking
 * fashion.
 *
 * Return value: Returns -1 on error, or the number of bytes succesfully
 * copied across streams.
 **/
int
camel_stream_write_to_stream (CamelStream *stream, CamelStream *output_stream,
			      CamelException *ex)
{
	char tmp_buf[4096];
	int total = 0;
	int nb_read;
	int nb_written;

	g_return_val_if_fail (CAMEL_IS_STREAM (stream), -1);
	g_return_val_if_fail (CAMEL_IS_STREAM (output_stream), -1);

	while (!camel_stream_eos (stream)) {
		nb_read = camel_stream_read (stream, tmp_buf,
					     sizeof (tmp_buf), ex);
		if (nb_read < 0)
			return -1;
		else if (nb_read > 0) {
			nb_written = 0;

			while (nb_written < nb_read) {
				int len = camel_stream_write (output_stream, tmp_buf + nb_written, nb_read - nb_written, ex);
				if (len < 0)
					return -1;
				nb_written += len;
			}
			total += nb_written;
		}
	}
	return total;
}
