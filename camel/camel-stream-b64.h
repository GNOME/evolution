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


#ifndef CAMEL_STREAM_B64_H
#define CAMEL_STREAM_B64_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <gtk/gtk.h>
#include "camel-stream.h"


#define CAMEL_STREAM_B64_TYPE     (camel_stream_b64_get_type ())
#define CAMEL_STREAM_B64(obj)     (GTK_CHECK_CAST((obj), CAMEL_STREAM_B64_TYPE, CamelStreamB64))
#define CAMEL_STREAM_B64_CLASS(k) (GTK_CHECK_CLASS_CAST ((k), CAMEL_STREAM_B64_TYPE, CamelStreamB64Class))
#define CAMEL_IS_STREAM_B64(o)    (GTK_CHECK_TYPE((o), CAMEL_STREAM_B64_TYPE))


typedef enum {

	CAMEL_STREAM_B64_DECODER,
	CAMEL_STREAM_B64_ENCODER

} CamelStreamB64Mode;


/* private type */
typedef struct {

	guchar state;
	guchar keep;
 
} CamelStream64DecodeStatus;

typedef struct {

	guchar state;
	guchar keep;
	guchar end_state;  
	guchar line_length; 

} CamelStream64EncodeStatus;


typedef union {
	CamelStream64DecodeStatus decode_status;
	CamelStream64EncodeStatus encode_status;
} CamelStream64Status;

typedef struct 
{
	CamelStream parent_object;

	/* --  all these fields are private  -- */

	CamelStream *input_stream;      /* the stream we get the data from before co/de-coding them */ 
	CamelStreamB64Mode mode;        /* the stream code or decode in B64 depending on that flag  */
	gboolean eos;

	/* decoding status */
	CamelStream64Status status;

} CamelStreamB64;



typedef struct {
	CamelStreamClass parent_class;
	
	/* Virtual methods */	
	void     (*init_with_input_stream)       (CamelStreamB64 *stream_b64, CamelStream *input_stream);

} CamelStreamB64Class;






/* Standard Gtk function */
GtkType camel_stream_b64_get_type (void);




/* public methods */						
CamelStream *         camel_stream_b64_new_with_input_stream      (CamelStream *input_stream);

void                  camel_stream_b64_set_mode                   (CamelStreamB64 *stream_b64,
								   CamelStreamB64Mode mode);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_STREAM_B64_H */






