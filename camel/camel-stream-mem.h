/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-stream-mem.h :stream based on memory buffer
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


#ifndef CAMEL_STREAM_MEM_H
#define CAMEL_STREAM_MEM_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <camel/camel-seekable-stream.h>
#include <sys/types.h>

#define CAMEL_STREAM_MEM_TYPE     (camel_stream_mem_get_type ())
#define CAMEL_STREAM_MEM(obj)     (GTK_CHECK_CAST((obj), CAMEL_STREAM_MEM_TYPE, CamelStreamMem))
#define CAMEL_STREAM_MEM_CLASS(k) (GTK_CHECK_CLASS_CAST ((k), CAMEL_STREAM_MEM_TYPE, CamelStreamMemClass))
#define CAMEL_IS_STREAM_MEM(o)    (GTK_CHECK_TYPE((o), CAMEL_STREAM_MEM_TYPE))

struct _CamelStreamMem
{
	CamelSeekableStream parent_object;

	gboolean owner;		/* do we own the buffer? */
	GByteArray *buffer;
};

typedef struct {
	CamelSeekableStreamClass parent_class;
	
	/* Virtual methods */	
} CamelStreamMemClass;

/* Standard Gtk function */
GtkType camel_stream_mem_get_type (void);

/* public methods */
CamelStream *camel_stream_mem_new (void);
CamelStream *camel_stream_mem_new_with_byte_array (GByteArray *buffer);
CamelStream *camel_stream_mem_new_with_buffer (const char *buffer, size_t len);

/* these are really only here for implementing classes */
void camel_stream_mem_set_byte_array (CamelStreamMem *, GByteArray *buffer);
void camel_stream_mem_set_buffer (CamelStreamMem *, const char *buffer, size_t len);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_STREAM_MEM_H */
