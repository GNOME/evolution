/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camelDataWrapper.h : Abstract class for a data wrapper */

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


#ifndef CAMEL_DATA_WRAPPER_H
#define CAMEL_DATA_WRAPPER_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <gtk/gtk.h>
#include "camel-types.h"
#include "gmime-content-field.h"


#define CAMEL_DATA_WRAPPER_TYPE     (camel_data_wrapper_get_type ())
#define CAMEL_DATA_WRAPPER(obj)     (GTK_CHECK_CAST((obj), CAMEL_DATA_WRAPPER_TYPE, CamelDataWrapper))
#define CAMEL_DATA_WRAPPER_CLASS(k) (GTK_CHECK_CLASS_CAST ((k), CAMEL_DATA_WRAPPER_TYPE, CamelDataWrapperClass))
#define CAMEL_IS_DATA_WRAPPER(o)    (GTK_CHECK_TYPE((o), CAMEL_DATA_WRAPPER_TYPE))


struct _CamelDataWrapper
{
	GtkObject parent_object;

	CamelStream *input_stream;
	CamelStream *output_stream;

	GMimeContentField *mime_type;
};



typedef struct {

	GtkObjectClass parent_class;
	
	/* Virtual methods */	
	void                (*set_input_stream)       (CamelDataWrapper *data_wrapper, 
						       CamelStream *stream);
	CamelStream *       (*get_input_stream)       (CamelDataWrapper *data_wrapper);
	void                (*set_output_stream)      (CamelDataWrapper *data_wrapper, 
						       CamelStream *stream);
	CamelStream *       (*get_output_stream)      (CamelDataWrapper *data_wrapper);

	void                (*set_mime_type)          (CamelDataWrapper *data_wrapper, 
						       const gchar * mime_type);
	gchar *             (*get_mime_type)          (CamelDataWrapper *data_wrapper);
	GMimeContentField * (*get_mime_type_field)    (CamelDataWrapper *data_wrapper);
	void                (*set_mime_type_field)    (CamelDataWrapper *data_wrapper, 
						       GMimeContentField *mime_type_field);


	/* deprecated method */
	CamelStream *       (*get_stream)             (CamelDataWrapper *data_wrapper);
	void                (*write_to_stream)        (CamelDataWrapper *data_wrapper, 
						       CamelStream *stream);
	void                (*construct_from_stream)  (CamelDataWrapper *data_wrapper, 
						       CamelStream *stream);

} CamelDataWrapperClass;



/* Standard Gtk function */
GtkType camel_data_wrapper_get_type (void);


/* public methods */

void                camel_data_wrapper_write_to_stream          (CamelDataWrapper *data_wrapper, 
								 CamelStream *stream);
void                camel_data_wrapper_set_mime_type            (CamelDataWrapper *data_wrapper, 
								 const gchar *mime_type);
gchar *             camel_data_wrapper_get_mime_type            (CamelDataWrapper *data_wrapper);
GMimeContentField * camel_data_wrapper_get_mime_type_field      (CamelDataWrapper *data_wrapper);
void                camel_data_wrapper_set_mime_type_field      (CamelDataWrapper *data_wrapper, 
								 GMimeContentField *mime_type);

void                camel_data_wrapper_set_input_stream         (CamelDataWrapper *data_wrapper, 
								 CamelStream *stream);
CamelStream *       camel_data_wrapper_get_input_stream         (CamelDataWrapper *data_wrapper);
void                camel_data_wrapper_set_output_stream        (CamelDataWrapper *data_wrapper, 
								 CamelStream *stream);
CamelStream *       camel_data_wrapper_get_output_stream        (CamelDataWrapper *data_wrapper);



/* deprecated methods. Left until the new parser scheme is ok */
CamelStream *       camel_data_wrapper_get_stream               (CamelDataWrapper *data_wrapper);
void                camel_data_wrapper_construct_from_stream    (CamelDataWrapper *data_wrapper, 
								 CamelStream *stream);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_DATA_WRAPPER_H */
