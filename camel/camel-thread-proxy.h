/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-folder-pt-proxy.h : proxy folder using posix threads */

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





#ifndef CAMEL_THREAD_PROXY_H
#define CAMEL_THREAD_PROXY_H 1

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include "camel-op-queue.h"

#define CAMEL_THREAD_PROXY(o)  (CamelThreadProxy *)(o)


typedef struct {
	guint signal_id;
	GtkArg *args;
} CamelThreadProxySignalData;


typedef struct {

	GtkObject *real_object;
	GtkObject *proxy_object;
	
	CamelOpQueue *server_op_queue;
	CamelOpQueue *client_op_queue;

  
	gint pipe_client_fd;
	gint pipe_server_fd;
	GIOChannel *notify_source;
	GIOChannel *notify_channel;

	/* signal proxy */
	GMutex *signal_data_mutex;
	GCond *signal_data_cond;
	CamelThreadProxySignalData signal_data;

} CamelThreadProxy;


CamelThreadProxy *camel_thread_proxy_new (void);
void camel_thread_proxy_free (CamelThreadProxy *proxy);

void camel_thread_proxy_push_op (CamelThreadProxy *proxy, CamelOp *op);
void camel_thread_proxy_push_cb (CamelThreadProxy *proxy, CamelOp *cb);

void camel_thread_proxy_add_signals (CamelThreadProxy *proxy, 
				     GtkObject *proxy_object,
				     GtkObject *real_object,
				     char *signal_to_proxy[]);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_THREAD_PROXY_H */


