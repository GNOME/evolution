/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; fill-column: 160 -*- */
/* camel-data-wrapper.c : Abstract class for a data_wrapper */

/*
 *
 * Authors: Bertrand Guiheneuf <bertrand@helixcode.com>
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
#include "camel-data-wrapper.h"
#include "camel-exception.h"

#include <errno.h>

#define d(x)

static CamelObjectClass *parent_class = NULL;

/* Returns the class for a CamelDataWrapper */
#define CDW_CLASS(so) CAMEL_DATA_WRAPPER_CLASS (GTK_OBJECT (so)->klass)


static int construct_from_stream(CamelDataWrapper *, CamelStream *);
static int write_to_stream (CamelDataWrapper *data_wrapper, CamelStream *stream);
static void set_mime_type (CamelDataWrapper *data_wrapper, const gchar *mime_type);
static gchar *get_mime_type (CamelDataWrapper *data_wrapper);
static GMimeContentField *get_mime_type_field (CamelDataWrapper *data_wrapper);
static void set_mime_type_field (CamelDataWrapper *data_wrapper, GMimeContentField *mime_type);
static void finalize (GtkObject *object);

static void
camel_data_wrapper_class_init (CamelDataWrapperClass *camel_data_wrapper_class)
{
	GtkObjectClass *gtk_object_class =
		GTK_OBJECT_CLASS (camel_data_wrapper_class);

	parent_class = gtk_type_class (camel_object_get_type ());

	/* virtual method definition */
	camel_data_wrapper_class->write_to_stream = write_to_stream;
	camel_data_wrapper_class->set_mime_type = set_mime_type;
	camel_data_wrapper_class->get_mime_type = get_mime_type;
	camel_data_wrapper_class->get_mime_type_field = get_mime_type_field;
	camel_data_wrapper_class->set_mime_type_field = set_mime_type_field;

	camel_data_wrapper_class->construct_from_stream = construct_from_stream;

	/* virtual method overload */
	gtk_object_class->finalize = finalize;
}

static void
camel_data_wrapper_init (gpointer object, gpointer klass)
{
	CamelDataWrapper *camel_data_wrapper = CAMEL_DATA_WRAPPER (object);

	camel_data_wrapper->mime_type = gmime_content_field_new (NULL, NULL);
}



