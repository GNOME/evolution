/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-remote-store.h : class for a remote store */

/* 
 * Authors: Peter Williams <peterw@helixcode.com>
 *
 * Copyright (C) 2000 Helix Code, Inc.
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

#ifndef CAMEL_REMOTE_STORE_H
#define CAMEL_REMOTE_STORE_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include "camel-store.h"

#define CAMEL_REMOTE_STORE_TYPE     (camel_remote_store_get_type ())
#define CAMEL_REMOTE_STORE(obj)     (CAMEL_CHECK_CAST((obj), CAMEL_REMOTE_STORE_TYPE, CamelRemoteStore))
#define CAMEL_REMOTE_STORE_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), CAMEL_REMOTE_STORE_TYPE, CamelRemoteStoreClass))
#define CAMEL_IS_REMOTE_STORE(o)    (CAMEL_CHECK_TYPE((o), CAMEL_REMOTE_STORE_TYPE))

typedef struct {
	CamelStore parent_object;	
	
	CamelStream *istream, *ostream;
	guint timeout_id;
} CamelRemoteStore;


typedef struct {
	CamelStoreClass parent_class;

	gint (*send_string)   (CamelRemoteStore *store, CamelException *ex, 
			       char *fmt, va_list ap);
	gint (*send_stream)   (CamelRemoteStore *store, CamelStream *stream, 
			       CamelException *ex);
	gint (*recv_line)     (CamelRemoteStore *store, char **dest, 
			       CamelException *ex);
	void (*keepalive)     (CamelRemoteStore *store);
} CamelRemoteStoreClass;


/* Standard Camel function */
CamelType camel_remote_store_get_type (void);

/* Extra public functions */
gint camel_remote_store_send_string (CamelRemoteStore *store, CamelException *ex,
				     char *fmt, ...);
gint camel_remote_store_send_stream (CamelRemoteStore *store, CamelStream *stream, 
				     CamelException *ex);
gint camel_remote_store_recv_line (CamelRemoteStore *store, char **dest,
				   CamelException *ex);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_REMOTE_STORE_H */


