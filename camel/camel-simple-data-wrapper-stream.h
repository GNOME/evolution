/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* camel-simple-data-wrapper-stream.h
 *
 * Copyright 1999, 2000 HelixCode (http://www.helixcode.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ettore Perazzoli
 */

#ifndef __CAMEL_SIMPLE_DATA_WRAPPER_STREAM_H__
#define __CAMEL_SIMPLE_DATA_WRAPPER_STREAM_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

#include "camel-simple-data-wrapper.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define CAMEL_TYPE_SIMPLE_DATA_WRAPPER_STREAM \
	(camel_simple_data_wrapper_stream_get_type ())
#define CAMEL_SIMPLE_DATA_WRAPPER_STREAM(obj) \
	(GTK_CHECK_CAST ((obj), CAMEL_TYPE_SIMPLE_DATA_WRAPPER_STREAM, CamelSimpleDataWrapperStream))
#define CAMEL_SIMPLE_DATA_WRAPPER_STREAM_CLASS(klass) \
	(GTK_CHECK_CLASS_CAST ((klass), CAMEL_TYPE_SIMPLE_DATA_WRAPPER_STREAM, CamelSimpleDataWrapperStreamClass))
#define CAMEL_IS_SIMPLE_DATA_WRAPPER_STREAM(obj) \
	(GTK_CHECK_TYPE ((obj), CAMEL_TYPE_SIMPLE_DATA_WRAPPER_STREAM))
#define CAMEL_IS_SIMPLE_DATA_WRAPPER_STREAM_CLASS(klass) \
	(GTK_CHECK_CLASS_TYPE ((obj), CAMEL_TYPE_SIMPLE_DATA_WRAPPER_STREAM))


typedef struct _CamelSimpleDataWrapperStream       CamelSimpleDataWrapperStream;
typedef struct _CamelSimpleDataWrapperStreamClass  CamelSimpleDataWrapperStreamClass;

struct _CamelSimpleDataWrapperStream {
	CamelStream parent;

	CamelSimpleDataWrapper *wrapper;
	gint current_position;
};

struct _CamelSimpleDataWrapperStreamClass {
	CamelStreamClass parent_class;
};


GtkType camel_simple_data_wrapper_stream_get_type (void);
CamelStream *camel_simple_data_wrapper_stream_new (CamelSimpleDataWrapper *wrapper);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __CAMEL_SIMPLE_DATA_WRAPPER_STREAM_H__ */
