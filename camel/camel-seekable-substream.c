/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-stream-fs.c : file system based stream
 *
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
#include "camel-seekable-substream.h"

static CamelSeekableStreamClass *parent_class = NULL;

/* Returns the class for a CamelSeekableSubStream */
#define CSS_CLASS(so) CAMEL_SEEKABLE_SUBSTREAM_CLASS (GTK_OBJECT(so)->klass)

static	gint	       stream_read	      (CamelStream *stream,
					       gchar *buffer, gint n);
static	gint	       stream_write	      (CamelStream *stream,
					       const gchar *buffer,
					       gint n);
static	void	       stream_flush	      (CamelStream *stream);
static	gboolean       eos		      (CamelStream *stream);
static	off_t	       stream_seek	      (CamelSeekableStream *stream,
					       off_t offset,
					       CamelStreamSeekPolicy policy);
static	void	       finalize		      (GtkObject *object);

static	void	       init_with_seekable_stream_and_bounds	 (CamelSeekableSubstream *seekable_substream,
								  CamelSeekableStream *parent_stream,
								  off_t start, off_t end);

static void
camel_seekable_substream_class_init (CamelSeekableSubstreamClass *camel_seekable_substream_class)
{
	CamelSeekableStreamClass *camel_seekable_stream_class =	CAMEL_SEEKABLE_STREAM_CLASS (camel_seekable_substream_class);
	CamelStreamClass *camel_stream_class = CAMEL_STREAM_CLASS (camel_seekable_substream_class);
	GtkObjectClass *gtk_object_class = GTK_OBJECT_CLASS (camel_seekable_substream_class);

	parent_class = gtk_type_class (camel_seekable_stream_get_type ());

	/* virtual method definition */

	/* virtual method overload */
	camel_stream_class->read = stream_read;
	camel_stream_class->write = stream_write;
	camel_stream_class->flush = stream_flush;
	camel_stream_class->eos = eos;

	camel_seekable_stream_class->seek = stream_seek;

	gtk_object_class->finalize = finalize;
}


