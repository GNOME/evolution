/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-execpetion.h : exception utils */

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



#ifndef CAMEL_EXCEPTION_H
#define CAMEL_EXCEPTION_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <glib.h>
#include <camel/camel-types.h>

typedef enum {
#include "camel-exception-list.def"

} ExceptionId;


struct _CamelException {
	/* do not access the fields directly */
	ExceptionId id;
	char *desc;

};



/* creation and destruction functions */
CamelException *          camel_exception_new           (void);
void                      camel_exception_free          (CamelException *exception);
void                      camel_exception_init          (CamelException *ex);


/* exception content manipulation */
void                      camel_exception_clear         (CamelException *exception);
void                      camel_exception_set           (CamelException *ex,
							 ExceptionId id,
							 const char *desc);
void                      camel_exception_setv          (CamelException *ex,
							 ExceptionId id,
							 const char *format,  
							 ...);


/* exception content transfer */
void                      camel_exception_xfer          (CamelException *ex_dst,
							 CamelException *ex_src);


/* exception content retrieval */
ExceptionId               camel_exception_get_id        (CamelException *ex);
const gchar *             camel_exception_get_description (CamelException *ex);

#define camel_exception_is_set(ex) (camel_exception_get_id (ex) != CAMEL_EXCEPTION_NONE)

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_EXCEPTION_H */

