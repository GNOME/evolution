/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; fill-column: 160 -*- */

/* camel-stream-buffer.c : Buffer any other other stream
 *
 * Authors: Michael Zucchi <notzed@ximian.com>
 *
 * Copyright 1999-2003 Ximian, Inc. (www.ximian.com)
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "camel-stream-buffer.h"

static CamelStreamClass *parent_class = NULL;

enum {
	BUF_USER = 1<<0,	/* user-supplied buffer, do not free */
};

#define BUF_SIZE 1024

static ssize_t stream_read (CamelStream *stream, char *buffer, size_t n);
static ssize_t stream_write (CamelStream *stream, const char *buffer, size_t n);
static int stream_flush (CamelStream *stream);
static int stream_close (CamelStream *stream);
static gboolean stream_eos (CamelStream *stream);

static void init_vbuf(CamelStreamBuffer *sbf, CamelStream *s, CamelStreamBufferMode mode, char *buf, guint32 size);
static void init(CamelStreamBuffer *sbuf, CamelStream *s, CamelStreamBufferMode mode);

static void
camel_stream_buffer_class_init (CamelStreamBufferClass *camel_stream_buffer_class)
{
	CamelStreamClass *camel_stream_class = CAMEL_STREAM_CLASS (camel_stream_buffer_class);

	parent_class = CAMEL_STREAM_CLASS (camel_type_get_global_classfuncs (camel_stream_get_type ()));

	/* virtual method definition */
	camel_stream_buffer_class->init = init;
	camel_stream_buffer_class->init_vbuf = init_vbuf;

	/* virtual method overload */
	camel_stream_class->read = stream_read;
	camel_stream_class->write = stream_write;
	camel_stream_class->flush = stream_flush;
	camel_stream_class->close = stream_close;
	camel_stream_class->eos = stream_eos;
}

static void
camel_stream_buffer_init (gpointer object, gpointer klass)
{
	CamelStreamBuffer *sbf = CAMEL_STREAM_BUFFER (object);

	sbf->flags = 0;
	sbf->size = BUF_SIZE;
	sbf->buf = g_malloc(BUF_SIZE);
	sbf->ptr = sbf->buf;
	sbf->end = sbf->buf;
	sbf->mode = CAMEL_STREAM_BUFFER_READ | CAMEL_STREAM_BUFFER_BUFFER;
	sbf->stream = 0;
	sbf->linesize = 80;
	sbf->linebuf = g_malloc(sbf->linesize);
}

static void
camel_stream_buffer_finalize (CamelObject *object)
{
	CamelStreamBuffer *sbf = CAMEL_STREAM_BUFFER (object);

	if (!(sbf->flags & BUF_USER)) {
		g_free(sbf->buf);
	}
	if (sbf->stream)
		camel_object_unref (sbf->stream);

	g_free(sbf->linebuf);
}


CamelType
camel_stream_buffer_get_type (void)
{
	static CamelType camel_stream_buffer_type = CAMEL_INVALID_TYPE;

	if (camel_stream_buffer_type == CAMEL_INVALID_TYPE)	{
		camel_stream_buffer_type = camel_type_register (camel_stream_get_type (), "CamelStreamBuffer",
								sizeof (CamelStreamBuffer),
								sizeof (CamelStreamBufferClass),
								(CamelObjectClassInitFunc) camel_stream_buffer_class_init,
								NULL,
								(CamelObjectInitFunc) camel_stream_buffer_init,
								(CamelObjectFinalizeFunc) camel_stream_buffer_finalize);
	}

	return camel_stream_buffer_type;
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
	
	sbf->ptr = sbf->buf;
	sbf->end = sbf->buf;
	sbf->size = size;
	sbf->mode = mode;
}

static void
init_vbuf(CamelStreamBuffer *sbf, CamelStream *s, CamelStreamBufferMode mode, char *buf, guint32 size)
{
	set_vbuf(sbf, buf, mode, size);
	if (sbf->stream)
		camel_object_unref (sbf->stream);
	sbf->stream = s;
	camel_object_ref (sbf->stream);
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

	sbf = CAMEL_STREAM_BUFFER (camel_object_new (camel_stream_buffer_get_type ()));
	CAMEL_STREAM_BUFFER_CLASS (CAMEL_OBJECT_GET_CLASS(sbf))->init (sbf, stream, mode);

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
	sbf = CAMEL_STREAM_BUFFER (camel_object_new (camel_stream_buffer_get_type ()));
	CAMEL_STREAM_BUFFER_CLASS (CAMEL_OBJECT_GET_CLASS(sbf))->init_vbuf (sbf, stream, mode, buf, size);

	return CAMEL_STREAM (sbf);
}

static ssize_t
stream_read (CamelStream *stream, char *buffer, size_t n)
{
	CamelStreamBuffer *sbf = CAMEL_STREAM_BUFFER (stream);
	ssize_t bytes_read = 1;
	ssize_t bytes_left;
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
				bytes_read = camel_stream_read(sbf->stream, bptr, n);
				if (bytes_read>0) {
					n -= bytes_read;
					bptr += bytes_read;
				}
			} else {
				bytes_read = camel_stream_read(sbf->stream, sbf->buf, sbf->size);
				if (bytes_read>0) {
					size_t bytes_used = bytes_read > n ? n : bytes_read;
					sbf->ptr = sbf->buf;
					sbf->end = sbf->buf+bytes_read;
					memcpy(bptr, sbf->ptr, bytes_used);
					sbf->ptr += bytes_used;
					bptr += bytes_used;
					n -= bytes_used;
				}
			}
		} else {
			memcpy(bptr, sbf->ptr, n);
			sbf->ptr += n;
			bptr += n;
			n = 0;
		}
	}

	return (ssize_t)(bptr - buffer);
}

