/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-seekable-substream.h: stream that piggybacks on another stream */

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


#ifndef CAMEL_SEEKABLE_SUBSTREAM_H
#define CAMEL_SEEKABLE_SUBSTREAM_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <camel/camel-seekable-stream.h>

#define CAMEL_SEEKABLE_SUBSTREAM_TYPE       (camel_seekable_substream_get_type ())
#define CAMEL_SEEKABLE_SUBSTREAM(obj)       (GTK_CHECK_CAST((obj), CAMEL_SEEKABLE_SUBSTREAM_TYPE, CamelSeekableSubstream))
#define CAMEL_SEEKABLE_SUBSTREAM_CLASS(k)   (GTK_CHECK_CLASS_CAST ((k), CAMEL_SEEKABLE_SUBSTREAM_TYPE, CamelSeekableSubstreamClass))
#define CAMEL_IS_SEEKABLE_SUBSTREAM(o)      (GTK_CHECK_TYPE((o), CAMEL_SEEKABLE_SUBSTREAM_TYPE))

struct _CamelSeekableSubstream
{
	CamelSeekableStream parent_object;

	/*  --**-- Private fields --**--  */
	CamelSeekableStream *parent_stream;
};

typedef struct {
	CamelSeekableStreamClass parent_class;

} CamelSeekableSubstreamClass;

/* Standard Gtk function */
GtkType camel_seekable_substream_get_type (void);

/* public methods */

/* obtain a new seekable substream */
CamelStream *
camel_seekable_substream_new_with_seekable_stream_and_bounds (CamelSeekableStream    *parent_stream,
							      off_t start, off_t end);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_SEEKABLE_SUBSTREAM_H */
