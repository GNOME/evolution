/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-stream-fs.c : file system based stream */

/* inspired by gnome-stream-fs.c in bonobo by Miguel de Icaza */
/* 
 *
 * Author : 
 *  Bertrand Guiheneuf <bertrand@helixcode.com>
 *
 * Copyright 1999, 2000 HelixCode (http://www.helixcode.com) .
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
#include "camel-log.h"

static CamelSeekableStreamClass *parent_class=NULL;


/* Returns the class for a CamelSeekableSubStream */
#define CSS_CLASS(so) CAMEL_SEEKABLE_SUBSTREAM_CLASS (GTK_OBJECT(so)->klass)




static	gint	       _read			      (CamelStream *stream, 
						       gchar *buffer, gint n);
static	gint	       _write			      (CamelStream *stream, 
						       const gchar *buffer, 
						       gint n);
static	void	       _flush			      (CamelStream *stream);
static	gint	       _available		      (CamelStream *stream);
static	gboolean       _eos			      (CamelStream *stream);
static	void	       _close			      (CamelStream *stream);
static	gint	       _seek			      (CamelSeekableStream *stream, 
						       gint offset, 
						       CamelStreamSeekPolicy policy);

static	void	       _finalize		      (GtkObject *object);
static	void	       _destroy			      (GtkObject *object);

static	void	       _init_with_seekable_stream_and_bounds	 (CamelSeekableSubstream *seekable_substream, 
								  CamelSeekableStream *parent_stream,
								  guint32 inf_bound, 
								  gint64  sup_bound);




static void
camel_seekable_substream_class_init (CamelSeekableSubstreamClass *camel_seekable_substream_class)
{
	CamelSeekableStreamClass *camel_seekable_stream_class = CAMEL_SEEKABLE_STREAM_CLASS (camel_seekable_substream_class);
	CamelStreamClass *camel_stream_class = CAMEL_STREAM_CLASS (camel_seekable_substream_class);
	GtkObjectClass *gtk_object_class = GTK_OBJECT_CLASS (camel_seekable_substream_class);

	parent_class = gtk_type_class (camel_seekable_stream_get_type ());
	
	/* virtual method definition */
	camel_seekable_substream_class->init_with_seekable_stream_and_bounds = _init_with_seekable_stream_and_bounds;
	
	/* virtual method overload */
	camel_stream_class->read = _read;
	camel_stream_class->write = _write;
	camel_stream_class->flush = _flush;
	camel_stream_class->available = _available;
	camel_stream_class->eos = _eos;
	camel_stream_class->close = _close;

	camel_seekable_stream_class->seek = _seek;

	gtk_object_class->finalize = _finalize;
	gtk_object_class->destroy = _destroy;

}

static void
camel_seekable_substream_init (gpointer   object,  gpointer   klass)
{
	/* does nothing */
}




