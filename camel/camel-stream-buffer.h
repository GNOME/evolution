/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-stream-buffer.h :stream which buffers another stream */

/*
 *
 * Author :
 *  Michael Zucchi <notzed@helixcode.com>
 *
 * Copyright 2000 Helix Code Inc. (http://www.helixcode.com) .
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


#ifndef CAMEL_STREAM_BUFFER_H
#define CAMEL_STREAM_BUFFER_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <camel/camel-seekable-stream.h>
#include <stdio.h>

#define CAMEL_STREAM_BUFFER_TYPE     (camel_stream_buffer_get_type ())
#define CAMEL_STREAM_BUFFER(obj)     (GTK_CHECK_CAST((obj), CAMEL_STREAM_BUFFER_TYPE, CamelStreamBuffer))
#define CAMEL_STREAM_BUFFER_CLASS(k) (GTK_CHECK_CLASS_CAST ((k), CAMEL_STREAM_BUFFER_TYPE, CamelStreamBufferClass))
#define CAMEL_IS_STREAM_BUFFER(o)    (GTK_CHECK_TYPE((o), CAMEL_STREAM_BUFFER_TYPE))

typedef enum
{
	CAMEL_STREAM_BUFFER_BUFFER = 0,
	CAMEL_STREAM_BUFFER_NEWLINE,
	CAMEL_STREAM_BUFFER_NONE,
	CAMEL_STREAM_BUFFER_READ = 0x00,
	CAMEL_STREAM_BUFFER_WRITE = 0x80,
	CAMEL_STREAM_BUFFER_MODE = 0x80
} CamelStreamBufferMode;

struct _CamelStreamBuffer
{
	CamelStream parent_object;

	/* these are all of course, private */
	CamelStream *stream;

	unsigned char *buf, *ptr, *end;
	int size;

	unsigned char *linebuf;	/* for reading lines at a time */
	int linesize;

	CamelStreamBufferMode mode;
	unsigned int flags;	/* internal flags */
};


typedef struct {
	CamelStreamClass parent_class;

	/* Virtual methods */
	void (*init) (CamelStreamBuffer *stream_buffer, CamelStream *stream,
		      CamelStreamBufferMode mode);
	void (*init_vbuf) (CamelStreamBuffer *stream_buffer,
			   CamelStream *stream, CamelStreamBufferMode mode,
			   char *buf, guint32 size);

} CamelStreamBufferClass;


/* Standard Gtk function */
GtkType camel_stream_buffer_get_type (void);


/* public methods */
CamelStream *camel_stream_buffer_new (CamelStream *s,
				      CamelStreamBufferMode mode);
CamelStream *camel_stream_buffer_new_with_vbuf (CamelStream *s,
						CamelStreamBufferMode mode,
						char *buf, guint32 size);

/* unimplemented
   CamelStream *camel_stream_buffer_set_vbuf (CamelStreamBuffer *b, CamelStreamBufferMode mode, char *buf, guint32 size); */

/* read a line of characters */
int camel_stream_buffer_gets (CamelStreamBuffer *b, char *buf, unsigned int max);

char *camel_stream_buffer_read_line (CamelStreamBuffer *sbf);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_STREAM_BUFFER_H */