/* only returns the number passed in, or -1 on an error */
static ssize_t
stream_write_all(CamelStream *stream, const char *buffer, size_t n)
{
	size_t left = n, w;

	while (left > 0) {
		w = camel_stream_write(stream, buffer, left);
		if (w == -1)
			return -1;
		left -= w;
		buffer += w;
	}

	return n;
}

static ssize_t
stream_write (CamelStream *stream, const char *buffer, size_t n)
{
	CamelStreamBuffer *sbf = CAMEL_STREAM_BUFFER (stream);
	ssize_t total = n;
	ssize_t left, todo;

	g_return_val_if_fail( (sbf->mode & CAMEL_STREAM_BUFFER_MODE) == CAMEL_STREAM_BUFFER_WRITE, 0);

	/* first, copy as much as we can */
	left = sbf->size - (sbf->ptr-sbf->buf);
	todo = MIN(left, n);

	memcpy(sbf->ptr, buffer, todo);
	n -= todo;
	buffer += todo;
	sbf->ptr += todo;

	/* if we've filled the buffer, write it out, reset buffer */
	if (left == todo) {
		if (stream_write_all(sbf->stream, sbf->buf, sbf->size) == -1)
			return -1;

		sbf->ptr = sbf->buf;
	}

	/* if we still have more, write directly, or copy to buffer */
	if (n > 0) {
		if (n >= sbf->size/3) {
			if (stream_write_all(sbf->stream, buffer, n) == -1)
				return -1;
		} else {
			memcpy(sbf->ptr, buffer, n);
			sbf->ptr += n;
		}
	}

	return total;
}

static int
stream_flush (CamelStream *stream)
{
	CamelStreamBuffer *sbf = CAMEL_STREAM_BUFFER (stream);

	if ((sbf->mode & CAMEL_STREAM_BUFFER_MODE) == CAMEL_STREAM_BUFFER_WRITE) {
		size_t len = sbf->ptr - sbf->buf;
		
		if (camel_stream_write (sbf->stream, sbf->buf, len) == -1)
			return -1;
		
		sbf->ptr = sbf->buf;
	} else {
		/* nothing to do for read mode 'flush' */
	}

	return camel_stream_flush(sbf->stream);
}

static int
stream_close (CamelStream *stream)
{
	CamelStreamBuffer *sbf = CAMEL_STREAM_BUFFER (stream);

	if (stream_flush(stream) == -1)
		return -1;
	return camel_stream_close(sbf->stream);
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
 *
 * Read a line of characters up to the next newline character or
 * @max-1 characters.
 *
 * If the newline character is encountered, then it will be
 * included in the buffer @buf.  The buffer will be #NUL terminated.
 *
 * Return value: The number of characters read, or 0 for end of file,
 * and -1 on error.
 **/
int camel_stream_buffer_gets(CamelStreamBuffer *sbf, char *buf, unsigned int max)
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
			if (c == '\n') {
				*outptr = 0;
				sbf->ptr = inptr;
				return outptr-buf;
			}
		}
		if (outptr == outend)
			break;

		bytes_read = camel_stream_read (sbf->stream, sbf->buf, sbf->size);
		if (bytes_read == -1) {
			if (buf == outptr)
				return -1;
			else
				bytes_read = 0;
		}
		inptr = sbf->ptr = sbf->buf;
		inend = sbf->end = sbf->buf + bytes_read;
	} while (bytes_read>0);

	sbf->ptr = inptr;
	*outptr = 0;

	return (int)(outptr - buf);
}

/**
 * camel_stream_buffer_read_line: read a complete line from the stream
 * @sbf: A CamelStreamBuffer
 *
 * This function reads a complete newline-terminated line from the stream
 * and returns it in allocated memory. The trailing newline (and carriage
 * return if any) are not included in the returned string.
 *
 * Return value: the line read, which the caller must free when done with,
 * or NULL on eof. If an error occurs, @ex will be set.
 **/
char *
camel_stream_buffer_read_line (CamelStreamBuffer *sbf)
{
	unsigned char *p;
	int nread;

	p = sbf->linebuf;

	while (1) {
		nread = camel_stream_buffer_gets (sbf, p, sbf->linesize - (p - sbf->linebuf));
		if (nread <=0) {
			if (p > sbf->linebuf)
				break;
			return NULL;
		}

		p += nread;
		if (p[-1] == '\n')
			break;

		nread = p - sbf->linebuf;
		sbf->linesize *= 2;
		sbf->linebuf = g_realloc (sbf->linebuf, sbf->linesize);
		p = sbf->linebuf + nread;
	}

	p--;
	if (p > sbf->linebuf && p[-1] == '\r')
		p--;
	p[0] = 0;

	return g_strdup(sbf->linebuf);
}






