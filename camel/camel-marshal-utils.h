/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-maeshal-utils.h : marshal utils */

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



#ifndef CAMEL_MARSHAL_UTILS_H
#define CAMEL_MARSHAL_UTILS_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <gtk/gtk.h>


typedef void (*CamelFunc) ();

typedef void ( *CamelMarshal) (CamelFunc func,
				   GtkArg *args);





typedef struct {

	CamelMarshal marshal;
	guint n_params;
	GtkType	 *params_type;

} CamelFuncDef;



typedef struct {
	CamelFuncDef *func_def;
	CamelFunc func;
	GtkArg	*params;
	gpointer user_data;
} CamelOp;


CamelFuncDef *
camel_func_def_new (CamelMarshal marshal, 
		    guint n_params, 
		    ...);


CamelOp *camel_op_new (CamelFuncDef *func_def);
void camel_op_free (CamelOp *op);
void camel_op_run (CamelOp *op);
void camel_op_run_and_free (CamelOp *op);
void camel_op_set_user_data (CamelOp *op, gpointer user_data);
gpointer camel_op_get_user_data (CamelOp *op);

CamelOp *camel_marshal_create_op (CamelFuncDef *func_def, CamelFunc func, ...);

/* marshallers */
void camel_marshal_NONE__POINTER_INT_POINTER (CamelFunc func, 
					      GtkArg *args);
void camel_marshal_NONE__POINTER_BOOL_POINTER (CamelFunc func, 
					      GtkArg *args);
void camel_marshal_NONE__POINTER_INT_POINTER_POINTER (CamelFunc func, 
						      GtkArg *args);
void camel_marshal_NONE__POINTER_BOOL_POINTER_POINTER (CamelFunc func, 
						       GtkArg *args);
void camel_marshal_NONE__POINTER_POINTER_POINTER (CamelFunc func, 
						  GtkArg *args);
void camel_marshal_NONE__INT (CamelFunc func, 
			      GtkArg *args);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_MARSHAL_UTILS_H */

