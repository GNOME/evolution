/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

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
#include "camel-seekable-stream.h"
#include "camel-log.h"

static CamelStreamClass *parent_class=NULL;


/* Returns the class for a CamelSeekableStream */
#define CSS_CLASS(so) CAMEL_SEEKABLE_STREAM_CLASS (GTK_OBJECT(so)->klass)

static gint      _seek      (CamelSeekableStream *stream, 
			     gint offset, 
			     CamelStreamSeekPolicy policy);
static void      _reset     (CamelStream *stream);


static void
camel_seekable_stream_class_init (CamelSeekableStreamClass *camel_seekable_stream_class)
{
	CamelStreamClass *camel_stream_class = CAMEL_STREAM_CLASS (camel_seekable_stream_class);

	parent_class = gtk_type_class (camel_stream_get_type ());
	
	/* seekable stream methods */
	camel_seekable_stream_class->seek = _seek;

	/* camel stream methods overload */
	camel_stream_class->reset = _reset;
}

GtkType
camel_seekable_stream_get_type (void)
{
	static GtkType camel_seekable_stream_type = 0;
	
	if (!camel_seekable_stream_type)	{
		GtkTypeInfo camel_seekable_stream_info =	
		{
			"CamelSeekableStream",
			sizeof (CamelSeekableStream),
			sizeof (CamelSeekableStreamClass),
			(GtkClassInitFunc) camel_seekable_stream_class_init,
			(GtkObjectInitFunc) NULL,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		camel_seekable_stream_type = gtk_type_unique (camel_stream_get_type (), &camel_seekable_stream_info);
	}
	
	return camel_seekable_stream_type;
}




static gint
_seek (CamelSeekableStream *stream, 
       gint offset, 
       CamelStreamSeekPolicy policy)
{
	g_warning ("CamelSeekableStream::seek called on default implementation \n");
	return -1;
}



/**
 * camel_stream_seek:
 * @stream: a CamelStream object.
 * @offset: offset value
 * @policy: what to do with the offset
 * 
 * 
 * 
 * Return value: new position, -1 if operation failed.
 **/
gint
camel_seekable_stream_seek (CamelSeekableStream *stream, 
			    gint offset, 
			    CamelStreamSeekPolicy policy)
{
	return CSS_CLASS (stream)->seek (stream, offset, policy);
}




/**
 * camel_seekable_stream_get_current_position: get the position of a stream
 * @stream: seekable stream object 
 * 
 * Get the current position of a seekable stream.
 * 
 * Return value: the position.
 **/
guint32  
camel_seekable_stream_get_current_position  (CamelSeekableStream *stream)
{
	return stream->cur_pos;		
}



/* a default implementation of reset for seekable streams */
static void 
_reset (CamelStream *stream)
{
	CamelSeekableStream *seekable_stream;

	g_assert (stream);
	seekable_stream = CAMEL_SEEKABLE_STREAM (stream);

	camel_seekable_stream_seek (seekable_stream, 0, CAMEL_STREAM_SET);	
}






