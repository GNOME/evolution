/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-stream.c : abstract class for a stream */


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

#include "camel-stream.h"

static CamelStreamClass *parent_class=NULL;


/* Returns the class for a CamelMimeMessage */
#define CS_CLASS(so) CAMEL_STREAM_CLASS (GTK_OBJECT(so)->klass)

static gint _read (CamelStream *stream, gchar **buffer, gint n);
static gint _write (CamelStream *stream, gchar *buffer, gint n);
static void _flush (CamelStream *stream);
static gint _available (CamelStream *stream);
static gboolean _eos (CamelStream *stream);
static void _close (CamelStream *stream);


static void
camel_stream_class_init (CamelStreamClass *camel_stream_class)
{

	parent_class = gtk_type_class (gtk_object_get_type ());

	/* virtual method definition */
	camel_stream_class->read = _read;
	camel_stream_class->write = _write;
	camel_stream_class->flush = _flush;
	camel_stream_class->available = _available;
	camel_stream_class->eos = _eos;
	camel_stream_class->close = _close;

	/* virtual method overload */

}



GtkType
camel_stream_get_type (void)
{
	static GtkType camel_stream_type = 0;
	
	if (!camel_stream_type)	{
		GtkTypeInfo camel_stream_info =	
		{
			"CamelMimeMessage",
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
 * _read: read bytes from a stream
 * @stream: stream
 * @buffer: buffer where bytes are stored
 * @n: max number of bytes to read
 * 
 * 
 * 
 * Return value: number of bytes actually read.
 **/
static gint
_read (CamelStream *stream, gchar **buffer, gint n)
{
	
}


/**
 * _write: read bytes to a stream
 * @stream: the stream
 * @buffer: byte buffer
 * @n: number of bytes to write
 * 
 * 
 * 
 * Return value: the number of bytes actually written
 *  in the stream.
 **/
static gint
_write (CamelStream *stream, gchar *buffer, gint n)
{

}



/**
 * _flush: flush pending changes 
 * @stream: the stream
 * 
 * 
 **/
static void
_flush (CamelStream *stream)
{

}



/**
 * _available: return the number of bytes available for reading
 * @stream: the stream
 * 
 * Return the number of bytes available without blocking.
 * 
 * Return value: the number of bytes available
 **/
static gint 
_available (CamelStream *stream)
{

}


/**
 * _eos: test if there are bytes left to read
 * @stream: the stream
 * 
 * 
 * 
 * Return value: true if all stream has been read
 **/
static gboolean
_eos (CamelStream *stream)
{

}


/**
 * _close: close a stream
 * @stream: the stream
 * 
 * 
 **/
static void
_close (CamelStream *stream)
{

}
