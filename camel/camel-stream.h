/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-stream.h : class for an abstract stream */

/* 
 *
 * Copyright (C) 1999 Bertrand Guiheneuf <Bertrand.Guiheneuf@inria.fr> .
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


#ifndef CAMEL_STREAM_H
#define CAMEL_STREAM_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <gtk/gtk.h>

#define CAMEL_STREAM_TYPE     (camel_stream_get_type ())
#define CAMEL_STREAM(obj)     (GTK_CHECK_CAST((obj), CAMEL_STREAM_TYPE, CamelStream))
#define CAMEL_STREAM_CLASS(k) (GTK_CHECK_CLASS_CAST ((k), CAMEL_STREAM_TYPE, CamelStreamClass))
#define IS_CAMEL_STREAM(o)    (GTK_CHECK_TYPE((o), CAMEL_STREAM_TYPE))



typedef struct 
{
	GtkObject parent_object;

} CamelStream;



typedef struct {
	GtkObjectClass parent_class;
	
	/* Virtual methods */	
gint  (*read) (CamelStream *stream, gchar *buffer, gint n);
gint  (*write) (CamelStream *stream, gchar *buffer, gint n);
void  (*flush) (CamelStream *stream);
gint  (*available) (CamelStream *stream);
gboolean  (*eos) (CamelStream *stream);
void  (*close) (CamelStream *stream);

} CamelStreamClass;



/* Standard Gtk function */
GtkType camel_stream_get_type (void);


/* public methods */
gint camel_stream_read (CamelStream *stream, gchar *buffer, gint n);
gint camel_stream_write (CamelStream *stream, gchar *buffer, gint n);
void camel_stream_close (CamelStream *stream);

/* utility macros and funcs */
#define camel_stream_write_string(stream, string) camel_stream_write ((stream), (string), strlen (string))

void camel_stream_write_strings (CamelStream *stream, ... );

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_STREAM_H */
