/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camelDataWrapper.h : Abstract class for a data wrapper */

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


#ifndef CAMEL_DATA_WRAPPER_H
#define CAMEL_DATA_WRAPPER_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <gtk/gtk.h>
#include <stdio.h>
#include "gmime-content-field.h"
#include "camel-stream.h"



#define CAMEL_DATA_WRAPPER_TYPE     (camel_data_wrapper_get_type ())
#define CAMEL_DATA_WRAPPER(obj)     (GTK_CHECK_CAST((obj), CAMEL_DATA_WRAPPER_TYPE, CamelDataWrapper))
#define CAMEL_DATA_WRAPPER_CLASS(k) (GTK_CHECK_CLASS_CAST ((k), CAMEL_DATA_WRAPPER_TYPE, CamelDataWrapperClass))
#define IS_CAMEL_DATA_WRAPPER(o)    (GTK_CHECK_TYPE((o), CAMEL_DATA_WRAPPER_TYPE))


typedef struct 
{
	GtkObject parent_object;

	GMimeContentField *content_type;
	
} CamelDataWrapper;



typedef struct {
	GtkObjectClass parent_class;
	
	/* Virtual methods */	
	void  (*write_to_stream) (CamelDataWrapper *data_wrapper, CamelStream *stream);
	void  (*construct_from_stream) (CamelDataWrapper *data_wrapper, CamelStream *stream, guint size);
	void  (*set_content_type) (CamelDataWrapper *data_wrapper, GString *content_type);
	GString * (*get_content_type) (CamelDataWrapper *data_wrapper);

} CamelDataWrapperClass;



/* Standard Gtk function */
GtkType camel_data_wrapper_get_type (void);


/* public methods */

void camel_data_wrapper_write_to_stream (CamelDataWrapper *data_wrapper, CamelStream *stream);
void camel_data_wrapper_construct_from_stream (CamelDataWrapper *data_wrapper, CamelStream *stream, guint size);
void camel_data_wrapper_set_content_type (CamelDataWrapper *data_wrapper, GString *content_type);
static GString *camel_data_wrapper_get_content_type (CamelDataWrapper *data_wrapper);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_DATA_WRAPPER_H */
