/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-simple-data-wrapper.c : simple implementation of a data wrapper */
/* store the data in a glib byte array                                   */

/*
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

#include "camel-simple-data-wrapper.h"
#include "camel-simple-data-wrapper-stream.h"
#include <camel/camel-stream-mem.h>
#include "camel-mime-utils.h"
#include <camel/camel-mime-filter.h>
#include <camel/camel-mime-filter-basic.h>
#include <camel/camel-mime-filter-charset.h>
#include <camel/camel-stream-filter.h>
#include <camel/camel-seekable-substream.h>

#include <string.h>

#define d(x)

static CamelDataWrapperClass *parent_class = NULL;

/* Returns the class for a CamelDataWrapper */
#define CSDW_CLASS(so) CAMEL_SIMPLE_DATA_WRAPPER_CLASS (GTK_OBJECT (so)->klass)

static void            construct_from_stream     (CamelDataWrapper *data_wrapper,
						     CamelStream *stream);
static void            write_to_stream           (CamelDataWrapper *data_wrapper,
						     CamelStream *stream);
static void            finalize                  (GtkObject *object);
static CamelStream *   get_output_stream         (CamelDataWrapper *data_wrapper);
static void construct_from_parser(CamelDataWrapper *dw, CamelMimeParser *mp);



static void
camel_simple_data_wrapper_class_init (CamelSimpleDataWrapperClass *camel_simple_data_wrapper_class)
{
	CamelDataWrapperClass *camel_data_wrapper_class =
		CAMEL_DATA_WRAPPER_CLASS (camel_simple_data_wrapper_class);
	GtkObjectClass *gtk_object_class =
		GTK_OBJECT_CLASS (camel_data_wrapper_class);

	parent_class = gtk_type_class (camel_data_wrapper_get_type ());


	/* virtual method overload */
	camel_data_wrapper_class->write_to_stream = write_to_stream;
	camel_data_wrapper_class->construct_from_stream = construct_from_stream;
	camel_data_wrapper_class->get_output_stream = get_output_stream;

	camel_data_wrapper_class->construct_from_parser = construct_from_parser;

	gtk_object_class->finalize = finalize;
}


static void
camel_simple_data_wrapper_init (CamelSimpleDataWrapper *wrapper)
{
	wrapper->byte_array = NULL;
	wrapper->has_byte_array_stream = FALSE;
}


