/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-execpetion.c : exception utils */

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

#include <config.h>
#include "camel-exception.h"

void 
camel_exception_free (CamelException *exception)
{
	if (!exception) return;

	if (exception->desc)
		g_free (exception->desc);

	g_free (exception);
}


CamelException *
camel_exception_new ()
{
	CamelException *ex;

	ex = g_new (CamelException, 1);
	return ex;
}


void
camel_exception_set (CamelException *ex,
		     ExceptionId id,
		     const char *desc)
{
	ex->id = id;
	if (ex->desc)
		g_free (ex->desc);
	ex->desc = g_strdup (desc);
}

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
