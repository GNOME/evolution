/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-stream-mem.c : memory buffer based stream */

/* inspired by gnome-stream-mem.c in bonobo by Miguel de Icaza */
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
#include "camel-stream-mem.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "camel-log.h"

static CamelStreamClass *parent_class=NULL;


/* Returns the class for a CamelStreamMEM */
#define CS_CLASS(so) CAMEL_STREAM_MEM_CLASS (GTK_OBJECT(so)->klass)

static gint _read (CamelStream *stream, gchar *buffer, gint n);
static gint _write (CamelStream *stream, const gchar *buffer, gint n);
static void _flush (CamelStream *stream);
static gint _available (CamelStream *stream);
static gboolean _eos (CamelStream *stream);
static void _close (CamelStream *stream);
static gint _seek (CamelStream *stream, gint offset, CamelStreamSeekPolicy policy);

static void _finalize (GtkObject *object);

static void
camel_stream_mem_class_init (CamelStreamMemClass *camel_stream_mem_class)
{
	CamelStreamClass *camel_stream_class = CAMEL_STREAM_CLASS (camel_stream_mem_class);
	GtkObjectClass *gtk_object_class = GTK_OBJECT_CLASS (camel_stream_mem_class);
	
	parent_class = gtk_type_class (gtk_object_get_type ());
	
	/* virtual method definition */
	
	/* virtual method overload */
	camel_stream_class->read = _read;
	camel_stream_class->write = _write;
	camel_stream_class->flush = _flush;
	camel_stream_class->available = _available;
	camel_stream_class->eos = _eos;
	camel_stream_class->close = _close;
	camel_stream_class->seek = _seek;
	
	gtk_object_class->finalize = _finalize;
	
}

static void
camel_stream_mem_init (gpointer   object,  gpointer   klass)
{
	CamelStreamMem *camel_stream_mem = CAMEL_STREAM_MEM (object);
	camel_stream_mem->position = 0;
}

GtkType
camel_stream_mem_get_type (void)
{
	static GtkType camel_stream_mem_type = 0;
	
	if (!camel_stream_mem_type)	{
		GtkTypeInfo camel_stream_mem_info =	
		{
			"CamelStreamMem",
			sizeof (CamelStreamMem),
			sizeof (CamelStreamMemClass),
			(GtkClassInitFunc) camel_stream_mem_class_init,
			(GtkObjectInitFunc) camel_stream_mem_init,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		camel_stream_mem_type = gtk_type_unique (camel_stream_get_type (), &camel_stream_mem_info);
	}
	
	return camel_stream_mem_type;
}


CamelStream *
camel_stream_mem_new (CamelStreamMemMode mode)
{
	CamelStreamMem *stream_mem;
	GByteArray *buffer;

	buffer = g_byte_array_new ();	
	stream_mem = (CamelStreamMem *)camel_stream_mem_new_with_buffer (buffer, mode);
	return CAMEL_STREAM (stream_mem);
}


CamelStream *
camel_stream_mem_new_with_buffer (GByteArray *buffer, CamelStreamMemMode mode)
{
	CamelStreamMem *stream_mem;
	
	stream_mem = gtk_type_new (camel_stream_mem_get_type ());
	stream_mem->mode = mode;
	stream_mem->buffer = buffer;
	
	return CAMEL_STREAM (stream_mem);
}



static void           
_finalize (GtkObject *object)
{
	CamelStreamMem *stream_mem = CAMEL_STREAM_MEM (object);


	CAMEL_LOG_FULL_DEBUG ("Entering CamelStreamMem::finalize\n");
	g_byte_array_free (stream_mem->buffer, TRUE);
	
	GTK_OBJECT_CLASS (parent_class)->finalize (object);
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelStreamMem::finalize\n");
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
_read (CamelStream *stream, gchar *buffer, gint n)
{
	CamelStreamMem *camel_stream_mem = CAMEL_STREAM_MEM (stream);
	gint nb_bytes_to_read;

	g_assert (stream);
	nb_bytes_to_read = MIN (n, (camel_stream_mem->buffer)->len - camel_stream_mem->position);
	if (nb_bytes_to_read>0) {
		memcpy (buffer, (camel_stream_mem->buffer)->data + camel_stream_mem->position, nb_bytes_to_read);
		camel_stream_mem->position += nb_bytes_to_read;
	} else nb_bytes_to_read = -1;

	return nb_bytes_to_read;
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
_write (CamelStream *stream, const gchar *buffer, gint n)
{
	CamelStreamMem *camel_stream_mem = CAMEL_STREAM_MEM (stream);

	g_assert (stream);
	g_return_val_if_fail (camel_stream_mem->position>=0, -1);
	camel_stream_mem->buffer = g_byte_array_append (camel_stream_mem->buffer, (const guint8 *)buffer, n);
	camel_stream_mem->position += n;
	
	return n;
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
	g_warning ("Not implemented yet");
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
	g_warning ("Not implemented yet");
	return -1;
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
	g_warning ("Not implemented yet");
	return FALSE;
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
	g_warning ("Not implemented yet");
}



static gint
_seek (CamelStream *stream, gint offset, CamelStreamSeekPolicy policy)
{
	gint position;
	CamelStreamMem *camel_stream_mem = CAMEL_STREAM_MEM (stream);

	switch  (policy) {
	case CAMEL_STREAM_SET:
		position = offset;
		break;
	case CAMEL_STREAM_CUR:
		position = camel_stream_mem->position + offset;
		break;
	case CAMEL_STREAM_END:
		position = (camel_stream_mem->buffer)->len + offset;
		break;
	default:
		return -1;
	}
		
	position = MIN (position, (camel_stream_mem->buffer)->len);
	position = MAX (position, 0);

	camel_stream_mem->position = position;

	return position;
}
