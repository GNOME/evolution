/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 *
 * Author : 
 *  Bertrand Guiheneuf <bertrand@helixcode.com>
 *
 * Copyright 1999-2003 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of version 2 of the GNU General Public 
 * License as published by the Free Software Foundation.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <glib.h>
#include <pthread.h>

#include "camel-exception.h"
#include "libedataserver/e-memory.h"

#include "camel-debug.h"

/* dont turn this off */
#define w(x) x

/* i dont know why gthread_mutex stuff even exists, this is easier */

/* also, i'm not convinced mutexes are needed here.  But it
   doesn't really hurt either */
static pthread_mutex_t exception_mutex = PTHREAD_MUTEX_INITIALIZER;

#define CAMEL_EXCEPTION_LOCK(e) (pthread_mutex_lock(&exception_mutex))
#define CAMEL_EXCEPTION_UNLOCK(e) (pthread_mutex_unlock(&exception_mutex))

static EMemChunk *exception_chunks = NULL;

/**
 * camel_exception_new: allocate a new exception object. 
 * 
 * Create and returns a new exception object.
 * 
 * 
 * Return value: The newly allocated exception object.
 **/
CamelException *
camel_exception_new (void)
{
	CamelException *ex;

	CAMEL_EXCEPTION_LOCK(exception);

	if (exception_chunks == NULL)
		exception_chunks = e_memchunk_new(16, sizeof(CamelException));

	ex = e_memchunk_alloc(exception_chunks);
	ex->desc = NULL;

	/* set the Exception Id to NULL */
	ex->id = CAMEL_EXCEPTION_NONE;

	CAMEL_EXCEPTION_UNLOCK(exception);

	return ex;
}

/**
 * camel_exception_init: init a (statically allocated) exception. 
 * 
 * Init an exception. This routine is mainly
 * useful when using a statically allocated
 * exception. 
 * 
 * 
 **/
void
camel_exception_init (CamelException *ex)
{
	ex->desc = NULL;

	/* set the Exception Id to NULL */
	ex->id = CAMEL_EXCEPTION_NONE;
}


/**
 * camel_exception_clear: Clear an exception
 * @exception: the exception object
 * 
 * Clear an exception, that is, set the 
 * exception ID to CAMEL_EXCEPTION_NONE and
 * free the description text.
 * If the exception is NULL, this funtion just
 * returns.
 **/
void 
camel_exception_clear (CamelException *exception)
{
	if (!exception)
		return;

	CAMEL_EXCEPTION_LOCK(exception);

	if (exception->desc)
		g_free (exception->desc);
	exception->desc = NULL;
	exception->id = CAMEL_EXCEPTION_NONE;

	CAMEL_EXCEPTION_UNLOCK(exception);
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
	if (!exception)
		return;
	
	if (exception->desc)
		g_free (exception->desc);

	CAMEL_EXCEPTION_LOCK(exception);

	e_memchunk_free(exception_chunks, exception);

	CAMEL_EXCEPTION_UNLOCK(exception);
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
	if (camel_debug("exception"))
		printf("CamelException.set(%p, %d, '%s')\n", ex, id, desc);

	if (!ex)
		return;

	CAMEL_EXCEPTION_LOCK(exception);

	ex->id = id;

	if (desc != ex->desc) {
		g_free (ex->desc);
		ex->desc = g_strdup (desc);
	}

	CAMEL_EXCEPTION_UNLOCK(exception);
}

/**
 * camel_exception_setv: set an exception 
 * @ex: exception object 
 * @id: exception id 
 * @format: format of the description string. The format string is
 * used as in printf().
 * 
 * Set the value of an exception. The exception id is 
 * a unique number representing the exception. The 
 * textual description is a small text explaining 
 * what happened and provoked the exception. 
 * In this version, the string is created from the format 
 * string and the variable argument list.
 *
 * It is safe to say:
 *   camel_exception_setv (ex, ..., camel_exception_get_description (ex), ...);
 *
 * When @ex is NULL, nothing is done, this routine
 * simply returns.
 *
 **/
void
camel_exception_setv (CamelException *ex,
		      ExceptionId id,
		      const char *format, 
		      ...)
{
	va_list args;
	char *desc;

	va_start(args, format);
	desc = g_strdup_vprintf (format, args);
	va_end (args);

	if (camel_debug("exception"))
		printf("CamelException.setv(%p, %d, '%s')\n", ex, id, desc);
	
	if (!ex) {
		g_free(desc);
		return;
	}

	CAMEL_EXCEPTION_LOCK(exception);

	g_free(ex->desc);
	ex->desc = desc;
	ex->id = id;

	CAMEL_EXCEPTION_UNLOCK(exception);
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
	if (ex_src == NULL) {
		w(g_warning ("camel_exception_xfer: trying to transfer NULL exception to %p\n", ex_dst));
		return;
	}

	if (ex_dst == NULL) {
		/* must have same side-effects */
		camel_exception_clear (ex_src);
		return;
	}

	CAMEL_EXCEPTION_LOCK(exception);

	if (ex_dst->desc)
		g_free (ex_dst->desc);

	ex_dst->id = ex_src->id;
	ex_dst->desc = ex_src->desc;

	ex_src->desc = NULL;
	ex_src->id = CAMEL_EXCEPTION_NONE;

	CAMEL_EXCEPTION_UNLOCK(exception);
}

/**
 * camel_exception_get_id: get the exception id
 * @ex: The exception object
 * 
 * Return the id of an exception. 
 * If @ex is NULL, return CAMEL_EXCEPTION_NONE;
 * 
 * Return value: Exception ID.
 **/
ExceptionId
camel_exception_get_id (CamelException *ex)
{
	if (ex)
		return ex->id;
	
	w(g_warning ("camel_exception_get_id called with NULL parameter."));
	
	return CAMEL_EXCEPTION_NONE;
}

/**
 * camel_exception_get_description: get the description of an exception.
 * @ex: The exception object
 * 
 * Return the exception description text. 
 * If @ex is NULL, return NULL;
 * 
 * 
 * Return value: Exception description text.
 **/
const gchar *
camel_exception_get_description (CamelException *ex)
{
	char *ret = NULL;

	if (ex)
		ret = ex->desc;
	else
		w(g_warning ("camel_exception_get_description called with NULL parameter."));
		
	return ret;
}
