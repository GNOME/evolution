/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-marshal-utils.c : marshal utils */

/* 
 *
 * Copyright (C) 1999 Bertrand Guiheneuf <Bertrand.Guiheneuf@aful.org> .
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
  register GtkArg *last_param;
  register gboolean failed = FALSE;

  for (last_param = params + func_def->n_params; 
       params < last_param; 
       params++)
    {
      register gchar *error;

      params->name = NULL;
      params->type = *(func_def->params_type++);
      GTK_ARG_COLLECT_VALUE (params,
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


gboolean
camel_marshal_exec_func (CamelFuncDef *func_def, ...)
{
	GtkArg	*params;
	gboolean error;
	va_list args;

	g_assert (func_def);

	params = g_new (GtkArg, func_def->n_params);

	va_start (args, func_def);
	error = _collect_params (params, func_def, args);
	va_end (args);
	if (!error)
		error = func_def->marshal (func_def->func, params);
 
	g_free (params);
	return (!error);
}


CamelOp *
camel_marshal_create_op (CamelFuncDef *func_def, ...)
{
	GtkArg	*params;
	gboolean error;
	CamelOp *op;
	va_list args;

	g_assert (func_def);

	op = camel_op_new (func_def);
	
	va_start (args, func_def);
	error = _collect_params (op->params, func_def, args);
	va_end (args);
	 
	if (error) {
		camel_op_free (op);
		return NULL;
	} else 
		return (op);
}







/* misc marshaller */


typedef void (*CamelMarshal_NONE__POINTER_INT) (gpointer arg1,
						gint arg2);
void camel_marshal_NONE__POINTER_INT (CamelFunc func, 
				      GtkArg *args)
{
	CamelMarshal_NONE__POINTER_INT rfunc;
	rfunc = (CamelMarshal_NONE__POINTER_INT) func;
	(* rfunc) (GTK_VALUE_POINTER(args[0]),
		   GTK_VALUE_INT(args[1]));
}





typedef void (*CamelMarshal_NONE__POINTER_INT_POINTER) (gpointer arg1,
							gint arg2,
							gpointer arg3);
void camel_marshal_NONE__POINTER_INT_POINTER (CamelFunc func, 
					      GtkArg *args)
{
	CamelMarshal_NONE__POINTER_INT_POINTER rfunc;
	rfunc = (CamelMarshal_NONE__POINTER_INT_POINTER) func;
	(* rfunc) (GTK_VALUE_POINTER(args[0]),
		   GTK_VALUE_INT(args[1]),
		   GTK_VALUE_POINTER(args[2]));
}