GtkType
camel_seekable_substream_get_type (void)
{
	static GtkType camel_seekable_substream_type = 0;
	
	if (!camel_seekable_substream_type)	{
		GtkTypeInfo camel_seekable_substream_info =	
		{
			"CamelSeekableSubstream",
			sizeof (CamelSeekableSubstream),
			sizeof (CamelSeekableSubstreamClass),
			(GtkClassInitFunc) camel_seekable_substream_class_init,
			(GtkObjectInitFunc) camel_seekable_substream_init,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		camel_seekable_substream_type = gtk_type_unique (camel_seekable_stream_get_type (), &camel_seekable_substream_info);
	}
	
	return camel_seekable_substream_type;
}






static void           
_destroy (GtkObject *object)
{
	
	CAMEL_LOG_FULL_DEBUG ("Entering CamelSeekableSubstream::destroy\n");
	
	/* does nothing for the moment */
	GTK_OBJECT_CLASS (parent_class)->destroy (object);
	
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelSeekableSubstream::destroy\n");
}


static void           
_finalize (GtkObject *object)
{
	CamelSeekableStream *seekable_stream;
	CamelSeekableSubstream *seekable_substream;
	CAMEL_LOG_FULL_DEBUG ("Entering CamelSeekableSubstream::finalize\n");
	
	seekable_stream = CAMEL_SEEKABLE_STREAM (object);
	seekable_substream = CAMEL_SEEKABLE_SUBSTREAM (object);

	if (seekable_substream->parent_stream)
		gtk_object_unref (GTK_OBJECT (seekable_substream->parent_stream));

	GTK_OBJECT_CLASS (parent_class)->finalize (object);
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelSeekableSubstream::finalize\n");
}



static void 
_set_bounds (CamelSeekableSubstream *seekable_substream, guint32 inf_bound, gint64 sup_bound)
{
	CAMEL_LOG_FULL_DEBUG ("CamelSeekableSubstream::_set_bounds entering\n");

	g_assert (seekable_substream);
	g_assert (seekable_substream->parent_stream);
	
	/* store the bounds */
	seekable_substream->inf_bound = inf_bound;
	seekable_substream->sup_bound = sup_bound;


	seekable_substream->cur_pos = 0;
	seekable_substream->eos = FALSE;
	
	CAMEL_LOG_FULL_DEBUG ("In CamelSeekableSubstream::_set_bounds (%p), "
			      "setting inf bound to %lu, "
			      "sup bound to %lld, current position to %lu from %lu\n"
			      "Parent stream = %p\n", 
			      seekable_substream,
			      seekable_substream->inf_bound, seekable_substream->sup_bound,
			      seekable_substream->cur_pos, inf_bound,
			      seekable_substream->parent_stream);
	
	CAMEL_LOG_FULL_DEBUG ("CamelSeekableSubstream::_set_bounds Leaving\n");
}

static void
_reemit_parent_signal (CamelStream *parent_stream, gpointer user_data)
{
	CamelSeekableSubstream *seekable_substream = CAMEL_SEEKABLE_SUBSTREAM (user_data);

	gtk_signal_emit_by_name (GTK_OBJECT (seekable_substream), "data_available");
	
}


static void 	   
_init_with_seekable_stream_and_bounds	 (CamelSeekableSubstream *seekable_substream, 
					  CamelSeekableStream    *parent_stream,
					  guint32                 inf_bound, 
					  gint64                  sup_bound)
{
	/* sanity checks */
	g_assert (seekable_substream);
	g_assert (!seekable_substream->parent_stream);
	g_assert (parent_stream);
	
	/* store the parent stream */
	seekable_substream->parent_stream = parent_stream;
	gtk_object_ref (GTK_OBJECT (parent_stream));

	/* set the bound of the substream */
	_set_bounds (seekable_substream, inf_bound, sup_bound);

	/* 
	 *  connect to the parent stream "data_available"
	 *  stream so that we can reemit the signal on the 
	 *  seekable substream in case some data would 
	 *  be available for us 
	 */
	gtk_signal_connect (GTK_OBJECT (parent_stream),
			    "data_available", 
			    _reemit_parent_signal,
			    seekable_substream);

	gtk_signal_emit_by_name (GTK_OBJECT (seekable_substream), "data_available");

}



CamelSeekableSubstream *
camel_seekable_substream_new_with_seekable_stream_and_bounds (CamelSeekableStream    *parent_stream,
							      guint32                 inf_bound, 
							      gint64                  sup_bound)
{
	CamelSeekableSubstream *seekable_substream;

	/* create the seekable substream */
	seekable_substream = gtk_type_new (camel_seekable_substream_get_type ());

	/* initialize it */
	CSS_CLASS (seekable_substream)->init_with_seekable_stream_and_bounds (seekable_substream,
									      parent_stream,
									      inf_bound,
									      sup_bound);
	return seekable_substream;
}





/**
 * _read: read bytes from a stream
 * @stream: stream
 * @buffer: buffer where bytes are stored
 * @n: max number of bytes to read
 * 
 * 
 * 
 * Return value: number of bytes actually read.
 **/
static gint
_read (CamelStream *stream, gchar *buffer, gint n)
{
	CamelSeekableStream *seekable_stream = CAMEL_SEEKABLE_STREAM (stream);
	CamelSeekableSubstream *seekable_substream = CAMEL_SEEKABLE_SUBSTREAM (stream);
	gint v;
	gint nb_to_read;
	guint32 parent_stream_current_position;
	guint32 position_in_parent;
	
	g_assert (stream);
	g_assert (seekable_substream->parent_stream);

	g_assert (n);


	/* 
	   we are doing something quite infefficient : 
	   
	   each time we want to read a block, we store
	   the parent stream position so that we can 
	   change it, and restore it before returning 

	   This may change. I don't know yet. 
	   It may be useless to reseek every time
	   and is incompatible with buffering. 
	*/


	parent_stream_current_position = 
		camel_seekable_stream_get_current_position (seekable_substream->parent_stream);

	/* compute the position in the parent stream */
	position_in_parent =  
		seekable_stream->cur_pos + seekable_substream->inf_bound;
		
	/* compute how much byte should be read */
	if (seekable_substream->sup_bound != -1)
		nb_to_read = 
			MIN (seekable_substream->sup_bound - position_in_parent, n);
	else
		nb_to_read = n;
	//printf ("Nb to read = %d\n", n); 
	if (!nb_to_read) {
		
		seekable_substream->eos = TRUE;
		return 0;
	}

	/*printf ("In SeekableSubstream(%p)::read, the position has to be changed\n", stream);
	printf ("--- parent_stream_current_position=%d,  position_in_parent=%d\n", 
		parent_stream_current_position, position_in_parent);
	printf ("--- seekable_substream->inf_bound=%d\n", seekable_substream->inf_bound);
	printf ("--- seekable_substream->sup_bound=%d\n", seekable_substream->sup_bound);
	printf ("--- seekable_substream->parent_stream=%p\n", seekable_substream->parent_stream);
	*/
	/* go to our position in the parent stream */
	if (parent_stream_current_position != position_in_parent) {
		
		
	camel_seekable_stream_seek (seekable_substream->parent_stream, 
					    position_in_parent,
					    CAMEL_STREAM_SET);
	}

	/* check if we were able to set the position in the parent */
	parent_stream_current_position = 
		camel_seekable_stream_get_current_position (seekable_substream->parent_stream);

	if (parent_stream_current_position != position_in_parent) {
		seekable_substream->eos = TRUE;
		return 0;
	}

	
	/* Read the data */
	if (nb_to_read >0 )
		v = camel_stream_read ( CAMEL_STREAM (seekable_substream->parent_stream), 
					buffer, 
					nb_to_read);
	else 
		v = 0;

	/* if the return value is negative, an error occured, 
	   we must do something  FIXME : handle exception */ 
	if (v<0)
		CAMEL_LOG_FULL_DEBUG ("CamelSeekableSubstream::read v=%d\n", v);
	else 
		seekable_stream->cur_pos += v;


	/* printf ("Nb Bytes Read : %d\n",v); */
	/* return the number of bytes read */
	return v;
}


/**
 * _write: write bytes to a stream
 * @stream: the stream
 * @buffer: byte buffer
 * @n: number of bytes to write
 * 
 * 
 * 
 * Return value: the number of bytes actually written
 *  in the stream.
 **/
static gint
_write (CamelStream *stream, const gchar *buffer, gint n)
{
	/* NOT VALID ON SEEKABLE SUBSTREAM */
	g_warning ("CamelSeekableSubstream:: seekable substream don't have a write method\n");
	CAMEL_LOG_WARNING ( "CamelSeekableSubstream:: seekable substream don't have a write method\n");
	return -1;
}



/**
 * _flush: flush pending changes 
 * @stream: the stream
 * 
 * 
 **/
static void
_flush (CamelStream *stream)
{
	/* NOT VALID ON SEEKABLE SUBSTREAM */
	g_warning ("CamelSeekableSubstream:: seekable substream don't have a flush method\n");
	CAMEL_LOG_WARNING ( "CamelSeekableSubstream:: seekable substream don't have a flush method\n");
}



/**
 * _available: return the number of bytes available for reading
 * @stream: the stream
 * 
 * Return the number of bytes available without blocking.
 * 
 * Return value: the number of bytes available
 **/
static gint 
_available (CamelStream *stream)
{
	g_warning ("not implemented\n");
	return -1;
}


/**
 * _eos: test if there are bytes left to read
 * @stream: the stream
 * 
 * 
 * 
 * Return value: true if all stream has been read
 **/
static gboolean
_eos (CamelStream *stream)
{
	CamelSeekableSubstream *seekable_substream = CAMEL_SEEKABLE_SUBSTREAM (stream);
	CamelSeekableStream *seekable_stream = CAMEL_SEEKABLE_STREAM (stream);
	guint32 substream_len;
	gboolean eos;
	
	g_assert (stream);

	if (seekable_substream->eos)
		eos = TRUE;
	else {

		/* first check that the parent stream is ok */
		eos = camel_stream_eos (CAMEL_STREAM (seekable_substream->parent_stream));
		if ((!eos) && (seekable_substream->sup_bound != -1)) {
			substream_len = seekable_substream->sup_bound - seekable_substream->inf_bound;		
			eos = ( seekable_stream->cur_pos >= substream_len);
		} 
	}

	
	return eos;
}


/**
 * _close: close a stream
 * @stream: the stream
 * 
 * 
 **/
static void
_close (CamelStream *stream)
{
	CamelSeekableSubstream *seekable_substream = CAMEL_SEEKABLE_SUBSTREAM (stream);

	g_assert (stream);
	seekable_substream->open = FALSE;
}


static gint
_seek (CamelSeekableStream *stream, gint offset, CamelStreamSeekPolicy policy)
{
	CamelSeekableSubstream *seekable_substream = CAMEL_SEEKABLE_SUBSTREAM (stream);
	CamelSeekableStream *seekable_stream = CAMEL_SEEKABLE_STREAM (stream);
	gint64 real_offset = 0; 
	guint32 substream_len;
	guint32 parent_pos;
	gboolean seek_done = FALSE;

	substream_len = seekable_substream->sup_bound - seekable_substream->inf_bound;
	
	seekable_substream->eos = FALSE;

	switch  (policy) {

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
			seekable_stream->cur_pos = parent_pos - seekable_substream->inf_bound;
			seek_done = TRUE;
		}

		break;

	default:
		return -1;
	}

	if (!seek_done) {
		if (real_offset > 0) {
			seekable_stream->cur_pos = MIN (real_offset, substream_len);
		} else 
			seekable_stream->cur_pos = 0;
	}
	

	return seekable_stream->cur_pos;
}


