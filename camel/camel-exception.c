/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-execpetion.c : exception utils */

/* 
 *
 * Author : 
 *  Bertrand Guiheneuf <Bertrand.Guiheneuf@aful.org>
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

#include <config.h>
#include "camel-exception.h"



/**
 * camel_exception_new: allocate a new exception object. 
 * 
 * Create and returns a new exception object.
 * 
 * 
 * Return value: The newly allocated exception object.
 **/
CamelException *
camel_exception_new ()
{
	CamelException *ex;

	ex = g_new (CamelException, 1);
	return ex;
}

/**
 * camel_exception_free: Free an exception 
 * @exception: The exception object to free
 * 
 * Free an exception object. If the exception
 * is NULL, nothing is done, the routine simply
 * returns.
 **/
void 
camel_exception_free (CamelException *exception)
{
	if (!exception) return;
	
	/* free the description text */
	if (exception->desc)
		g_free (exception->desc);
       	/* free the exeption itself */
	g_free (exception);
}

/**
 * camel_exception_set: set an exception 
 * @ex: exception object 
 * @id: exception id 
 * @desc: textual description of the exception
 * 
 * Set the value of an exception. The exception id is 
 * a unique number representing the exception. The 
 * textual description is a small text explaining 
 * what happened and provoked the exception.
 *
 * When @ex is NULL, nothing is done, this routine
 * simply returns.
 *
 **/
void
camel_exception_set (CamelException *ex,
		     ExceptionId id,
		     const char *desc)
{
	/* if no exception is given, do nothing */
	if (!ex) return;

	ex->id = id;

	/* remove the previous exception description */
	if (ex->desc)
		g_free (ex->desc);
	ex->desc = g_strdup (desc);
}



/**
 * camel_exception_xfer: transfer an exception
 * @ex_dst: Destination exception object 
 * @ex_src: Source exception object
 * 
 * Transfer the content of an exception from
 * an exception object to another. 
 * The destination exception receives the id and
 * the description text of the source exception. 
 **/
void 
camel_exception_xfer (CamelException *ex_dst,
		      CamelException *ex_src)
{
	if (ex_dst->desc)
		g_free (ex_dst->desc);

	ex_dst->id = ex_src->id;
	ex_dst->desc = ex_src->desc;

	ex_src->desc = NULL;
	ex_src->id = CAMEL_EXCEPTION_NONE;
}
