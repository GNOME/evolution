/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */


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
#include "camel-stream-b64.h"

#define BSIZE 512



static CamelStreamClass *parent_class = NULL;

static guchar char_to_six_bits [256] = {
	128, 128, 128, 128, 128, 128, 128, 128, /*   0 .. 7   */
	128, 128, 128, 128, 128, 128, 128, 128, /*   8 .. 15  */
	128, 128, 128, 128, 128, 128, 128, 128, /*  16 .. 23  */
	128, 128, 128, 128, 128, 128, 128, 128, /*  24 .. 31  */
	128, 128, 128, 128, 128, 128, 128, 128, /*  32 .. 39  */
	128, 128, 128,  62, 128, 128, 128,  63, /*  40 .. 47  */
	 52,  53,  54,  55,  56,  57,  58,  59, /*  48 .. 55  */
	 60,  61, 128, 128, 128,  64, 128, 128, /*  56 .. 63  */
	128,   0,   1,   2,   3,   4,   5,   6, /*  64 .. 71  */
	  7,   8,   9,  10,  11,  12,  13,  14, /*  72 .. 79  */
	 15,  16,  17,  18,  19,  20,  21,  22, /*  80 .. 87  */
	 23,  24,  25, 128, 128, 128, 128, 128, /*  88 .. 95  */
	128,  26,  27,  28,  29,  30,  31,  32, /*  96 .. 103 */
	 33,  34,  35,  36,  37,  38,  39,  40, /* 104 .. 111 */
	 41,  42,  43,  44,  45,  46,  47,  48, /* 112 .. 119 */
	 49,  50,  51, 128, 128, 128, 128, 128, /* 120 .. 127 */
	128, 128, 128, 128, 128, 128, 128, 128, /* 128 .. 135 */
	128, 128, 128, 128, 128, 128, 128, 128, /* 136 .. 143 */
	128, 128, 128, 128, 128, 128, 128, 128, /* 144 .. 151 */
	128, 128, 128, 128, 128, 128, 128, 128, /* 152 .. 159 */
	128, 128, 128, 128, 128, 128, 128, 128, /* 160 .. 167 */
	128, 128, 128, 128, 128, 128, 128, 128, /* 168 .. 175 */
	128, 128, 128, 128, 128, 128, 128, 128, /* 176 .. 183 */
	128, 128, 128, 128, 128, 128, 128, 128, /* 184 .. 191 */
	128, 128, 128, 128, 128, 128, 128, 128, /* 192 .. 199 */
	128, 128, 128, 128, 128, 128, 128, 128, /* 200 .. 207 */
	128, 128, 128, 128, 128, 128, 128, 128, /* 208 .. 215 */
	128, 128, 128, 128, 128, 128, 128, 128, /* 216 .. 223 */
	128, 128, 128, 128, 128, 128, 128, 128, /* 224 .. 231 */
	128, 128, 128, 128, 128, 128, 128, 128, /* 232 .. 239 */
	128, 128, 128, 128, 128, 128, 128, 128, /* 240 .. 247 */
	128, 128, 128, 128, 128, 128, 128, 128  /* 248 .. 255 */
};


static gchar six_bits_to_char[65] = 
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";

/* Returns the class for a CamelStreamB64 */
#define CSB64_CLASS(so) CAMEL_STREAM_B64_CLASS (GTK_OBJECT(so)->klass)

static void           my_init_with_input_stream         (CamelStreamB64 *stream_b64, 
							 CamelStream *input_stream);

static gint           my_read                           (CamelStream *stream, 
							 gchar *buffer, 
							 gint n);

static void           my_reset                          (CamelStream *stream);

static gint           my_read_decode                    (CamelStream *stream, 
							 gchar *buffer, 
							 gint n);
static gint           my_read_encode                    (CamelStream *stream, 
							 gchar *buffer, 
							 gint n);
static gboolean       my_eos                            (CamelStream *stream);

