/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-stream-fs.h :stream based on unix filesystem */

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


#ifndef CAMEL_SEEKABLE_STREAM_H
#define CAMEL_SEEKABLE_STREAM_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <gtk/gtk.h>
#include "camel-types.h"
#include "camel-stream.h"

#define CAMEL_SEEKABLE_STREAM_TYPE     (camel_seekable_stream_get_type ())
#define CAMEL_SEEKABLE_STREAM(obj)     (GTK_CHECK_CAST((obj), CAMEL_SEEKABLE_STREAM_TYPE, CamelSeekableStream))
#define CAMEL_SEEKABLE_STREAM_CLASS(k) (GTK_CHECK_CLASS_CAST ((k), CAMEL_SEEKABLE_STREAM_TYPE, CamelSeekableStreamClass))
#define CAMEL_IS_SEEKABLE_STREAM(o)    (GTK_CHECK_TYPE((o), CAMEL_SEEKABLE_STREAM_TYPE))


typedef enum
{
	CAMEL_STREAM_SET,
	CAMEL_STREAM_CUR,
	CAMEL_STREAM_END

} CamelStreamSeekPolicy;


struct _CamelSeekableStream
{
	CamelStream parent_object;
	
	guint32 cur_pos;     /* current postion in the stream */

};



typedef struct {
	CamelStreamClass parent_class;
	
	/* Virtual methods */	
	gint (*seek)       (CamelSeekableStream *stream, gint offset, CamelStreamSeekPolicy policy);
	
	
} CamelSeekableStreamClass;



/* Standard Gtk function */
GtkType camel_seekable_stream_get_type (void);


/* public methods */
gint     camel_seekable_stream_seek                      (CamelSeekableStream *stream, 
							  gint offset, 
							  CamelStreamSeekPolicy policy);
guint32  camel_seekable_stream_get_current_position      (CamelSeekableStream *stream);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_SEEKABLE_STREAM_H */
