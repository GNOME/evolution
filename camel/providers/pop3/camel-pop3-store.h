/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-pop3-store.h : class for an pop3 store */

/* 
 * Authors:
 *   Dan Winship <danw@ximian.com>
 *   Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 2000-2002 Ximian, Inc. (www.ximian.com)
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


#ifndef CAMEL_POP3_STORE_H
#define CAMEL_POP3_STORE_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <camel/camel-types.h>
#include <camel/camel-store.h>
#include "camel-pop3-engine.h"

#define CAMEL_POP3_STORE_TYPE     (camel_pop3_store_get_type ())
#define CAMEL_POP3_STORE(obj)     (CAMEL_CHECK_CAST((obj), CAMEL_POP3_STORE_TYPE, CamelPOP3Store))
#define CAMEL_POP3_STORE_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), CAMEL_POP3_STORE_TYPE, CamelPOP3StoreClass))
#define CAMEL_IS_POP3_STORE(o)    (CAMEL_CHECK_TYPE((o), CAMEL_POP3_STORE_TYPE))


typedef struct {
	CamelStore parent_object;

	CamelPOP3Engine *engine; /* pop processing engine */

	struct _CamelDataCache *cache;
} CamelPOP3Store;



typedef struct {
	CamelStoreClass parent_class;

} CamelPOP3StoreClass;


/* public methods */
void camel_pop3_store_expunge (CamelPOP3Store *store, CamelException *ex);

/* support functions */
enum { CAMEL_POP3_OK, CAMEL_POP3_ERR, CAMEL_POP3_FAIL };
int camel_pop3_command (CamelPOP3Store *store, char **ret, CamelException *ex, char *fmt, ...);
char *camel_pop3_command_get_additional_data (CamelPOP3Store *store, int total, CamelException *ex);

/* Standard Camel function */
CamelType camel_pop3_store_get_type (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_POP3_STORE_H */


