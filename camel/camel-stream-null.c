/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; fill-column: 160 -*- */
/* camel-stream.c : abstract class for a stream */

/*
 * Author:
 *  Michael Zucchi <notzed@helixcode.com>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "camel-stream-null.h"

static CamelObjectClass *parent_class = NULL;

/* Returns the class for a CamelStream */
#define CS_CLASS(so) CAMEL_STREAM_NULL_CLASS(CAMEL_OBJECT_GET_CLASS(so))

/* dummy implementations, for a NULL stream */
static ssize_t   stream_read       (CamelStream *stream, char *buffer, size_t n) { return 0; }
static ssize_t   stream_write      (CamelStream *stream, const char *buffer, size_t n) { return n; }
static int       stream_close      (CamelStream *stream) { return 0; }
static int       stream_flush      (CamelStream *stream) { return 0; }
static gboolean  stream_eos        (CamelStream *stream) { return TRUE; }
static int       stream_reset      (CamelStream *stream) { return 0; }

static void
camel_stream_null_class_init (CamelStreamClass *camel_stream_null_class)
{
	CamelStreamClass *camel_stream_class = (CamelStreamClass *)camel_stream_null_class;

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
camel_stream_null_get_type (void)
{
	static CamelType camel_stream_null_type = CAMEL_INVALID_TYPE;

	if (camel_stream_null_type == CAMEL_INVALID_TYPE) {
		camel_stream_null_type = camel_type_register( camel_stream_get_type(),
							      "CamelStreamNull",
							      sizeof( CamelStreamNull ),
							      sizeof( CamelStreamNullClass ),
							      (CamelObjectClassInitFunc) camel_stream_null_class_init,
							      NULL,
							      NULL,
							      NULL );
	}

	return camel_stream_null_type;
}

/**
 * camel_stream_fs_new_with_fd:
 * @fd: a file descriptor
 *
 * Returns a NULL stream.  A null stream is always at eof, and
 * always returns success for all reads and writes.
 *
 * Return value: the stream
 **/
CamelStreamNull *
camel_stream_null_new(void)
{
	return (CamelStreamNull *)camel_object_new(camel_stream_null_get_type ());
}
