/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-stream-fs.c : file system based stream */

/*
 *
 * Author :
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
static	gint	       available	      (CamelStream *stream);
static	gboolean       eos		      (CamelStream *stream);
static	gint	       stream_seek	      (CamelSeekableStream *stream,
					       gint offset,
					       CamelStreamSeekPolicy policy);
					
static	void	       finalize		      (GtkObject *object);

static	void	       init_with_seekable_stream_and_bounds	 (CamelSeekableSubstream *seekable_substream,
								  CamelSeekableStream *parent_stream,
								  guint32 inf_bound,
								  gint64  sup_bound);



static void
camel_seekable_substream_class_init (CamelSeekableSubstreamClass *camel_seekable_substream_class)
{
	CamelSeekableStreamClass *camel_seekable_stream_class =
		CAMEL_SEEKABLE_STREAM_CLASS (camel_seekable_substream_class);
	CamelStreamClass *camel_stream_class =
		CAMEL_STREAM_CLASS (camel_seekable_substream_class);
	GtkObjectClass *gtk_object_class =
		GTK_OBJECT_CLASS (camel_seekable_substream_class);

	parent_class = gtk_type_class (camel_seekable_stream_get_type ());

	/* virtual method definition */
	camel_seekable_substream_class->init_with_seekable_stream_and_bounds =
		init_with_seekable_stream_and_bounds;

	/* virtual method overload */
	camel_stream_class->read = stream_read;
	camel_stream_class->write = stream_write;
	camel_stream_class->flush = stream_flush;
	camel_stream_class->available = available;
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
set_bounds (CamelSeekableSubstream *seekable_substream,
	    guint32 inf_bound, gint64 sup_bound)
{
	/* store the bounds */
	seekable_substream->inf_bound = inf_bound;
	seekable_substream->sup_bound = sup_bound;

	seekable_substream->eos = FALSE;
}


static void
reemit_parent_signal (CamelStream *parent_stream, gpointer user_data)
{
	CamelSeekableSubstream *seekable_substream =
		CAMEL_SEEKABLE_SUBSTREAM (user_data);

	gtk_signal_emit_by_name (GTK_OBJECT (seekable_substream),
				 "data_available");
}


static void
init_with_seekable_stream_and_bounds (CamelSeekableSubstream *seekable_substream,
				      CamelSeekableStream *parent_stream,
				      guint32 inf_bound, gint64 sup_bound)
{
	/* Store the parent stream. */
	seekable_substream->parent_stream = parent_stream;
	gtk_object_ref (GTK_OBJECT (parent_stream));
	gtk_object_sink (GTK_OBJECT (parent_stream));

	/* Set the bound of the substream. */
	set_bounds (seekable_substream, inf_bound, sup_bound);

	/* Connect to the parent stream "data_available" signal so
	 * that we can reemit the signal on the seekable substream in
	 * case some data would be available for us
	 */
	gtk_signal_connect (GTK_OBJECT (parent_stream), "data_available",
			    reemit_parent_signal, seekable_substream);

	gtk_signal_emit_by_name (GTK_OBJECT (seekable_substream),
				 "data_available");
}

/**
 * camel_seekable_substream_new_with_seekable_stream_and_bounds:
 * @parent_stream: a seekable parent stream
 * @inf_bound: a lower bound
 * @sup_bound: an upper bound
 *
 * Creates a new CamelSeekableSubstream that references the portion
 * of @parent_stream from @inf_bound to @sup_bound. (If @sup_bound is -1,
 * it references to the end of stream, even if the stream grows.)
 *
 * While the substream is open, the caller cannot assume anything about
 * the current position of @parent_stream. After the substream has been
 * closed, @parent_stream will stabilize again.
 *
 * Return value: the substream
 **/
CamelStream *
camel_seekable_substream_new_with_seekable_stream_and_bounds (CamelSeekableStream *parent_stream,
							      guint32 inf_bound,
							      gint64 sup_bound)
{
	CamelSeekableSubstream *seekable_substream;

	g_return_val_if_fail (CAMEL_IS_SEEKABLE_STREAM (parent_stream), NULL);

	/* Create the seekable substream. */
	seekable_substream =
		gtk_type_new (camel_seekable_substream_get_type ());

	/* Initialize it. */
	init_with_seekable_stream_and_bounds (seekable_substream,
					      parent_stream,
					      inf_bound, sup_bound);
	return CAMEL_STREAM (seekable_substream);
}


