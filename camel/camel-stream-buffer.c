/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* camel-stream-buffer.c : Buffer any other other stream
 *
 * Authors: Michael Zucchi <notzed@helixcode.com>
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
#include "camel-stream-buffer.h"
#include "camel-exception.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

static CamelStreamBufferClass *parent_class = NULL;

enum {
	BUF_USER = 1<<0,	/* user-supplied buffer, do not free */
};

#define BUF_SIZE 1024

static int stream_read (CamelStream *stream, char *buffer,
			unsigned int n, CamelException *ex);
static int stream_write (CamelStream *stream, const char *buffer,
			 unsigned int n, CamelException *ex);
static void stream_flush (CamelStream *stream, CamelException *ex);
static gboolean stream_eos (CamelStream *stream);

static void finalize (GtkObject *object);
static void destroy (GtkObject *object);

static void init_vbuf(CamelStreamBuffer *sbf, CamelStream *s, CamelStreamBufferMode mode, char *buf, guint32 size);
static void init(CamelStreamBuffer *sbuf, CamelStream *s, CamelStreamBufferMode mode);

static void
camel_stream_buffer_class_init (CamelStreamBufferClass *camel_stream_buffer_class)
{
	CamelStreamClass *camel_stream_class = CAMEL_STREAM_CLASS (camel_stream_buffer_class);
	GtkObjectClass *gtk_object_class = GTK_OBJECT_CLASS (camel_stream_buffer_class);

	parent_class = gtk_type_class (camel_stream_get_type ());

	/* virtual method definition */
	camel_stream_buffer_class->init = init;
	camel_stream_buffer_class->init_vbuf = init_vbuf;

	/* virtual method overload */
	camel_stream_class->read = stream_read;
	camel_stream_class->write = stream_write;
	camel_stream_class->flush = stream_flush;
	camel_stream_class->eos = stream_eos;

	gtk_object_class->finalize = finalize;
	gtk_object_class->destroy = destroy;

}

static void
camel_stream_buffer_init (gpointer   object,  gpointer   klass)
{
	CamelStreamBuffer *sbf = CAMEL_STREAM_BUFFER (object);

	sbf->flags = 0;
	sbf->size = BUF_SIZE;
	sbf->buf = g_malloc(BUF_SIZE);
	sbf->ptr = sbf->buf;
	sbf->end = sbf->buf;
	sbf->mode = CAMEL_STREAM_BUFFER_READ | CAMEL_STREAM_BUFFER_BUFFER;
	sbf->stream = 0;
}

