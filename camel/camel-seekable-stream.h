/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-stream-fs.h :stream based on unix filesystem */

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


#ifndef CAMEL_SEEKABLE_STREAM_H
#define CAMEL_SEEKABLE_STREAM_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <camel/camel-stream.h>
#include <sys/types.h>
#include <unistd.h>

#define CAMEL_SEEKABLE_STREAM_TYPE     (camel_seekable_stream_get_type ())
#define CAMEL_SEEKABLE_STREAM(obj)     (CAMEL_CHECK_CAST((obj), CAMEL_SEEKABLE_STREAM_TYPE, CamelSeekableStream))
#define CAMEL_SEEKABLE_STREAM_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), CAMEL_SEEKABLE_STREAM_TYPE, CamelSeekableStreamClass))
#define CAMEL_IS_SEEKABLE_STREAM(o)    (CAMEL_CHECK_TYPE((o), CAMEL_SEEKABLE_STREAM_TYPE))


typedef enum
{
	CAMEL_STREAM_SET = SEEK_SET,
	CAMEL_STREAM_CUR = SEEK_CUR,
	CAMEL_STREAM_END = SEEK_END
} CamelStreamSeekPolicy;

#define CAMEL_STREAM_UNBOUND (~0)

struct _CamelSeekableStream
{
	CamelStream parent_object;

	off_t position;		/* current postion in the stream */
	off_t bound_start;	/* first valid position */
	off_t bound_end;	/* first invalid position */
};

typedef struct {
	CamelStreamClass parent_class;

	/* Virtual methods */
	off_t (*seek)       (CamelSeekableStream *stream, off_t offset,
			     CamelStreamSeekPolicy policy);
	off_t (*tell)	    (CamelSeekableStream *stream);
	int  (*set_bounds)  (CamelSeekableStream *stream,
			     off_t start, off_t end);
} CamelSeekableStreamClass;

/* Standard Camel function */
CamelType camel_seekable_stream_get_type (void);

/* public methods */
off_t    camel_seekable_stream_seek            (CamelSeekableStream *stream, off_t offset,
						CamelStreamSeekPolicy policy);
off_t	 camel_seekable_stream_tell    	       (CamelSeekableStream *stream);
int	 camel_seekable_stream_set_bounds      (CamelSeekableStream *, off_t start, off_t end);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_SEEKABLE_STREAM_H */
