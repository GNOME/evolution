/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-simple-data-wrapper.c: a simple stream-backed data wrapper */

/*
 * Author:
 *   Dan Winship <danw@helixcode.com>
 *
 * Copyright 2000 Helix Code, Inc. (http://www.helixcode.com)
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
#include "camel-exception.h"

#include <errno.h>

static CamelDataWrapperClass *parent_class = NULL;

/* Returns the class for a CamelDataWrapper */
#define CDW_CLASS(so) CAMEL_DATA_WRAPPER_CLASS (GTK_OBJECT (so)->klass)


static int construct_from_stream (CamelDataWrapper *, CamelStream *);
static int write_to_stream (CamelDataWrapper *data_wrapper,
			    CamelStream *stream);

static void finalize (GtkObject *object);

static void
camel_simple_data_wrapper_class_init (CamelSimpleDataWrapperClass *camel_simple_data_wrapper_class)
{
	GtkObjectClass *gtk_object_class =
		GTK_OBJECT_CLASS (camel_simple_data_wrapper_class);
	CamelDataWrapperClass *camel_data_wrapper_class =
		CAMEL_DATA_WRAPPER_CLASS (camel_simple_data_wrapper_class);

	parent_class = gtk_type_class (camel_data_wrapper_get_type ());

	/* virtual method overload */
	camel_data_wrapper_class->write_to_stream = write_to_stream;
	camel_data_wrapper_class->construct_from_stream =
		construct_from_stream;

	gtk_object_class->finalize = finalize;
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
			(GtkObjectInitFunc) NULL,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		camel_simple_data_wrapper_type = gtk_type_unique (camel_data_wrapper_get_type (), &camel_simple_data_wrapper_info);
	}

	return camel_simple_data_wrapper_type;
}


static void
finalize (GtkObject *object)
{
	CamelSimpleDataWrapper *simple_data_wrapper =
		CAMEL_SIMPLE_DATA_WRAPPER (object);

	if (simple_data_wrapper->content)
		gtk_object_unref (GTK_OBJECT (simple_data_wrapper->content));

	GTK_OBJECT_CLASS (parent_class)->finalize (object);
}

/**
 * camel_simple_data_wrapper_new:
 * 
 * Create a new camel data wrapper object.
 * 
 * Return value: 
 **/
CamelDataWrapper *
camel_simple_data_wrapper_new (void)
{
	return gtk_type_new (camel_simple_data_wrapper_get_type ());
}


static int
construct_from_stream (CamelDataWrapper *wrapper, CamelStream *stream)
{
	CamelSimpleDataWrapper *simple_data_wrapper =
		CAMEL_SIMPLE_DATA_WRAPPER (wrapper);

	if (simple_data_wrapper->content)
		gtk_object_unref((GtkObject *)simple_data_wrapper->content);

	simple_data_wrapper->content = stream;
	gtk_object_ref (GTK_OBJECT (stream));
	return 0;
}


static int
write_to_stream (CamelDataWrapper *data_wrapper, CamelStream *stream)
{
	CamelSimpleDataWrapper *simple_data_wrapper =
		CAMEL_SIMPLE_DATA_WRAPPER (data_wrapper);

	if (simple_data_wrapper->content == NULL)
		return -1;

	if (camel_stream_reset (simple_data_wrapper->content) == -1)
		return -1;

	return camel_stream_write_to_stream (simple_data_wrapper->content, stream);
}