GtkType
camel_simple_data_wrapper_get_type (void)
{
	static GtkType camel_simple_data_wrapper_type = 0;

	if (!camel_simple_data_wrapper_type) {
		GtkTypeInfo camel_simple_data_wrapper_info =
		{
			"CamelSimpleDataWrapper",
			sizeof (CamelSimpleDataWrapper),
			sizeof (CamelSimpleDataWrapperClass),
			(GtkClassInitFunc) camel_simple_data_wrapper_class_init,
			(GtkObjectInitFunc) camel_simple_data_wrapper_init,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		camel_simple_data_wrapper_type =
			gtk_type_unique (camel_data_wrapper_get_type (),
					 &camel_simple_data_wrapper_info);
	}

	return camel_simple_data_wrapper_type;
}


static void
finalize (GtkObject *object)
{
	CamelSimpleDataWrapper *simple_data_wrapper =
		CAMEL_SIMPLE_DATA_WRAPPER (object);

	if (simple_data_wrapper->byte_array)
		g_byte_array_free (simple_data_wrapper->byte_array, TRUE);

	GTK_OBJECT_CLASS (parent_class)->finalize (object);
}


/**
 * camel_simple_data_wrapper_new:
 *
 * Return value: a new CamelSimpleDataWrapper object
 **/
CamelSimpleDataWrapper *
camel_simple_data_wrapper_new (void)
{
	return (CamelSimpleDataWrapper *)
		gtk_type_new (CAMEL_SIMPLE_DATA_WRAPPER_TYPE);
}


static void
write_to_stream (CamelDataWrapper *data_wrapper, CamelStream *stream)
{
	CamelSimpleDataWrapper *simple_data_wrapper =
		CAMEL_SIMPLE_DATA_WRAPPER (data_wrapper);
	GByteArray *array;

	array = simple_data_wrapper->byte_array;
	if ( array && array->len)
		camel_stream_write (stream, (gchar *)array->data, array->len);
	else
		parent_class->write_to_stream (data_wrapper, stream);
}


#define CMSDW_TMP_BUF_SIZE 100
static void
construct_from_stream (CamelDataWrapper *data_wrapper, CamelStream *stream)
{
	CamelSimpleDataWrapper *simple_data_wrapper =
		CAMEL_SIMPLE_DATA_WRAPPER (data_wrapper);
	gint nb_bytes_read;
	static gchar *tmp_buf;
	GByteArray *array;

	if (!tmp_buf)
		tmp_buf = g_new (gchar, CMSDW_TMP_BUF_SIZE);

	array = simple_data_wrapper->byte_array;
	if (array)
		g_byte_array_free (array, FALSE);

	array = g_byte_array_new ();
	simple_data_wrapper->byte_array = array;
	nb_bytes_read = camel_stream_read (stream, tmp_buf, CMSDW_TMP_BUF_SIZE);
	while (nb_bytes_read > 0) {
		if (nb_bytes_read > 0)
			g_byte_array_append (array, tmp_buf, nb_bytes_read);
		nb_bytes_read = camel_stream_read (stream, tmp_buf,
						   CMSDW_TMP_BUF_SIZE);
	};
}




/**
 * camel_simple_data_wrapper_set_text: set some text as data wrapper content
 * @simple_data_wrapper: SimpleDataWrapper object
 * @text: the text to use
 *
 * Utility routine used to set up the content of a SimpleDataWrapper object
 * to be a character string.
 **/
void
camel_simple_data_wrapper_set_text (CamelSimpleDataWrapper *simple_data_wrapper, const gchar *text)
{
	GByteArray *array;

	array = simple_data_wrapper->byte_array;
	if (array)
		g_byte_array_free (array, FALSE);

	array = g_byte_array_new ();
	simple_data_wrapper->byte_array = array;

	g_byte_array_append (array, text, strlen (text));
}


static CamelStream *
get_output_stream (CamelDataWrapper *data_wrapper)
{
	CamelSimpleDataWrapper *simple_data_wrapper;
	CamelStream *output_stream = NULL;

	simple_data_wrapper = CAMEL_SIMPLE_DATA_WRAPPER (data_wrapper);

	if (simple_data_wrapper->byte_array &&
	    !(simple_data_wrapper->has_byte_array_stream)) {
		output_stream = camel_simple_data_wrapper_stream_new (simple_data_wrapper);
		camel_data_wrapper_set_output_stream (data_wrapper, output_stream);
	}

	return parent_class->get_output_stream (data_wrapper);
}

/* simple data wrapper */
static void
construct_from_parser(CamelDataWrapper *dw, CamelMimeParser *mp)
{
	GByteArray *buffer;
	char *buf;
	int len;
	off_t start, end;
	CamelMimeFilter *fdec = NULL, *fch = NULL;
	struct _header_content_type *ct;
	int decid=-1, chrid=-1, cache=FALSE;
	CamelStream *source;
	char *encoding;

	d(printf("constructing simple-data-wrapper\n"));

		/* Ok, try and be smart.  If we're storing a small message (typical) convert it,
		   and store it in memory as we parse it ... if not, throw away the conversion
		   and scan till the end ... */

		/* if we can't seek, dont have a stream/etc, then we must cache it */
	source = camel_mime_parser_stream(mp);
	gtk_object_ref((GtkObject *)source);
	if (source == NULL
	    || !CAMEL_IS_SEEKABLE_STREAM(source))
		cache = TRUE;

	/* first, work out conversion, if any, required, we dont care about what we dont know about */
	encoding = header_content_encoding_decode(camel_mime_parser_header(mp, "content-transfer-encoding", NULL));
	if (encoding) {
		if (!strcasecmp(encoding, "base64")) {
			d(printf("Adding base64 decoder ...\n"));
			fdec = (CamelMimeFilter *)camel_mime_filter_basic_new_type(CAMEL_MIME_FILTER_BASIC_BASE64_DEC);
			decid = camel_mime_parser_filter_add(mp, fdec);
		} else if (!strcasecmp(encoding, "quoted-printable")) {
			d(printf("Adding quoted-printable decoder ...\n"));
			fdec = (CamelMimeFilter *)camel_mime_filter_basic_new_type(CAMEL_MIME_FILTER_BASIC_QP_DEC);
			decid = camel_mime_parser_filter_add(mp, fdec);
		}
		g_free(encoding);
	}

	/* if we're doing text, then see if we have to convert it to UTF8 as well */
	ct = camel_mime_parser_content_type(mp);
	if (header_content_type_is(ct, "text", "*")) {
		const char *charset = header_content_type_param(ct, "charset");
		if (charset!=NULL
		    && !(strcasecmp(charset, "us-ascii")==0
			 || strcasecmp(charset, "utf-8")==0)) {
			d(printf("Adding conversion filter from %s to utf-8\n", charset));
			fch = (CamelMimeFilter *)camel_mime_filter_charset_new_convert(charset, "utf-8");
			if (fch) {
				chrid = camel_mime_parser_filter_add(mp, (CamelMimeFilter *)fch);
			} else {
				g_warning("Cannot convert '%s' to 'utf-8', message display may be corrupt", charset);
			}
		}

	}

	buffer = g_byte_array_new();

		/* write to a memory buffer or something??? */
	start = camel_mime_parser_tell(mp);
	while ( camel_mime_parser_step(mp, &buf, &len) != HSCAN_BODY_END ) {
		if (buffer) {
			if (buffer->len > 20480 && !cache) {
				/* is this a 'big' message?  Yes?  We dont want to convert it all then.*/
				camel_mime_parser_filter_remove(mp, decid);
				camel_mime_parser_filter_remove(mp, chrid);
				decid = -1;
				chrid = -1;
				g_byte_array_free(buffer, TRUE);
				buffer = NULL;
			} else {
				g_byte_array_append(buffer, buf, len);
			}
		}
	}

	if (buffer) {
		CamelStream *mem;
		d(printf("Small message part, kept in memory!\n"));
		mem = camel_stream_mem_new_with_byte_array(buffer, CAMEL_STREAM_MEM_READ);
		camel_data_wrapper_set_output_stream (dw, mem);
	} else {
		CamelSeekableSubstream *sub;
		CamelStreamFilter *filter;

		d(printf("Big message part, left on disk ...\n"));

		end = camel_mime_parser_tell(mp);
		sub = (CamelSeekableSubstream *)camel_seekable_substream_new_with_seekable_stream_and_bounds ((CamelSeekableStream *)source, start, end);
		if (fdec || fch) {
			filter = camel_stream_filter_new_with_stream((CamelStream *)sub);
			if (fdec) {
				camel_mime_filter_reset(fdec);
				camel_stream_filter_add(filter, fdec);
			}
			if (fch) {
				camel_mime_filter_reset(fdec);
				camel_stream_filter_add(filter, fch);
			}
			camel_data_wrapper_set_output_stream (dw, (CamelStream *)filter);
		} else {
			camel_data_wrapper_set_output_stream (dw, (CamelStream *)sub);
		}
	}

	camel_mime_parser_filter_remove(mp, decid);
	camel_mime_parser_filter_remove(mp, chrid);

	if (fdec)
		gtk_object_unref((GtkObject *)fdec);
	if (fch)
		gtk_object_unref((GtkObject *)fch);
	gtk_object_unref((GtkObject *)source);

	/* FIXME: lookup in headers for content-type/encoding */
#if 0
	/* trivial, mem-based ... */
	buffer = g_byte_array_new();
	start = camel_mime_parser_tell(mp);
	while ( camel_mime_parser_step(mp, &buf, &len) != HSCAN_BODY_END ) {
		g_byte_array_append(buffer, buf, len);
	}
	end = camel_mime_parser_tell(mp);
	mem = camel_stream_mem_new_with_byte_array(buffer, CAMEL_STREAM_MEM_READ);
	camel_data_wrapper_set_output_stream (dw, mem);
#endif
}
