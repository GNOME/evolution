/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* camel-stream-data-wrapper.h
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

#ifndef __CAMEL_STREAM_DATA_WRAPPER_H__
#define __CAMEL_STREAM_DATA_WRAPPER_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include "camel-data-wrapper.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */


#define CAMEL_TYPE_STREAM_DATA_WRAPPER \
	(camel_stream_data_wrapper_get_type ())
#define CAMEL_STREAM_DATA_WRAPPER(obj) \
	(GTK_CHECK_CAST ((obj), CAMEL_TYPE_STREAM_DATA_WRAPPER, CamelStreamDataWrapper))
#define CAMEL_STREAM_DATA_WRAPPER_CLASS(klass) \
	(GTK_CHECK_CLASS_CAST ((klass), CAMEL_TYPE_STREAM_DATA_WRAPPER, CamelStreamDataWrapperClass))
#define CAMEL_IS_STREAM_DATA_WRAPPER(obj) \
	(GTK_CHECK_TYPE ((obj), CAMEL_TYPE_STREAM_DATA_WRAPPER))
#define CAMEL_IS_STREAM_DATA_WRAPPER_CLASS(klass) \
	(GTK_CHECK_CLASS_TYPE ((obj), CAMEL_TYPE_STREAM_DATA_WRAPPER))


typedef struct _CamelStreamDataWrapper       CamelStreamDataWrapper;
typedef struct _CamelStreamDataWrapperClass  CamelStreamDataWrapperClass;

struct _CamelStreamDataWrapper {
	CamelDataWrapper parent;

	CamelStream *stream;
};

struct _CamelStreamDataWrapperClass {
	CamelDataWrapperClass parent_class;
};


GtkType camel_stream_data_wrapper_get_type (void);
CamelDataWrapper *camel_stream_data_wrapper_new (CamelStream *stream);
void camel_stream_data_wrapper_construct (CamelStreamDataWrapper *wrapper,
					  CamelStream *stream);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __CAMEL_STREAM_DATA_WRAPPER_H__ */
