/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-marshal-utils.c : marshal utils */

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




#include "config.h"
#include "camel-log.h"
#include "camel-marshal-utils.h"
#include "camel-arg-collector.c"


#define NB_OP_CHUNKS 20
static GMemChunk *op_chunk=NULL;
static GStaticMutex op_chunk_mutex = G_STATIC_MUTEX_INIT;

CamelFuncDef *
camel_func_def_new (CamelMarshal marshal, guint n_params, ...)
{	
	CamelFuncDef *func_def;
	va_list args;
	GtkType type;
	int i;
	
	func_def = g_new (CamelFuncDef, 1);
	func_def->marshal = marshal;
	func_def->n_params = n_params;
	func_def->params_type = g_new (GtkType, n_params);

	va_start (args, n_params);	
	for (i=0; i<n_params; i++) {
		type = va_arg (args, GtkType);
		func_def->params_type [i] = type; 
	}
	va_end (args);

	return func_def;
}




static gboolean
_collect_params (GtkArg	*params,
		 CamelFuncDef *func_def,
		 va_list var_args)
{
  GtkArg *last_param;
  int i;
  gboolean failed = FALSE;
  

  for (i=0; 
       i<func_def->n_params; 
       i++, params++)
    {
      gchar *error;

      params->name = NULL;
      params->type = (func_def->params_type) [i];
      CAMEL_ARG_COLLECT_VALUE (params,
			       var_args,
			       error);
      if (error)
	{
	  failed = TRUE;
	  CAMEL_LOG_FULL_DEBUG ("CamelMarshall::_collect_params(): %s", error);
	  g_free (error);
	}
    }
  return (failed);
}



/**
 * camel_marshal_create_op: create an operation 
 * @func_def: function definition object
 * @func: function to call
 * 
 * create a function ready to be executed. The 
 * vari
 * 
 * 
 * Return value: operation ready to be executed
 **/
CamelOp *
camel_marshal_create_op (CamelFuncDef *func_def, CamelFunc func, ...)
{
	GtkArg	*params;
	gboolean error;
	CamelOp *op;
	va_list args;

	g_assert (func_def);

	op = camel_op_new (func_def);
	op->func = func;

	va_start (args, func);
	error = _collect_params (op->params, func_def, args);
	va_end (args);
	 
	if (error) {
		camel_op_free (op);
		return NULL;
	} else 
		return (op);
}




/**
 * camel_op_new: return a new CamelOp object 
 * 
 * The obtained object must be destroyed with 
 * camel_op_free ()
 * 
 * Return value: the newly allocated CamelOp object
 **/
CamelOp *
camel_op_new (CamelFuncDef *func_def)
{
	CamelOp *op;

	CAMEL_LOG_FULL_DEBUG ("Entering CamelOp::new\n");
	g_static_mutex_lock (&op_chunk_mutex);
	if (!op_chunk)
		op_chunk = g_mem_chunk_create (CamelOp, 
					       NB_OP_CHUNKS,
					       G_ALLOC_AND_FREE);
	g_static_mutex_unlock (&op_chunk_mutex);

	op = g_chunk_new (CamelOp, op_chunk);
	op->func_def = func_def;
	op->params = g_new (GtkArg, func_def->n_params);
	
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelOp::new\n");
	return op;	
}

/**
 * camel_op_free: free a CamelOp object allocated with camel_op_new
 * @op: CamelOp object to free
 * 
 * Free a CamelOp object allocated with camel_op_new ()
 * this routine won't work with CamelOp objects allocated 
 * with other allocators.
 **/
void 
camel_op_free (CamelOp *op)
{
	CAMEL_LOG_FULL_DEBUG ("Entering CamelOp::free\n");
	g_free (op->params);
	g_chunk_free (op, op_chunk);
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelOp::free\n");
}


/**
 * camel_op_run: run an operation 
 * @op: the operation object
 * 
 * run an operation 
 * 
 **/
void
camel_op_run (CamelOp *op)
{
	GtkArg	*params;
	gboolean error;
	
	CAMEL_LOG_FULL_DEBUG ("Entering CamelOp::run\n");
	g_assert (op);
	g_assert (op->func_def);
	g_assert (op->params);

	op->func_def->marshal (op->func, op->params);
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelOp::run\n");
}




/**
 * camel_op_set_user_data: set the private field
 * @op: operation 
 * @user_data: private field
 * 
 * associate a field to an operation object
 **/
void 
camel_op_set_user_data (CamelOp *op, gpointer user_data)
{
	CAMEL_LOG_FULL_DEBUG ("Entering CamelOp::set_user_data\n");
	g_assert (op);
	op->user_data = user_data;
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelOp::set_user_data\n");
}


/**
 * camel_op_get_user_data: return the private field
 * @op: operation object
 * 
 * return the private field associated to 
 * an operation object.
 * 
 * Return value: 
 **/
gpointer 
camel_op_get_user_data (CamelOp *op)
{
	CAMEL_LOG_FULL_DEBUG ("Entering CamelOp::get_user_data\n");
	g_assert (op);
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelOp::get_user_data\n");
	return op->user_data;
}



/* misc marshaller */


typedef void (*CamelMarshal_NONE__POINTER_INT) (gpointer arg1,
						gint arg2);
void camel_marshal_NONE__POINTER_INT (CamelFunc func, 
				      GtkArg *args)
{
	CamelMarshal_NONE__POINTER_INT rfunc;

	CAMEL_LOG_FULL_DEBUG ("Entering camel_marshal_NONE__POINTER_INT\n");
	rfunc = (CamelMarshal_NONE__POINTER_INT) func;
	(* rfunc) (GTK_VALUE_POINTER(args[0]),
		   GTK_VALUE_INT(args[1]));
	CAMEL_LOG_FULL_DEBUG ("Leaving camel_marshal_NONE__POINTER_INT\n");
}





