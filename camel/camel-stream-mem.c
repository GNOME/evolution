/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-stream-mem.c: memory buffer based stream */

/*
 * Authors: Bertrand Guiheneuf <bertrand@helixcode.com>
 *	    Michael Zucchi <notzed@helixcode.com>
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
#include "camel-stream-mem.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

static CamelStreamClass *parent_class = NULL;

/* Returns the class for a CamelStreamMem */
#define CSM_CLASS(so) CAMEL_STREAM_MEM_CLASS (GTK_OBJECT(so)->klass)

static int stream_read (CamelStream *stream, char *buffer, unsigned int n);
static int stream_write (CamelStream *stream, const char *buffer, unsigned int n);
static gboolean stream_eos (CamelStream *stream);
static off_t stream_seek (CamelSeekableStream *stream, off_t offset,
			  CamelStreamSeekPolicy policy);

static void finalize (GtkObject *object);

static void
camel_stream_mem_class_init (CamelStreamMemClass *camel_stream_mem_class)
{
	CamelSeekableStreamClass *camel_seekable_stream_class =
		CAMEL_SEEKABLE_STREAM_CLASS (camel_stream_mem_class);
	CamelStreamClass *camel_stream_class =
		CAMEL_STREAM_CLASS (camel_stream_mem_class);
	GtkObjectClass *gtk_object_class =
		GTK_OBJECT_CLASS (camel_stream_mem_class);

	parent_class = gtk_type_class (camel_stream_get_type ());

	/* virtual method overload */
	camel_stream_class->read = stream_read;
	camel_stream_class->write = stream_write;
	camel_stream_class->eos = stream_eos;

	camel_seekable_stream_class->seek = stream_seek;

	gtk_object_class->finalize = finalize;
}

static void
camel_stream_mem_init (gpointer object, gpointer klass)
{
	CamelStreamMem *stream_mem = CAMEL_STREAM_MEM (object);

	stream_mem->owner = FALSE;
	stream_mem->buffer = 0;
}

GtkType
camel_stream_mem_get_type (void)
{
	static GtkType camel_stream_mem_type = 0;

	if (!camel_stream_mem_type) {
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

		camel_stream_mem_type = gtk_type_unique (camel_seekable_stream_get_type (), &camel_stream_mem_info);
	}

	return camel_stream_mem_type;
}


CamelStream *
camel_stream_mem_new (void)
{
	return camel_stream_mem_new_with_byte_array (g_byte_array_new ());
}

CamelStream *
camel_stream_mem_new_with_buffer (const char *buffer, size_t len)
{
	GByteArray *ba;

	ba = g_byte_array_new ();
	g_byte_array_append (ba, (const guint8 *)buffer, len);
	return camel_stream_mem_new_with_byte_array (ba);
}

CamelStream *
camel_stream_mem_new_with_byte_array (GByteArray *byte_array)
{
	CamelStreamMem *stream_mem;

	stream_mem = gtk_type_new (camel_stream_mem_get_type ());
	stream_mem->buffer = byte_array;
	stream_mem->owner = TRUE;

	return CAMEL_STREAM (stream_mem);
}

/* note: with these functions the caller is the 'owner' of the buffer */
void camel_stream_mem_set_byte_array (CamelStreamMem *s, GByteArray *buffer)
{
	if (s->buffer && s->owner)
		g_byte_array_free(s->buffer, TRUE);
	s->owner = FALSE;
	s->buffer = buffer;
}

void camel_stream_mem_set_buffer (CamelStreamMem *s, const char *buffer,
				  size_t len)
{
	GByteArray *ba;

	ba = g_byte_array_new ();
	g_byte_array_append (ba, (const guint8 *)buffer, len);
	camel_stream_mem_set_byte_array(s, ba);
}

static void
finalize (GtkObject *object)
{
	CamelStreamMem *stream_mem = CAMEL_STREAM_MEM (object);

	if (stream_mem->buffer && stream_mem->owner)
		g_byte_array_free (stream_mem->buffer, TRUE);

	GTK_OBJECT_CLASS (parent_class)->finalize (object);
}

static int
stream_read (CamelStream *stream, char *buffer, unsigned int n)
{
	CamelStreamMem *camel_stream_mem = CAMEL_STREAM_MEM (stream);
	CamelSeekableStream *seekable = CAMEL_SEEKABLE_STREAM (stream);

	if (seekable->bound_end != CAMEL_STREAM_UNBOUND)
		n = MIN(seekable->bound_end - seekable->position, n);

	n = MIN (n, camel_stream_mem->buffer->len - seekable->position);
	if (n > 0) {
		memcpy (buffer, camel_stream_mem->buffer->data +
			seekable->position, n);
		seekable->position += n;
	} else
		n = -1;

	return n;
}

static int
stream_write (CamelStream *stream, const char *buffer, unsigned int n)
{
	CamelStreamMem *stream_mem = CAMEL_STREAM_MEM (stream);
	CamelSeekableStream *seekable = CAMEL_SEEKABLE_STREAM (stream);

	if (seekable->bound_end != CAMEL_STREAM_UNBOUND)
		n = MIN(seekable->bound_end - seekable->position, n);

#warning "g_byte_arrays use g_malloc and so are totally unsuitable for this object"
	if (seekable->position == stream_mem->buffer->len) {
		stream_mem->buffer =
			g_byte_array_append (stream_mem->buffer, (const guint8 *)buffer, n);
	} else {
		g_byte_array_set_size (stream_mem->buffer,
				       n+stream_mem->buffer->len);
		memcpy (stream_mem->buffer->data + seekable->position, buffer, n);
	}
	seekable->position += n;

	return n;
}

static gboolean
stream_eos (CamelStream *stream)
{
	CamelStreamMem *stream_mem = CAMEL_STREAM_MEM (stream);
	CamelSeekableStream *seekable_stream = CAMEL_SEEKABLE_STREAM (stream);

	return stream_mem->buffer->len <= seekable_stream->position;
}

static off_t
stream_seek (CamelSeekableStream *stream, off_t offset,
	     CamelStreamSeekPolicy policy)
{
	off_t position;
	CamelStreamMem *stream_mem = CAMEL_STREAM_MEM (stream);

	switch  (policy) {
	case CAMEL_STREAM_SET:
		position = offset;
		break;
	case CAMEL_STREAM_CUR:
		position = stream->position + offset;
		break;
	case CAMEL_STREAM_END:
		position = (stream_mem->buffer)->len + offset;
		break;
	}

	if (stream->bound_end == CAMEL_STREAM_UNBOUND)
		position = MIN (position, stream->bound_end);
	if (stream->bound_start == CAMEL_STREAM_UNBOUND)
		position = MAX (position, 0);
	else
		position = MAX (position, stream->bound_start);

	if (position > stream_mem->buffer->len) {
		int oldlen = stream_mem->buffer->len;
		g_byte_array_set_size (stream_mem->buffer, position);
		memset (stream_mem->buffer->data + oldlen, 0,
			position - oldlen);
	}

	stream->position = position;

	return position;
}
