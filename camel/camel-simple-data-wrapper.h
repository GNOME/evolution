/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-simple-data-wrapper.c : simple implementation of a data wrapper */
/* store the data in a glib byte array                                   */

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


#ifndef CAMEL_SIMPLE_DATA_WRAPPER_H
#define CAMEL_SIMPLE_DATA_WRAPPER_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <gtk/gtk.h>
#include <stdio.h>
#include "camel-stream.h"
#include "camel-data-wrapper.h"



#define CAMEL_SIMPLE_DATA_WRAPPER_TYPE     (camel_simple_data_wrapper_get_type ())
#define CAMEL_SIMPLE_DATA_WRAPPER(obj)     (GTK_CHECK_CAST((obj), CAMEL_SIMPLE_DATA_WRAPPER_TYPE, CamelSimpleDataWrapper))
#define CAMEL_SIMPLE_DATA_WRAPPER_CLASS(k) (GTK_CHECK_CLASS_CAST ((k), CAMEL_SIMPLE_DATA_WRAPPER_TYPE, CamelSimpleDataWrapperClass))
#define CAMEL_IS_SIMPLE_DATA_WRAPPER(o)    (GTK_CHECK_TYPE((o), CAMEL_SIMPLE_DATA_WRAPPER_TYPE))


typedef struct 
{
	CamelDataWrapper parent_object;

	GByteArray *byte_array;
	CamelStream *stream;
} CamelSimpleDataWrapper;



typedef struct {
	CamelDataWrapperClass parent_class;
	

} CamelSimpleDataWrapperClass;



/* Standard Gtk function */
GtkType camel_simple_data_wrapper_get_type (void);


/* public methods */

CamelSimpleDataWrapper *camel_simple_data_wrapper_new ();
void camel_simple_data_wrapper_set_text (CamelSimpleDataWrapper *simple_data_wrapper, const gchar *text);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_SIMPLE_DATA_WRAPPER_H */