typedef void (*CamelMarshal_NONE__POINTER_INT_POINTER) (gpointer arg1,
							gint arg2,
							gpointer arg3);
void camel_marshal_NONE__POINTER_INT_POINTER (CamelFunc func, 
					      GtkArg *args)
{
	CamelMarshal_NONE__POINTER_INT_POINTER rfunc;

	CAMEL_LOG_FULL_DEBUG ("Entering camel_marshal_NONE__POINTER_INT_POINTER\n");
	rfunc = (CamelMarshal_NONE__POINTER_INT_POINTER) func;
	(* rfunc) (GTK_VALUE_POINTER(args[0]),
		   GTK_VALUE_INT(args[1]),
		   GTK_VALUE_POINTER(args[2]));
	CAMEL_LOG_FULL_DEBUG ("Leaving camel_marshal_NONE__POINTER_INT_POINTER\n");
}


typedef void (*CamelMarshal_NONE__POINTER_BOOL_POINTER) (gpointer arg1,
							 gboolean arg2,
							 gpointer arg3);
void camel_marshal_NONE__POINTER_BOOL_POINTER (CamelFunc func, 
					      GtkArg *args)
{
	CamelMarshal_NONE__POINTER_BOOL_POINTER rfunc;

	CAMEL_LOG_FULL_DEBUG ("Entering camel_marshal_NONE__POINTER_BOOL_POINTER\n");
	rfunc = (CamelMarshal_NONE__POINTER_BOOL_POINTER) func;
	(* rfunc) (GTK_VALUE_POINTER(args[0]),
		   GTK_VALUE_BOOL(args[1]),
		   GTK_VALUE_POINTER(args[2]));
	CAMEL_LOG_FULL_DEBUG ("Leaving camel_marshal_NONE__POINTER_BOOL_POINTER\n");
}


typedef void (*CamelMarshal_NONE__POINTER_INT_POINTER_POINTER) (gpointer arg1,
								gint arg2,
								gpointer arg3,
								gpointer arg4);
void camel_marshal_NONE__POINTER_INT_POINTER_POINTER (CamelFunc func, 
						      GtkArg *args)
{
	CamelMarshal_NONE__POINTER_INT_POINTER_POINTER rfunc;

	CAMEL_LOG_FULL_DEBUG ("Entering camel_marshal_NONE__POINTER_INT_POINTER_POINTER\n");
	rfunc = (CamelMarshal_NONE__POINTER_INT_POINTER_POINTER) func;
	(* rfunc) (GTK_VALUE_POINTER(args[0]),
		   GTK_VALUE_INT(args[1]),
		   GTK_VALUE_POINTER(args[2]),
		   GTK_VALUE_POINTER(args[3]));
	CAMEL_LOG_FULL_DEBUG ("Leaving camel_marshal_NONE__POINTER_INT_POINTER_POINTER\n");
}



typedef void (*CamelMarshal_NONE__POINTER_BOOL_POINTER_POINTER) (gpointer arg1,
								 gboolean arg2,
								 gpointer arg3,
								 gpointer arg4);
void camel_marshal_NONE__POINTER_BOOL_POINTER_POINTER (CamelFunc func, 
						       GtkArg *args)
{
	CamelMarshal_NONE__POINTER_BOOL_POINTER_POINTER rfunc;

	CAMEL_LOG_FULL_DEBUG ("Entering camel_marshal_NONE__POINTER_BOOL_POINTER_POINTER\n");
	rfunc = (CamelMarshal_NONE__POINTER_BOOL_POINTER_POINTER) func;
	(* rfunc) (GTK_VALUE_POINTER(args[0]),
		   GTK_VALUE_BOOL(args[1]),
		   GTK_VALUE_POINTER(args[2]),
		   GTK_VALUE_POINTER(args[3]));
	CAMEL_LOG_FULL_DEBUG ("Leaving camel_marshal_NONE__POINTER_BOOL_POINTER_POINTER\n");
}



typedef void (*CamelMarshal_NONE__POINTER_POINTER_POINTER) (gpointer arg1,
							    gpointer arg2,
							    gpointer arg3);
void camel_marshal_NONE__POINTER_POINTER_POINTER (CamelFunc func, 
						  GtkArg *args)
{
	CamelMarshal_NONE__POINTER_POINTER_POINTER rfunc;

	CAMEL_LOG_FULL_DEBUG ("Entering camel_marshal_NONE__POINTER_POINTER_POINTER\n");
	rfunc = (CamelMarshal_NONE__POINTER_POINTER_POINTER) func;
	(* rfunc) (GTK_VALUE_POINTER(args[0]),
		   GTK_VALUE_POINTER(args[1]),
		   GTK_VALUE_POINTER(args[2]));
	CAMEL_LOG_FULL_DEBUG ("Leaving camel_marshal_NONE__POINTER_POINTER_POINTER\n");
}


typedef void (*CamelMarshal_NONE__INT) (gint arg1);
void camel_marshal_NONE__INT (CamelFunc func, 
			      GtkArg *args)
{
	CamelMarshal_NONE__INT rfunc;

	CAMEL_LOG_FULL_DEBUG ("Entering camel_marshal_NONE__INT\n");
	rfunc = (CamelMarshal_NONE__INT) func;
	(* rfunc) (GTK_VALUE_INT (args[0]));
	CAMEL_LOG_FULL_DEBUG ("Leaving camel_marshal_NONE__INT\n");
}






