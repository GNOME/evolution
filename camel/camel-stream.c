/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-stream.c : abstract class for a stream */

/*
 * Author:
 *  Bertrand Guiheneuf <bertrand@helixcode.com>
 *
 * Copyright 1999, 2000 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include "camel-stream.h"

static CamelObjectClass *parent_class = NULL;

/* Returns the class for a CamelStream */
#define CS_CLASS(so) CAMEL_STREAM_CLASS(CAMEL_OBJECT_GET_CLASS(so))

/* default implementations, do very little */
static ssize_t   stream_read       (CamelStream *stream, char *buffer, size_t n) { return 0; }
static ssize_t   stream_write      (CamelStream *stream, const char *buffer, size_t n) { return n; }
static int       stream_close      (CamelStream *stream) { return 0; }
static int       stream_flush      (CamelStream *stream) { return 0; }
static gboolean  stream_eos        (CamelStream *stream) { return stream->eos; }
static int       stream_reset      (CamelStream *stream) { return 0; }

static void
camel_stream_class_init (CamelStreamClass *camel_stream_class)
{
	parent_class = camel_type_get_global_classfuncs( CAMEL_OBJECT_TYPE );

	/* virtual method definition */
	camel_stream_class->read = stream_read;
	camel_stream_class->write = stream_write;
	camel_stream_class->close = stream_close;
	camel_stream_class->flush = stream_flush;
	camel_stream_class->eos = stream_eos;
	camel_stream_class->reset = stream_reset;
}

CamelType
camel_stream_get_type (void)
{
	static CamelType camel_stream_type = CAMEL_INVALID_TYPE;

	if (camel_stream_type == CAMEL_INVALID_TYPE) {
		camel_stream_type = camel_type_register( CAMEL_OBJECT_TYPE,
							 "CamelStream",
							 sizeof( CamelStream ),
							 sizeof( CamelStreamClass ),
							 (CamelObjectClassInitFunc) camel_stream_class_init,
							 NULL,
							 NULL,
							 NULL );
	}

	return camel_stream_type;
}


/**
 * camel_stream_read:
 * @stream: a CamelStream.
 * @buffer: output buffer
 * @n: max number of bytes to read.
 *
 * Attempts to read up to @len bytes from @stream into @buf.
 *
 * Returns the number of bytes actually read, or -1 on error and set
 * errno.
 **/
ssize_t
camel_stream_read (CamelStream *stream, char *buffer, size_t n)
{
	g_return_val_if_fail (CAMEL_IS_STREAM (stream), -1);
	g_return_val_if_fail (n == 0 || buffer, -1);
	
	return CS_CLASS (stream)->read (stream, buffer, n);
}


/**
 * camel_stream_write:
 * @stream: a CamelStream object.
 * @buffer: buffer to write.
 * @n: number of bytes to write
 *
 * Attempts to write up to @n bytes of @buffer into @stream.
 *
 * Returns the number of bytes written to the stream, or -1 on error
 * along with setting errno.
 **/
ssize_t
camel_stream_write (CamelStream *stream, const char *buffer, size_t n)
{
	g_return_val_if_fail (CAMEL_IS_STREAM (stream), -1);
	g_return_val_if_fail (n == 0 || buffer, -1);

	return CS_CLASS (stream)->write (stream, buffer, n);
}


/**
 * camel_stream_flush:
 * @stream: stream
 *
 * Flushes any buffered data to the stream's backing store.  Only
 * meaningful for writable streams.
 *
 * Returns 0 on success or -1 on fail along with setting errno.
 **/
int
camel_stream_flush (CamelStream *stream)
{
	g_return_val_if_fail (CAMEL_IS_STREAM (stream), -1);

	return CS_CLASS (stream)->flush (stream);
}


/**
 * camel_stream_close:
 * @stream: 
 * 
 * Closes the stream.
 * 
 * Returns 0 on success or -1 on error.
 **/
int
camel_stream_close (CamelStream *stream)
{
	g_return_val_if_fail (CAMEL_IS_STREAM (stream), -1);
	
	return CS_CLASS (stream)->close (stream);
}


/**
 * camel_stream_eos:
 * @stream: a CamelStream object
 *
 * Tests if there are bytes left to read on the @stream object.
 *
 * Returns %TRUE on EOS or %FALSE otherwise.
 **/
gboolean
camel_stream_eos (CamelStream *stream)
{
	g_return_val_if_fail (CAMEL_IS_STREAM (stream), TRUE);
	
	return CS_CLASS (stream)->eos (stream);
}


/**
 * camel_stream_reset:
 * @stream: the stream object
 *
 * Resets the stream. That is, put it in a state where it can be read
 * from the beginning again. Not all streams in Camel are seekable,
 * but they must all be resettable.
 *
 * Returns 0 on success or -1 on error along with setting errno.
 **/
int
camel_stream_reset (CamelStream *stream)
{
	g_return_val_if_fail (CAMEL_IS_STREAM (stream), -1);
	
	return CS_CLASS (stream)->reset (stream);
}

/***************** Utility functions ********************/

/**
 * camel_stream_write_string:
 * @stream: a stream object
 * @string: a string
 *
 * Writes the string to the stream.
 *
 * Returns the number of characters written or -1 on error.
 **/
ssize_t
camel_stream_write_string (CamelStream *stream, const char *string)
{
	return camel_stream_write (stream, string, strlen (string));
}


/**
 * camel_stream_printf:
 * @stream: a stream object
 * @fmt: a printf-style format string
 *
 * Write formatted output to a stream.
 *
 * Returns the number of characters written or -1 on error.
 **/
ssize_t
camel_stream_printf (CamelStream *stream, const char *fmt, ... )
{
	va_list args;
	char *string;
	ssize_t ret;
	
	g_return_val_if_fail (CAMEL_IS_STREAM (stream), -1);
	
	va_start (args, fmt);
	string = g_strdup_vprintf (fmt, args);
	va_end (args);
	
	if (!string)
		return -1;
	
	ret = camel_stream_write (stream, string, strlen (string));
	g_free (string);
	return ret;
}


/**
 * camel_stream_write_to_stream:
 * @stream: Source CamelStream.
 * @output_stream: Destination CamelStream.
 *
 * Write all of a stream (until eos) into another stream, in a
 * blocking fashion.
 *
 * Returns -1 on error, or the number of bytes succesfully
 * copied across streams.
 **/
ssize_t
camel_stream_write_to_stream (CamelStream *stream, CamelStream *output_stream)
{
	char tmp_buf[4096];
	ssize_t total = 0;
	ssize_t nb_read;
	ssize_t nb_written;

	g_return_val_if_fail (CAMEL_IS_STREAM (stream), -1);
	g_return_val_if_fail (CAMEL_IS_STREAM (output_stream), -1);

	while (!camel_stream_eos (stream)) {
		nb_read = camel_stream_read (stream, tmp_buf, sizeof (tmp_buf));
		if (nb_read < 0)
			return -1;
		else if (nb_read > 0) {
			nb_written = 0;

			while (nb_written < nb_read) {
				ssize_t len = camel_stream_write (output_stream, tmp_buf + nb_written,
								  nb_read - nb_written);
				if (len < 0)
					return -1;
				nb_written += len;
			}
			total += nb_written;
		}
	}
	return total;
}