GtkType
camel_data_wrapper_get_type (void)
{
	static GtkType camel_data_wrapper_type = 0;

	if (!camel_data_wrapper_type) {
		GtkTypeInfo camel_data_wrapper_info =
		{
			"CamelDataWrapper",
			sizeof (CamelDataWrapper),
			sizeof (CamelDataWrapperClass),
			(GtkClassInitFunc) camel_data_wrapper_class_init,
			(GtkObjectInitFunc) camel_data_wrapper_init,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		camel_data_wrapper_type = gtk_type_unique (camel_object_get_type (), &camel_data_wrapper_info);
	}

	return camel_data_wrapper_type;
}


static void
finalize (GtkObject *object)
{
	CamelDataWrapper *camel_data_wrapper = CAMEL_DATA_WRAPPER (object);

	if (camel_data_wrapper->mime_type)
		gmime_content_field_unref (camel_data_wrapper->mime_type);

	if (camel_data_wrapper->stream)
		gtk_object_unref (GTK_OBJECT (camel_data_wrapper->stream));

	GTK_OBJECT_CLASS (parent_class)->finalize (object);
}

static int
write_to_stream (CamelDataWrapper *data_wrapper, CamelStream *stream)
{
	if (data_wrapper->stream == NULL) {
		return -1;
	}

	if (camel_stream_reset (data_wrapper->stream) == -1)
		return -1;

	return camel_stream_write_to_stream (data_wrapper->stream, stream);
}

CamelDataWrapper *
camel_data_wrapper_new(void)
{
	return (CamelDataWrapper *)gtk_type_new(camel_data_wrapper_get_type());
}

/**
 * camel_data_wrapper_write_to_stream:
 * @data_wrapper: a data wrapper
 * @stream: stream for data to be written to
 * @ex: a CamelException
 *
 * Writes the data content to @stream in a machine-independent format
 * appropriate for the data. It should be possible to construct an
 * equivalent data wrapper object later by passing this stream to
 * camel_data_construct_from_stream().
 *
 * Return value: the number of bytes written, or -1 if an error occurs.
 **/
int
camel_data_wrapper_write_to_stream (CamelDataWrapper *data_wrapper,
				    CamelStream *stream)
{
	g_return_val_if_fail (CAMEL_IS_DATA_WRAPPER (data_wrapper), -1);
	g_return_val_if_fail (CAMEL_IS_STREAM (stream), -1);

	return CDW_CLASS (data_wrapper)->write_to_stream (data_wrapper, stream);
}

static int
construct_from_stream (CamelDataWrapper *data_wrapper, CamelStream *stream)
{
	if (data_wrapper->stream)
		gtk_object_unref((GtkObject *)data_wrapper->stream);

	data_wrapper->stream = stream;
	gtk_object_ref (GTK_OBJECT (stream));
	return 0;
}

/**
 * camel_data_wrapper_construct_from_stream:
 * @data_wrapper: a data wrapper
 * @stream: A stream that can be read from.
 *
 * Constructs the content of the data wrapper from the
 * supplied @stream.
 *
 * Return value: -1 on error.
 **/
int
camel_data_wrapper_construct_from_stream (CamelDataWrapper *data_wrapper,
					  CamelStream *stream)
{
	g_return_val_if_fail (CAMEL_IS_DATA_WRAPPER (data_wrapper), -1);
	g_return_val_if_fail (CAMEL_IS_STREAM (stream), -1);

	return CDW_CLASS (data_wrapper)->construct_from_stream (data_wrapper, stream);
}


static void
set_mime_type (CamelDataWrapper *data_wrapper, const gchar *mime_type)
{
	gmime_content_field_construct_from_string (data_wrapper->mime_type,
						   mime_type);
}

/**
 * camel_data_wrapper_set_mime_type:
 * @data_wrapper: a data wrapper
 * @mime_type: the text representation of a MIME type
 *
 * This sets the data wrapper's MIME type.
 * It might fail, but you won't know. It will allow you to set
 * Content-Type parameters on the data wrapper, which are meaningless.
 * You should not be allowed to change the MIME type of a data wrapper
 * that contains data, or at least, if you do, it should invalidate the
 * data.
 **/
void
camel_data_wrapper_set_mime_type (CamelDataWrapper *data_wrapper,
				  const gchar *mime_type)
{
	g_return_if_fail (CAMEL_IS_DATA_WRAPPER (data_wrapper));
	g_return_if_fail (mime_type != NULL);

	CDW_CLASS (data_wrapper)->set_mime_type (data_wrapper, mime_type);
}

static gchar *
get_mime_type (CamelDataWrapper *data_wrapper)
{
	return gmime_content_field_get_mime_type (data_wrapper->mime_type);
}

/**
 * camel_data_wrapper_get_mime_type:
 * @data_wrapper: a data wrapper
 *
 * Return value: the text form of the data wrapper's MIME type
 **/
gchar *
camel_data_wrapper_get_mime_type (CamelDataWrapper *data_wrapper)
{
	g_return_val_if_fail (CAMEL_IS_DATA_WRAPPER (data_wrapper), NULL);

	return CDW_CLASS (data_wrapper)->get_mime_type (data_wrapper);
}


static GMimeContentField *
get_mime_type_field (CamelDataWrapper *data_wrapper)
{
	return data_wrapper->mime_type;
}

/**
 * camel_data_wrapper_get_mime_type_field:
 * @data_wrapper: a data wrapper
 *
 * Return value: the parsed form of the data wrapper's MIME type
 **/
GMimeContentField *
camel_data_wrapper_get_mime_type_field (CamelDataWrapper *data_wrapper)
{
	g_return_val_if_fail (CAMEL_IS_DATA_WRAPPER (data_wrapper), NULL);

	return CDW_CLASS (data_wrapper)->get_mime_type_field (data_wrapper);
}

/**
 * camel_data_wrapper_set_mime_type_field:
 * @data_wrapper: a data wrapper
 * @mime_type: the parsed representation of a MIME type
 *
 * This sets the data wrapper's MIME type. It suffers from the same
 * flaws as camel_data_wrapper_set_mime_type.
 **/
static void
set_mime_type_field (CamelDataWrapper *data_wrapper,
		     GMimeContentField *mime_type)
{
	g_return_if_fail (CAMEL_IS_DATA_WRAPPER (data_wrapper));
	g_return_if_fail (mime_type != NULL);

	if (data_wrapper->mime_type)
		gmime_content_field_unref (data_wrapper->mime_type);
	data_wrapper->mime_type = mime_type;
	if (mime_type)
		gmime_content_field_ref (data_wrapper->mime_type);
}

void
camel_data_wrapper_set_mime_type_field (CamelDataWrapper *data_wrapper,
					GMimeContentField *mime_type)
{
	CDW_CLASS (data_wrapper)->set_mime_type_field (data_wrapper, mime_type);
}
