/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

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
#include "camel-seekable-stream.h"

static CamelStreamClass *parent_class = NULL;

/* Returns the class for a CamelSeekableStream */
#define CSS_CLASS(so) CAMEL_SEEKABLE_STREAM_CLASS (GTK_OBJECT(so)->klass)

static off_t seek        (CamelSeekableStream *stream, off_t offset,
			  CamelStreamSeekPolicy policy,
			  CamelException *ex);
static off_t stream_tell (CamelSeekableStream *stream);
static void reset        (CamelStream *stream, CamelException *ex);
static void set_bounds   (CamelSeekableStream *stream, off_t start, off_t end,
			  CamelException *ex);


static void
camel_seekable_stream_class_init (CamelSeekableStreamClass *camel_seekable_stream_class)
{
	CamelStreamClass *camel_stream_class =
		CAMEL_STREAM_CLASS (camel_seekable_stream_class);

	parent_class = gtk_type_class (camel_stream_get_type ());

	/* seekable stream methods */
	camel_seekable_stream_class->seek = seek;
	camel_seekable_stream_class->tell = stream_tell;
	camel_seekable_stream_class->set_bounds = set_bounds;

	/* camel stream methods overload */
	camel_stream_class->reset = reset;
}

static void
camel_seekable_stream_init (void *o)
{
	CamelSeekableStream *stream = (CamelSeekableStream *)o;

	stream->bound_start = 0;
	stream->bound_end = CAMEL_STREAM_UNBOUND;
}

GtkType
camel_seekable_stream_get_type (void)
{
	static GtkType camel_seekable_stream_type = 0;

	if (!camel_seekable_stream_type) {
		GtkTypeInfo camel_seekable_stream_info =
		{
			"CamelSeekableStream",
			sizeof (CamelSeekableStream),
			sizeof (CamelSeekableStreamClass),
			(GtkClassInitFunc) camel_seekable_stream_class_init,
			(GtkObjectInitFunc) camel_seekable_stream_init,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		camel_seekable_stream_type = gtk_type_unique (camel_stream_get_type (), &camel_seekable_stream_info);
	}

	return camel_seekable_stream_type;
}


static off_t
seek (CamelSeekableStream *stream, off_t offset,
      CamelStreamSeekPolicy policy, CamelException *ex)
{
	g_warning ("CamelSeekableStream::seek called on default "
		   "implementation\n");
	return -1;
}

/**
 * camel_stream_seek:
 * @stream: a CamelStream object.
 * @offset: offset value
 * @policy: what to do with the offset
 * @ex: a CamelException
 *
 * Seek to the specified position in @stream.
 *
 * If @policy is CAMEL_STREAM_SET, seeks to @offset.
 *
 * If @policy is CAMEL_STREAM_CUR, seeks to the current position plus
 * @offset.
 *
 * If @policy is CAMEL_STREAM_END, seeks to the end of the stream plus
 * @offset.
 *
 * Regardless of @policy, the stream's final position will be clamped
 * to the range specified by its lower and upper bounds, and the
 * stream's eos state will be updated.
 *
 * Return value: new position, -1 if operation failed.
 **/
off_t
camel_seekable_stream_seek (CamelSeekableStream *stream, off_t offset,
			    CamelStreamSeekPolicy policy,
			    CamelException *ex)
{
	g_return_val_if_fail (CAMEL_IS_SEEKABLE_STREAM (stream), -1);

	return CSS_CLASS (stream)->seek (stream, offset, policy, ex);
}


static off_t
stream_tell (CamelSeekableStream *stream)
{
	return stream->position;
}

/**
 * camel_seekable_stream_tell:
 * @stream: seekable stream object
 *
 * Get the current position of a seekable stream.
 *
 * Return value: the position.
 **/
off_t
camel_seekable_stream_tell (CamelSeekableStream *stream)
{
	g_return_val_if_fail (CAMEL_IS_SEEKABLE_STREAM (stream), -1);

	return CSS_CLASS (stream)->tell (stream);
}


static void
set_bounds (CamelSeekableStream *stream, off_t start, off_t end,
	    CamelException *ex)
{
	/* store the bounds */
	stream->bound_start = start;
	stream->bound_end = end;

	if (start > stream->position)
		camel_seekable_stream_seek (stream, start, CAMEL_STREAM_SET, ex);
}

/**
 * camel_seekable_stream_set_bounds:
 * @stream: a seekable stream
 * @start: the first valid position
 * @end: the first invalid position, or CAMEL_STREAM_UNBOUND
 *
 * Set the range of valid data this stream is allowed to cover.  If
 * there is to be no @end value, then @end should be set to
 * #CAMEL_STREAM_UNBOUND.
 **/
void
camel_seekable_stream_set_bounds (CamelSeekableStream *stream,
				  off_t start, off_t end, CamelException *ex)
{
	g_return_if_fail (CAMEL_IS_SEEKABLE_STREAM (stream));
	g_return_if_fail (end == CAMEL_STREAM_UNBOUND || end >= start);

	CSS_CLASS (stream)->set_bounds (stream, start, end, ex);
}

/* a default implementation of reset for seekable streams */
static void
reset (CamelStream *stream, CamelException *ex)
{
	CamelSeekableStream *seekable_stream;

	seekable_stream = CAMEL_SEEKABLE_STREAM (stream);

	camel_seekable_stream_seek (seekable_stream,
				    seekable_stream->bound_start,
				    CAMEL_STREAM_SET, ex);
}






