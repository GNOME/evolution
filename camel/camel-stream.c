/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-stream.c : abstract class for a stream */


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
#include "camel-stream.h"

static GtkObjectClass *parent_class = NULL;


/* Returns the class for a CamelMimeMessage */
#define CS_CLASS(so) CAMEL_STREAM_CLASS (GTK_OBJECT(so)->klass)

static void
default_camel_flush (CamelStream *stream)
{
	/* nothing */
}

static void
default_camel_close (CamelStream *stream)
{
	/* nothing */
}

static gint
default_camel_seek (CamelStream *stream, gint offset, CamelStreamSeekPolicy policy)
{
	/* nothing */
	return -1;
}

static void
camel_stream_class_init (CamelStreamClass *camel_stream_class)
{

	parent_class = gtk_type_class (gtk_object_get_type ());

	/* virtual method definition */
	camel_stream_class->read = NULL;
	camel_stream_class->write = NULL;
	camel_stream_class->flush = default_camel_flush;
	camel_stream_class->available = NULL;
	camel_stream_class->eos = NULL; 
	camel_stream_class->close = default_camel_close;
	camel_stream_class->seek = default_camel_seek;

	/* virtual method overload */
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
		
		camel_stream_type = gtk_type_unique (gtk_object_get_type (), &camel_stream_info);
	}
	
	return camel_stream_type;
}

/**
 * camel_stream_read: 
 * @stream: a CamelStream.
 * @buffer: buffer where bytes pulled from the stream are stored.
 * @n: max number of bytes to read.
 * 
 * Read at most @n bytes from the @stream object and stores them
 * in the buffer pointed at by @buffer.
 * 
 * Return value: number of bytes actually read.
 **/
gint 
camel_stream_read (CamelStream *stream, gchar *buffer, gint n)
{
	return CS_CLASS (stream)->read (stream, buffer, n);
}

/**
 * camel_stream_write: 
 * @stream: a CamelStream object.
 * @buffer: buffer to write.
 * @n: number of bytes to write
 *
 * Write @n bytes from the buffer pointed at by @buffer into @stream.
 *
 * Return value: the number of bytes actually written
 *  in the stream.
 **/
gint
camel_stream_write (CamelStream *stream, const gchar *buffer, gint n)
{
	return CS_CLASS (stream)->write (stream, buffer, n);
}

/**
 * camel_stream_flush:
 * @stream: a CamelStream object
 * 
 * Flushes the contents of the stream to its backing store.
 **/
void
camel_stream_flush (CamelStream *stream)
{
	CS_CLASS (stream)->flush (stream);
}

/**
 * camel_stream_available: 
 * @stream: a CamelStream object
 * 
 * Return value: the number of bytes available.
 **/
gint 
camel_stream_available (CamelStream *stream)
{
	return CS_CLASS (stream)->available (stream);
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
	return CS_CLASS (stream)->eos (stream);
}


/**
 * camel_stram_close: 
 * @stream: a CamelStream object.
 * 
 * Close the @stream object.
 **/
void
camel_stream_close (CamelStream *stream)
{
	CS_CLASS (stream)->close (stream);
}


/**
 * camel_stream_seek:
 * @stream: a CamelStream object.
 * @offset: offset value
 * @policy: what to do with the offset
 * 
 * 
 * 
 * Return value: new position, -1 if operation failed.
 **/
gint
camel_stream_seek (CamelStream *stream, gint offset, CamelStreamSeekPolicy policy)
{
	return CS_CLASS (stream)->seek (stream, offset, policy);
}




/***************** Utility functions ********************/

/**
 * came_stream_write_strings:
 * @stream: a CamelStream object.
 * @...: A %NULL terminated list of strings.
 *
 * This is a utility function that writes the list of
 * strings into the @stream object.
 */
void
camel_stream_write_strings (CamelStream *stream, ... )
{
	va_list args;
	const char *string;
	
	va_start(args, stream);
	string = va_arg (args, const char *);
	
	while (string) {
		camel_stream_write_string (stream, string);
		string = va_arg (args, char *);
	}
	va_end (args);
}