static void
camel_stream_b64_class_init (CamelStreamB64Class *camel_stream_b64_class)
{
	CamelStreamClass *camel_stream_class = CAMEL_STREAM_CLASS (camel_stream_b64_class);
	
	
	parent_class = gtk_type_class (camel_stream_get_type ());
	
	/* virtual method definition */
	camel_stream_b64_class->init_with_input_stream = my_init_with_input_stream;
	
	
	/* virtual method overload */
	camel_stream_class->read     = my_read;
	camel_stream_class->eos      = my_eos; 
	camel_stream_class->reset    = my_reset; 
	
	/* signal definition */
	
}

GtkType
camel_stream_b64_get_type (void)
{
	static GtkType camel_stream_b64_type = 0;
	
	if (!camel_stream_b64_type)	{
		GtkTypeInfo camel_stream_b64_info =	
		{
			"CamelStreamB64",
			sizeof (CamelStreamB64),
			sizeof (CamelStreamB64Class),
			(GtkClassInitFunc) camel_stream_b64_class_init,
			(GtkObjectInitFunc) NULL,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		camel_stream_b64_type = gtk_type_unique (camel_stream_get_type (), &camel_stream_b64_info);
	}
	
	return camel_stream_b64_type;
}


static void 
my_reemit_available_signal (CamelStream *parent_stream, gpointer user_data)
{
	gtk_signal_emit_by_name (GTK_OBJECT (user_data), "data_available");
}

static void           
my_init_with_input_stream (CamelStreamB64 *stream_b64, 
			   CamelStream *input_stream)
{
	g_assert (stream_b64);
	g_assert (input_stream);

	
	
	/* by default, the stream is in decode mode */	
	stream_b64->mode = CAMEL_STREAM_B64_DECODER;
	
	stream_b64->eos = FALSE;
	stream_b64->status.decode_status.keep = 0;
	stream_b64->status.decode_status.state = 0;
		
	stream_b64->input_stream = input_stream;
	
	gtk_object_ref (GTK_OBJECT (input_stream));
	
	/* 
	 *  connect to the parent stream "data_available"
	 *  stream so that we can reemit the signal on the 
	 *  seekable substream in case some data would 
	 *  be available for us 
	 */
	gtk_signal_connect (GTK_OBJECT (input_stream),
			    "data_available", 
			    my_reemit_available_signal,
			    stream_b64);
	
	
	/* bootstrapping signal */
	gtk_signal_emit_by_name (GTK_OBJECT (stream_b64), "data_available");
	
	
}



CamelStream *
camel_stream_b64_new_with_input_stream (CamelStream *input_stream)
{
	CamelStreamB64 *stream_b64;
	
	stream_b64 = gtk_type_new (camel_stream_b64_get_type ());
	CSB64_CLASS (stream_b64)->init_with_input_stream (stream_b64, input_stream);

	return CAMEL_STREAM (stream_b64);
}




void       
camel_stream_b64_set_mode (CamelStreamB64 *stream_b64,
			   CamelStreamB64Mode mode)
{
	g_assert (stream_b64);

	stream_b64->mode = mode;

	if (mode == CAMEL_STREAM_B64_DECODER) {
		stream_b64->status.decode_status.keep = 0;
		stream_b64->status.decode_status.state = 0;
	} else {
		stream_b64->status.encode_status.keep = 0;
		stream_b64->status.encode_status.state = 0;
		stream_b64->status.encode_status.end_state = 0;
	}

}




static gint 
my_read (CamelStream *stream, 
	 gchar *buffer, 
	 gint n)
{
	CamelStreamB64 *stream_b64 = CAMEL_STREAM_B64 (stream);
	
	g_assert (stream);
        
	if (stream_b64->mode == CAMEL_STREAM_B64_DECODER)
		return my_read_decode (stream, buffer, n);
	else 
		return my_read_encode (stream, buffer, n);
}



static gint 
my_read_decode (CamelStream *stream, 
		gchar *buffer, 
		gint n)
{
	CamelStreamB64 *stream_b64 = CAMEL_STREAM_B64 (stream);
	CamelStream64DecodeStatus *status;
	CamelStream    *input_stream;
	guchar six_bits_value;
	gint nb_read_in_input;
	guchar c;
	gint j = 0;
	
	g_assert (stream);
	input_stream = stream_b64->input_stream;
	
	g_assert (input_stream);
	status = &(stream_b64->status.decode_status);
	

	nb_read_in_input = camel_stream_read (input_stream, &c, 1);
	
	while ((nb_read_in_input >0 ) && (j<n)) {
		
		six_bits_value = char_to_six_bits[c];
		
		/* if we encounter an '=' we can assume the end of the stream 
		   has been found */
		if (six_bits_value == 64) {
			stream_b64->eos = TRUE;
			status->keep = 0;
			
			break;
		}
		
		/* test if we must ignore the character */
		if (six_bits_value != 128) {
			six_bits_value = six_bits_value & 0x3f;

			switch (status->state){
			case 0:
				status->keep =  six_bits_value << 2;
				
				break;
			case 1:
				buffer [j++] = status->keep | (six_bits_value >> 4);
				status->keep = (six_bits_value & 0xf) << 4;
				break;
			case 2:
				buffer [j++] = status->keep | (six_bits_value >> 2);
				status->keep = (six_bits_value & 0x3) << 6;
				break;
			case 3:
				buffer [j++] = status->keep | six_bits_value;
				status->keep = 0;
				break;
			}
			
			status->state = (status->state + 1) % 4;
			
		}
		
		if (j<n) nb_read_in_input = camel_stream_read (input_stream, &c, 1);
		
	}
	
	if ((nb_read_in_input == 0) && (camel_stream_eos (input_stream)))
		stream_b64->eos = TRUE;

	return j;
	
}


static gint 
my_read_encode (CamelStream *stream, 
		gchar *buffer, 
		gint n)
{
	CamelStreamB64 *stream_b64 = CAMEL_STREAM_B64 (stream);
	CamelStream64EncodeStatus *status;
	CamelStream *input_stream;
	gint nb_read_in_input = 0;
	guchar c;
	gint j = 0;
	gboolean end_of_read = FALSE;
	
	g_assert (stream);
	input_stream = stream_b64->input_stream;
	
	g_assert (input_stream);

	/* I don't know why the caller would want to 
	   read a zero length buffer but ... */
	if (n == 0)
		return 0;


	status = &(stream_b64->status.encode_status);
	
	
	if (status->end_state == 0) { 
		/* we are not at the end of the input steam, 
		   process the data normally */
		
		while ((j<n) && !end_of_read) {
			
			/* check if we must break the encoded line */
			if (status->line_length == 76) {
				buffer [j++] = '\n';
				status->line_length = 0;
				break;
			}
			
			/* 
			 * because we encode four characters for 
			 * 3 bytes, the last char does not need any  
			 * read to write in the stream
			 */
			if (status->state == 3) {
				buffer [j++] = six_bits_to_char [status->keep];
				status->state = 0;
				status->keep = 0;
				status->line_length++;
				break;
			}
			
			/* 
			 * in all the other phases of the stream 
			 * writing, we need to read a byte from the
			 * input stream 
			 */
			nb_read_in_input = camel_stream_read (input_stream, &c, 1);
			
			if (nb_read_in_input > 0) {
				switch (status->state){

				case 0:				
					buffer [j++] = six_bits_to_char [(c >> 2) & 0x3f];
					status->keep = (c & 0x3 ) << 4;
					break;
					
				case 1:
					buffer [j++] = six_bits_to_char [status->keep | (c >> 4)];
					status->keep = (c & 0x0f ) << 2;
					break;
					
				case 2:
					buffer [j++] = six_bits_to_char [status->keep | (c >> 6)] ;
					status->keep = (c & 0x3f );
					break;
					
				}
				
				status->state = (status->state + 1) % 4;
				status->line_length++;
			} else 
				end_of_read = TRUE;
			
			
			if (camel_stream_eos (input_stream))
				status->end_state = 1;
			
		}
	}
	
	/* 
	 * now comes the real annoying part. Because some clients
	 * expect the b64 encoded sequence length to be multiple of 4,
	 * we must pad the end with '='. 
	 * This is trivial when we write to stream as much as we want
	 * but this is not the case when we are limited in the number
	 * of chars we can write to the output stream. The consequence
	 * of this is that we must keep the state of the writing
	 * so that we can resume the next time this routine is called. 
	 */

	if ( status->end_state != 0) { 
		
		/* 
		 * we are at the end of the input stream
		 * we must pad the output with '='.
		 */
		while ((j<n) && (status->end_state != 6)) {
			
			if (status->end_state == 5) {

				status->end_state = 6;
				buffer [j++] = '\n';
				stream_b64->eos = TRUE;

			} else {
				
				switch (status->state) {
					
				/* 
				 * depending on state of the decoder, we need to 
				 * write different things. 
				 */
				case 0:	       
				/* 
				 * everyting has been written already and the
				 * output length is already a multiple of 3
				 * so that we have nothing to do.  
				 */
					status->end_state = 5;
					break;
					
				case 1:
				/* 
				 * we have something in keep  
				 * and two '=' we must write
				 */
					switch (status->end_state) {
					case 1:
						buffer [j++] = six_bits_to_char [status->keep] ;
						status->end_state++;
						break;
					case 2:
						buffer [j++] = '=';
						status->end_state++;
						break;
					case 3:
						buffer [j++] = '=';
						status->end_state = 5;
						break;
					}
					
					
					break;
					
					
				case 2:
				/* 
				 * we have something in keep  
				 * and one '=' we must write
				 */
					switch (status->end_state) {
					case 1:
						buffer [j++] = six_bits_to_char [status->keep];
						status->end_state++;
						break;
					case 2:
						buffer [j++] = '=';
						status->end_state = 5;
						break;
					}
					
					break;
					
				case 3:
				/* 
				 * we have something in keep we must write
				 */
					switch (status->end_state) {
					case 1:
						buffer [j++] = six_bits_to_char [status->keep];
						status->end_state++;
						break;
					case 2:
						buffer [j++] = '=';
						status->end_state = 5;
						break;
					}
					
					break;
				}
				
				
			}
		}
		
	}

	return j;
}
	
	
	
	
	
static gboolean
my_eos (CamelStream *stream)
{
	CamelStreamB64 *stream_b64 = CAMEL_STREAM_B64 (stream);
	
	g_assert (stream);
	g_assert (stream_b64->input_stream);
	
	return (stream_b64->eos);
}





static void 
my_reset (CamelStream *stream)
{
	CamelStreamB64 *stream_b64 = CAMEL_STREAM_B64 (stream);
	
	g_assert (stream);
	g_assert (stream_b64->input_stream);
	
	stream_b64->status.decode_status.keep = 0;
	stream_b64->status.decode_status.state = 0;
	
	stream_b64->eos = FALSE;

	camel_stream_reset (stream_b64->input_stream);
}



void
camel_stream_b64_write_to_stream (CamelStreamB64 *stream_b64, 
				  CamelStream *output_stream)
{
	gchar tmp_buf[4096];
	gint nb_read;
	gint nb_written;

	/* 
	 * default implementation that uses the input 
	 * stream and stream it in a blocking way
	 * to an output stream.
	 */
	g_assert (output_stream);
	g_assert (stream_b64);
	
	while (!camel_stream_eos (CAMEL_STREAM (stream_b64))) {
		nb_read = camel_stream_read (CAMEL_STREAM (stream_b64), tmp_buf, 4096);
		nb_written = 0;
		
		while (nb_written < nb_read) 
			nb_written += camel_stream_write (output_stream, tmp_buf + nb_written, nb_read - nb_written);
	}
	
}




