/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-pop3-store.h : class for an pop3 store */

/* 
 * Authors:
 *   Dan Winship <danw@helixcode.com>
 *
 * Copyright (C) 2000 Helix Code, Inc. (www.helixcode.com)
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


#ifndef CAMEL_POP3_STORE_H
#define CAMEL_POP3_STORE_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include "camel-types.h"
#include "camel-remote-store.h"

#define CAMEL_POP3_STORE_TYPE     (camel_pop3_store_get_type ())
#define CAMEL_POP3_STORE(obj)     (CAMEL_CHECK_CAST((obj), CAMEL_POP3_STORE_TYPE, CamelPop3Store))
#define CAMEL_POP3_STORE_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), CAMEL_POP3_STORE_TYPE, CamelPop3StoreClass))
#define IS_CAMEL_POP3_STORE(o)    (CAMEL_CHECK_TYPE((o), CAMEL_POP3_STORE_TYPE))


typedef struct {
	CamelRemoteStore parent_object;

	char *apop_timestamp, *implementation;
	gboolean supports_top, supports_uidl, expires;
	int login_delay;

} CamelPop3Store;



typedef struct {
	CamelRemoteStoreClass parent_class;

} CamelPop3StoreClass;


/* public methods */
void camel_pop3_store_expunge (CamelPop3Store *store, CamelException *ex);

/* support functions */
enum { CAMEL_POP3_OK, CAMEL_POP3_ERR, CAMEL_POP3_FAIL };
int camel_pop3_command (CamelPop3Store *store, char **ret, CamelException *ex, char *fmt, ...);
char *camel_pop3_command_get_additional_data (CamelPop3Store *store,
					      CamelException *ex);

/* Standard Camel function */
CamelType camel_pop3_store_get_type (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_POP3_STORE_H */


