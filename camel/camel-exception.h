/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-execpetion.h : exception utils */

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



#ifndef CAMEL_EXCEPTION_H
#define CAMEL_EXCEPTION_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <glib.h>


typedef enum {
#include "camel-exception-list.def"

} ExceptionId;

typedef struct {

	ExceptionId id;
	char *desc;

} CamelException;

void camel_exception_free (CamelException *exception);
CamelException *camel_exception_new ();
void camel_exception_set (CamelException *ex,
			  ExceptionId id,
			  const char *desc);
void camel_exception_xfer (CamelException *ex_dst,
			   CamelException *ex_src);



#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_EXCEPTION_H */