static gboolean
parent_reset (CamelSeekableSubstream *seekable_substream)
{
	CamelSeekableStream *parent, *seekable_stream =
		CAMEL_SEEKABLE_STREAM (seekable_substream);
	guint32 parent_stream_current_position;
	guint32 position_in_parent;

	parent = seekable_substream->parent_stream;
	g_return_val_if_fail (parent != NULL, FALSE);

	parent_stream_current_position =
		camel_seekable_stream_get_current_position (parent);

	/* Compute our position in the parent stream. */
	position_in_parent =
		seekable_stream->cur_pos + seekable_substream->inf_bound;

	/* Go to our position in the parent stream. */
	if (parent_stream_current_position != position_in_parent) {
		camel_seekable_stream_seek (parent, position_in_parent,
					    CAMEL_STREAM_SET);
	}

	/* Check if we were able to set the position in the parent. */
	parent_stream_current_position =
		camel_seekable_stream_get_current_position (parent);

	return parent_stream_current_position == position_in_parent;
}

static gint
stream_read (CamelStream *stream, gchar *buffer, gint n)
{
	CamelSeekableStream *seekable_stream =
		CAMEL_SEEKABLE_STREAM (stream);
	CamelSeekableSubstream *seekable_substream =
		CAMEL_SEEKABLE_SUBSTREAM (stream);
	gint v, nb_to_read, position_in_parent;

	/* Go to our position in the parent stream. */
	if (!parent_reset (seekable_substream)) {
		seekable_substream->eos = TRUE;
		return 0;
	}

	/* Compute how much byte should be read. */
	position_in_parent =
		seekable_stream->cur_pos + seekable_substream->inf_bound;
	if (seekable_substream->sup_bound != -1) {
		nb_to_read = MIN (seekable_substream->sup_bound -
				  position_in_parent, n);
	} else
		nb_to_read = n;

	if (!nb_to_read) {
		if (n)
			seekable_substream->eos = TRUE;
		return 0;
	}

	/* Read the data. */
	if (nb_to_read > 0) {
		v = camel_stream_read (CAMEL_STREAM (seekable_substream->parent_stream),
				       buffer, nb_to_read);
	} else
		v = 0;

	/* If the return value is negative, an error occured,
	 * we must do something  FIXME : handle exception
	 */
	if (v > 0)
		seekable_stream->cur_pos += v;

	return v;
}


static gint
stream_write (CamelStream *stream, const gchar *buffer, gint n)
{
	/* NOT VALID ON SEEKABLE SUBSTREAM */
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


static gint
available (CamelStream *stream)
{
	g_warning ("CamelSeekableSubstream::available not implemented\n");
	return -1;
}


static gboolean
eos (CamelStream *stream)
{
	CamelSeekableSubstream *seekable_substream =
		CAMEL_SEEKABLE_SUBSTREAM (stream);
	CamelSeekableStream *seekable_stream =
		CAMEL_SEEKABLE_STREAM (stream);
	guint32 substream_len;
	gboolean eos;

	g_assert (stream);

	if (seekable_substream->eos)
		eos = TRUE;
	else {
		parent_reset (seekable_substream);

		eos = camel_stream_eos (CAMEL_STREAM (seekable_substream->parent_stream));
		if ((!eos) && (seekable_substream->sup_bound != -1)) {
			substream_len = seekable_substream->sup_bound - seekable_substream->inf_bound;
			eos = ( seekable_stream->cur_pos >= substream_len);
		}
	}

	return eos;
}


static gint
stream_seek (CamelSeekableStream *stream, gint offset,
	     CamelStreamSeekPolicy policy)
{
	CamelSeekableSubstream *seekable_substream =
		CAMEL_SEEKABLE_SUBSTREAM (stream);
	CamelSeekableStream *seekable_stream =
		CAMEL_SEEKABLE_STREAM (stream);
	gint64 real_offset = 0;
	guint32 substream_len;
	guint32 parent_pos;
	gboolean seek_done = FALSE;

	substream_len = seekable_substream->sup_bound -
		seekable_substream->inf_bound;

	seekable_substream->eos = FALSE;

	switch (policy) {
	case CAMEL_STREAM_SET:
		real_offset = offset;
		break;

	case CAMEL_STREAM_CUR:
		real_offset = seekable_stream->cur_pos + offset;
		break;

	case CAMEL_STREAM_END:
		if (seekable_substream->sup_bound != -1)
			real_offset = substream_len - offset;
		else {
			parent_pos = camel_seekable_stream_seek (seekable_substream->parent_stream,
								 offset,
								 CAMEL_STREAM_END);
			seekable_stream->cur_pos = parent_pos -
				seekable_substream->inf_bound;
			seek_done = TRUE;
		}
		break;

	default:
		return -1;
	}

	if (!seek_done) {
		if (real_offset > 0) {
			seekable_stream->cur_pos = MIN (real_offset,
							substream_len);
		} else
			seekable_stream->cur_pos = 0;
	}

	return seekable_stream->cur_pos;
}