GtkType
camel_stream_buffer_get_type (void)
{
	static GtkType camel_stream_buffer_type = 0;

	gdk_threads_enter ();
	if (!camel_stream_buffer_type)	{
		GtkTypeInfo camel_stream_buffer_info =
		{
			"CamelStreamBuffer",
			sizeof (CamelStreamBuffer),
			sizeof (CamelStreamBufferClass),
			(GtkClassInitFunc) camel_stream_buffer_class_init,
			(GtkObjectInitFunc) camel_stream_buffer_init,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		camel_stream_buffer_type = gtk_type_unique (camel_stream_get_type (), &camel_stream_buffer_info);
	}
	gdk_threads_leave ();
	return camel_stream_buffer_type;
}


static void
destroy (GtkObject *object)
{
	CamelStreamBuffer *stream_buffer = CAMEL_STREAM_BUFFER (object);

	/* NOP to remove warnings */
	stream_buffer->buf = stream_buffer->buf;

	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}


static void
finalize (GtkObject *object)
{
	CamelStreamBuffer *sbf = CAMEL_STREAM_BUFFER (object);

	if (!(sbf->flags & BUF_USER)) {
		g_free(sbf->buf);
	}
	if (sbf->stream)
		gtk_object_unref(GTK_OBJECT(sbf->stream));

	GTK_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
set_vbuf(CamelStreamBuffer *sbf, char *buf, CamelStreamBufferMode mode, int size)
{
	if (sbf->buf && !(sbf->flags & BUF_USER)) {
		g_free(sbf->buf);
	}
	if (buf) {
		sbf->buf = buf;
		sbf->flags |= BUF_USER;
	} else {
		sbf->buf = g_malloc(size);
		sbf->flags &= ~BUF_USER;
	}
	sbf->size = size;
	sbf->mode = mode;
}

static void
init_vbuf(CamelStreamBuffer *sbf, CamelStream *s, CamelStreamBufferMode mode, char *buf, guint32 size)
{
	set_vbuf(sbf, buf, mode, size);
	if (sbf->stream)
		gtk_object_unref(GTK_OBJECT(sbf->stream));
	sbf->stream = s;
	gtk_object_ref(GTK_OBJECT(sbf->stream));
}

static void
init(CamelStreamBuffer *sbuf, CamelStream *s, CamelStreamBufferMode mode)
{
	init_vbuf(sbuf, s, mode, NULL, BUF_SIZE);
}


/**
 * camel_stream_buffer_new:
 * @stream: Existing stream to buffer.
 * @mode: Operational mode of buffered stream.
 *
 * Create a new buffered stream of another stream.  A default
 * buffer size (1024 bytes), automatically managed will be used
 * for buffering.
 *
 * See camel_stream_buffer_new_with_vbuf() for details on the
 * @mode parameter.
 *
 * Return value: A newly created buffered stream.
 **/
CamelStream *
camel_stream_buffer_new (CamelStream *stream, CamelStreamBufferMode mode)
{
	CamelStreamBuffer *sbf;
	sbf = gtk_type_new (camel_stream_buffer_get_type ());
	CAMEL_STREAM_BUFFER_CLASS (GTK_OBJECT(sbf)->klass)->init (sbf, stream, mode);

	return CAMEL_STREAM (sbf);
}

/**
 * camel_stream_buffer_new_with_vbuf:
 * @stream: An existing stream to buffer.
 * @mode: Mode to buffer in.
 * @buf: Memory to use for buffering.
 * @size: Size of buffer to use.
 *
 * Create a new stream which buffers another stream, @stream.
 *
 * The following values are available for @mode:
 *
 * CAMEL_STREAM_BUFFER_BUFFER, Buffer the input/output in blocks.
 * CAMEL_STREAM_BUFFER_NEWLINE, Buffer on newlines (for output).
 * CAMEL_STREAM_BUFFER_NONE, Perform no buffering.
 *
 * Note that currently this is ignored and CAMEL_STREAM_BUFFER_BUFFER
 * is always used.
 *
 * In addition, one of the following mode options should be or'd
 * together with the buffering mode:
 *
 * CAMEL_STREAM_BUFFER_WRITE, Buffer in write mode.
 * CAMEL_STREAM_BUFFER_READ, Buffer in read mode.
 *
 * Buffering can only be done in one direction for any
 * buffer instance.
 *
 * If @buf is non-NULL, then use the memory pointed to
 * (for upto @size bytes) as the buffer for all buffering
 * operations.  It is upto the application to free this buffer.
 * If @buf is NULL, then allocate and manage @size bytes
 * for all buffering.
 *
 * Return value: A new stream with buffering applied.
 **/
CamelStream *camel_stream_buffer_new_with_vbuf (CamelStream *stream, CamelStreamBufferMode mode, char *buf, guint32 size)
{
	CamelStreamBuffer *sbf;
	sbf = gtk_type_new (camel_stream_buffer_get_type ());
	CAMEL_STREAM_BUFFER_CLASS (GTK_OBJECT(sbf)->klass)->init_vbuf (sbf, stream, mode, buf, size);

	return CAMEL_STREAM (sbf);
}

static int
stream_read (CamelStream *stream, char *buffer, unsigned int n,
	     CamelException *ex)
{
	CamelStreamBuffer *sbf = CAMEL_STREAM_BUFFER (stream);
	int bytes_read = 1;
	int bytes_left;
	char *bptr = buffer;

	g_return_val_if_fail( (sbf->mode & CAMEL_STREAM_BUFFER_MODE) == CAMEL_STREAM_BUFFER_READ, 0);

	while (n && bytes_read > 0) {
		bytes_left = sbf->end - sbf->ptr;
		if (bytes_left < n) {
			if (bytes_left > 0) {
				memcpy(bptr, sbf->ptr, bytes_left);
				n -= bytes_left;
				bptr += bytes_left;
				sbf->ptr += bytes_left;
			}
			/* if we are reading a lot, then read directly to the destination buffer */
			if (n >= sbf->size/3) {
				bytes_read = camel_stream_read(sbf->stream, bptr, n, ex);
				if (bytes_read>0) {
					n -= bytes_read;
					bptr += bytes_read;
				}
			} else {
				bytes_read = camel_stream_read(sbf->stream, sbf->buf, sbf->size, ex);
				if (bytes_read>0) {
					sbf->ptr = sbf->buf;
					sbf->end = sbf->buf+bytes_read;
					memcpy(bptr, sbf->ptr, n);
					sbf->ptr += n;
					bptr += n;
					n -= bytes_read;
				}
			}
			if (camel_exception_is_set (ex))
				return -1;
		} else {
			memcpy(bptr, sbf->ptr, bytes_left);
			sbf->ptr += n;
			bptr += n;
			n = 0;
		}
	}

	return bptr-buffer;
}


static int
stream_write (CamelStream *stream, const char *buffer,
	      unsigned int n, CamelException *ex)
{
	CamelStreamBuffer *sbf = CAMEL_STREAM_BUFFER (stream);
	const char *bptr = buffer;
	int bytes_written = 1;
	int bytes_left;

	g_return_val_if_fail( (sbf->mode & CAMEL_STREAM_BUFFER_MODE) == CAMEL_STREAM_BUFFER_WRITE, 0);

	while (n && bytes_written > 0) {
		bytes_left = sbf->size - (sbf->ptr-sbf->buf);
		if (bytes_left<n) {
			memcpy(sbf->ptr, bptr, bytes_left);
			n -= bytes_left;
			bptr += bytes_left;
			bytes_written = camel_stream_write(sbf->stream, sbf->buf, sbf->size, ex);
			sbf->ptr = sbf->buf;
			/* if we are writing a lot, write directly to the stream */
			if (n >= sbf->size/3) {
				bytes_written = camel_stream_write(sbf->stream, bptr, n, ex);
				bytes_written = n;
				n -= bytes_written;
				bptr += bytes_written;
			} else {
				memcpy(sbf->ptr, bptr, n);
				sbf->ptr += n;
				bptr += n;
				n = 0;
			}
			if (camel_exception_is_set (ex))
				return -1;
		} else {
			memcpy(sbf->ptr, bptr, n);
			sbf->ptr += n;
			bptr += n;
			n = 0;
		}
	}
	return 0;
}



static void
stream_flush (CamelStream *stream, CamelException *ex)
{
	CamelStreamBuffer *sbf = CAMEL_STREAM_BUFFER (stream);

	if ((sbf->mode & CAMEL_STREAM_BUFFER_MODE) == CAMEL_STREAM_BUFFER_WRITE) {
		int written = camel_stream_write(sbf->stream, sbf->buf, sbf->ptr-sbf->buf, ex);
		if (written > 0) {
			sbf->ptr += written;
		}
	} else {
		/* nothing to do for read mode 'flush' */
	}

	if (!camel_exception_is_set (ex))
		camel_stream_flush(sbf->stream, ex);
}

static gboolean
stream_eos (CamelStream *stream)
{
	CamelStreamBuffer *sbf = CAMEL_STREAM_BUFFER (stream);

	return camel_stream_eos(sbf->stream) && sbf->ptr == sbf->end;
}

/**
 * camel_stream_buffer_gets:
 * @sbf: A CamelStreamBuffer.
 * @buf: Memory to write the string to.
 * @max: Maxmimum number of characters to store.
 * @ex: a CamelException
 *
 * Read a line of characters up to the next newline character or
 * @max characters.
 *
 * If the newline character is encountered, then it will be
 * included in the buffer @buf.  The buffer will be #NUL terminated.
 *
 * Return value: The number of characters read, or 0 for end of file.
 * If an error occurs, @ex will be set.
 **/
int camel_stream_buffer_gets(CamelStreamBuffer *sbf, char *buf,
			     unsigned int max, CamelException *ex)
{
	register char *outptr, *inptr, *inend, c, *outend;
	int bytes_read;

	outptr = buf;
	inptr = sbf->ptr;
	inend = sbf->end;
	outend = buf+max-1;	/* room for NUL */

	do {
		while (inptr<inend && outptr<outend) {
			c = *inptr++;
			*outptr++ = c;
			if (c=='\n') {
				*outptr = 0;
				sbf->ptr = inptr;
				return outptr-buf;
			}
		}
		if (outptr == outend)
			break;

		bytes_read = camel_stream_read(sbf->stream, sbf->buf,
					       sbf->size, ex);
		if (bytes_read>0) {
			inptr = sbf->ptr = sbf->buf;
			inend = sbf->end = sbf->buf + bytes_read;
		}
	} while (bytes_read>0 && !camel_exception_is_set (ex));

	sbf->ptr = inptr;
	if (outptr<=outend)
		*outptr = 0;

	return outptr-buf;
}

/**
 * camel_stream_buffer_read_line: read a complete line from the stream
 * @sbf: A CamelStreamBuffer
 * @ex: a CamelException
 *
 * This function reads a complete newline-terminated line from the stream
 * and returns it in allocated memory. The trailing newline (and carriage
 * return if any) are not included in the returned string.
 *
 * Return value: the line read, which the caller must free when done with,
 * or NULL on eof. If an error occurs, @ex will be set.
 **/
char *
camel_stream_buffer_read_line (CamelStreamBuffer *sbf, CamelException *ex)
{
	char *buf, *p;
	int bufsiz, nread;

	bufsiz = 80;
	p = buf = g_malloc (bufsiz);

	while (1) {
		nread = camel_stream_buffer_gets (sbf, p,
			bufsiz - (p - buf), ex);
		if (nread == 0 || camel_exception_is_set (ex)) {
			g_free (buf);
			return NULL;
		}

		p += nread;
		if (*(p - 1) == '\n')
			break;

		nread = p - buf;
		bufsiz *= 2;
		buf = g_realloc (buf, bufsiz);
		p = buf + nread;
	}

	*--p = '\0';
	if (*(p - 1) == '\r')
		*--p = '\0';
	return buf;
}