GtkType
camel_seekable_substream_get_type (void)
{
	static GtkType camel_seekable_substream_type = 0;

	if (!camel_seekable_substream_type) {
		GtkTypeInfo camel_seekable_substream_info =
		{
			"CamelSeekableSubstream",
			sizeof (CamelSeekableSubstream),
			sizeof (CamelSeekableSubstreamClass),
			(GtkClassInitFunc) camel_seekable_substream_class_init,
			(GtkObjectInitFunc) NULL,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		camel_seekable_substream_type = gtk_type_unique (camel_seekable_stream_get_type (), &camel_seekable_substream_info);
	}

	return camel_seekable_substream_type;
}

static void
finalize (GtkObject *object)
{
	CamelSeekableSubstream *seekable_substream =
		CAMEL_SEEKABLE_SUBSTREAM (object);

	if (seekable_substream->parent_stream)
		gtk_object_unref (GTK_OBJECT (seekable_substream->parent_stream));

	GTK_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
init_with_seekable_stream_and_bounds (CamelSeekableSubstream *seekable_substream,
				      CamelSeekableStream *parent_stream,
				      off_t start, off_t end)
{
	/* Store the parent stream. */
	seekable_substream->parent_stream = parent_stream;
	gtk_object_ref (GTK_OBJECT (parent_stream));
	gtk_object_sink (GTK_OBJECT (parent_stream));

	/* Set the bound of the substream. */
	camel_seekable_stream_set_bounds ((CamelSeekableStream *)seekable_substream, start, end);
}

/**
 * camel_seekable_substream_new_with_seekable_stream_and_bounds:
 * @parent_stream: a seekable parent stream
 * @inf_bound: a lower bound
 * @sup_bound: an upper bound
 *
 * Creates a new CamelSeekableSubstream that references the portion
 * of @parent_stream from @inf_bound to @sup_bound. (If @sup_bound is
 * #CAMEL_STREAM_UNBOUND, it references to the end of stream, even if
 * the stream grows.)
 *
 * While the substream is open, the caller cannot assume anything about
 * the current position of @parent_stream. After the substream has been
 * closed, @parent_stream will stabilize again.
 *
 * Return value: the substream
 **/
CamelStream *
camel_seekable_substream_new_with_seekable_stream_and_bounds (CamelSeekableStream *parent_stream,
							      off_t start, off_t end)
{
	CamelSeekableSubstream *seekable_substream;

	g_return_val_if_fail (CAMEL_IS_SEEKABLE_STREAM (parent_stream), NULL);

	/* Create the seekable substream. */
	seekable_substream = gtk_type_new (camel_seekable_substream_get_type ());

	/* Initialize it. */
	init_with_seekable_stream_and_bounds (seekable_substream,
					      parent_stream,
					      start, end);
	return CAMEL_STREAM (seekable_substream);
}

static gboolean
parent_reset (CamelSeekableSubstream *seekable_substream)
{
	CamelSeekableStream *parent, *seekable_stream = CAMEL_SEEKABLE_STREAM (seekable_substream);

	parent = seekable_substream->parent_stream;

	g_return_val_if_fail (parent != NULL, FALSE);

	if (camel_seekable_stream_tell(parent) != seekable_stream->position) {
		return camel_seekable_stream_seek(parent, seekable_stream->position, CAMEL_STREAM_SET)
			== seekable_stream->position;
	} else {
		return TRUE;
	}
}

static gint
stream_read (CamelStream *stream, gchar *buffer, gint n)
{
	CamelSeekableStream *seekable_stream = CAMEL_SEEKABLE_STREAM (stream);
	CamelSeekableSubstream *seekable_substream = CAMEL_SEEKABLE_SUBSTREAM (stream);
	int v;

	if (n == 0)
		return 0;

	/* Go to our position in the parent stream. */
	if (!parent_reset (seekable_substream)) {	
		stream->eos = TRUE;
		return 0;
	}

	/* Compute how much byte should be read. */
	if (seekable_stream->bound_end != CAMEL_STREAM_UNBOUND)
		n = MIN (seekable_stream->bound_end -  seekable_stream->position, n);

	if (n == 0) {
		stream->eos = TRUE;
		return 0;
	}

	v = camel_stream_read (CAMEL_STREAM (seekable_substream->parent_stream), buffer, n);

	/* ignore <0 - its an error, let the caller deal */
	if (v > 0)
		seekable_stream->position += v;

	return v;
}

static gint
stream_write (CamelStream *stream, const gchar *buffer, gint n)
{
	/* NOT VALID ON SEEKABLE SUBSTREAM */
	/* Well, its entirely valid, just not implemented */
	g_warning ("CamelSeekableSubstream:: seekable substream doesn't "
		   "have a write method\n");
	return -1;
}

static void
stream_flush (CamelStream *stream)
{
	/* NOT VALID ON SEEKABLE SUBSTREAM */
	g_warning ("CamelSeekableSubstream:: seekable substream doesn't "
		   "have a flush method\n");
}

static gboolean
eos (CamelStream *stream)
{
	CamelSeekableSubstream *seekable_substream = CAMEL_SEEKABLE_SUBSTREAM (stream);
	CamelSeekableStream *seekable_stream = CAMEL_SEEKABLE_STREAM (stream);
	guint32 substream_len;
	gboolean eos;

	g_assert (stream);

	if (stream->eos) {
		eos = TRUE;
	} else {
		parent_reset (seekable_substream);

		eos = camel_stream_eos (CAMEL_STREAM (seekable_substream->parent_stream));
		if ((!eos) && (seekable_stream->bound_end != CAMEL_STREAM_UNBOUND)) {
			substream_len = seekable_stream->bound_start - seekable_stream->bound_end;
			eos = ( seekable_stream->position >= substream_len);
		}
	}

	return eos;
}

/* seeks within a seekable substream follow the bound limits ... dont start at 0 */
static off_t
stream_seek (CamelSeekableStream *seekable_stream, off_t offset,
	     CamelStreamSeekPolicy policy)
{
	CamelSeekableSubstream *seekable_substream = CAMEL_SEEKABLE_SUBSTREAM (seekable_stream);
	CamelStream *stream = (CamelStream *)seekable_stream;
	off_t real_offset = 0;
	off_t parent_pos;

	stream->eos = FALSE;

	switch (policy) {
	case CAMEL_STREAM_SET:
		real_offset = offset;
		break;

	case CAMEL_STREAM_CUR:
		real_offset = seekable_stream->position + offset;
		break;

	case CAMEL_STREAM_END:
		if (seekable_stream->bound_end != CAMEL_STREAM_UNBOUND)
			real_offset = seekable_stream->bound_end + offset;
		else {
			parent_pos = camel_seekable_stream_seek (seekable_substream->parent_stream,
								 offset,
								 CAMEL_STREAM_END);
			if (parent_pos == -1)
				return -1;
			real_offset = parent_pos;
		}
		break;

	default:
		return -1;
	}

	if (seekable_stream->bound_end != CAMEL_STREAM_UNBOUND)
		real_offset = MIN (real_offset, seekable_stream->bound_end);

	if (real_offset<seekable_stream->bound_start)
		real_offset = seekable_stream->bound_start;

	seekable_stream->position = real_offset;
	return real_offset;
}
