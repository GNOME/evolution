/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-folder-pt-proxy.c : proxy folder using posix threads */

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
#include "camel-log.h"
#include "camel-marshal-utils.h"
#include "camel-thread-proxy.h"
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>



/* vocabulary: 
 *  operation: commanded by the main thread, executed by the child thread
 *  callback: commanded by the child thread, generally when an operation is 
 *    completed. Executed in the main thread, 
 */

/* needed for proper casts of async funcs when 
 * calling pthreads_create
 */
typedef void * (*thread_call_func) (void *);

/* forward declarations */
static gboolean  
_thread_notification_catch (GIOChannel *source,
			    GIOCondition condition,
			    gpointer data);

static void
_notify_availability (CamelThreadProxy *proxy, gchar op_name);

static int
_init_notify_system (CamelThreadProxy *proxy);



/**
 * camel_thread_proxy_new: create a new proxy object
 *  
 * Create a new proxy object. This proxy object can be used 
 * to run async operations and this operations can trigger 
 * callbacks. It can also be used to proxy signals.
 * 
 * Return value: The newly created proxy object
 **/
CamelThreadProxy *
camel_thread_proxy_new ()
{
	CamelThreadProxy *proxy;
	
	CAMEL_LOG_FULL_DEBUG ("Entering CamelThreadProxy::new\n");

	proxy = g_new (CamelThreadProxy, 1);
	if (!proxy)
		return NULL;

	proxy->server_op_queue = camel_op_queue_new ();
	proxy->client_op_queue = camel_op_queue_new ();
	proxy->signal_data_cond = g_cond_new();
	proxy->signal_data_mutex = g_mutex_new();
	if (_init_notify_system (proxy) < 0) {
		g_free (proxy);
		return NULL;
	}
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelThreadProxy::new\n");
	return proxy;
}


/**
 * camel_thread_proxy_free: free a proxy object
 * @proxy: proxy object to free
 * 
 * free a proxy object
 **/
void 
camel_thread_proxy_free (CamelThreadProxy *proxy)
{
	CAMEL_LOG_FULL_DEBUG ("Entering CamelThreadProxy::free\n");
	
	g_cond_free (proxy->signal_data_cond);
	g_mutex_free (proxy->signal_data_mutex);
	camel_op_queue_free (proxy->server_op_queue);
	camel_op_queue_free (proxy->client_op_queue);

	CAMEL_LOG_FULL_DEBUG ("Leaving CamelThreadProxy::free\n");
}





/* Operations handling */


/**
 * _op_run_free_notify:
 * @folder: folder to notify when the operation is completed. 
 * @op: operation to run. 
 * 
 * run an operation, free the operation field
 * and then notify the main thread of the op
 * completion.
 * 
 * this routine is intended to be called 
 * in a new thread (in _run_next_op_in_thread)
 * 
 **/
void
_op_run_free_and_notify (CamelOp *op)
{
	gboolean error;
	CamelThreadProxy *th_proxy;
	
	CAMEL_LOG_FULL_DEBUG ("Entering CamelThreadProxy::_op_run_free_and_notify\n");

	camel_op_run (op);
	camel_op_free (op);
	th_proxy = camel_op_get_user_data (op);
	_notify_availability (th_proxy, 'a');

	CAMEL_LOG_FULL_DEBUG ("Leaving CamelThreadProxy::_op_run_free_and_notify\n");
}


/**
 * _run_next_op_in_thread:  
 * @proxy_object: 
 * 
 * run the next operation pending in the proxy 
 * operation queue
 **/
static void 
_run_next_op_in_thread (CamelThreadProxy *proxy)
{
	CamelOp *op;
	CamelOpQueue *server_op_queue;
	CamelThreadProxy *th_proxy;
	pthread_t thread;

	CAMEL_LOG_FULL_DEBUG ("Entering CamelThreadProxy::_run_next_op_in_thread\n");

	server_op_queue = proxy->server_op_queue;
	/* get the next pending operation */
	op = camel_op_queue_pop_op (server_op_queue);
	if (!op) {
		camel_op_queue_set_service_availability (server_op_queue, TRUE);
		return;
	}
	
	/* run the operation in a child thread */
	pthread_create (&thread, NULL, (thread_call_func) _op_run_free_and_notify, op);

	CAMEL_LOG_FULL_DEBUG ("Leaving CamelThreadProxy::_run_next_op_in_thread\n");
}



/**
 * camel_thread_proxy_push_op: push an operation in the proxy operation queue
 * @proxy: proxy object 
 * @op: operation to push in the execution queue
 * 
 * if no thread is currently running, executes the 
 * operation directly, otherwise push the operation 
 * in the proxy operation queue.
 **/
void 
camel_thread_proxy_push_op (CamelThreadProxy *proxy, CamelOp *op)
{
	CamelOpQueue *server_op_queue;
	
	CAMEL_LOG_FULL_DEBUG ("Entering CamelThreadProxy::camel_thread_proxy_push_op\n");

	g_assert (proxy);
	server_op_queue = proxy->server_op_queue;
	
	/* put the proxy object in the user data
	   so that it can be notified when the 
	   operation is completed */
	camel_op_set_user_data (op, (gpointer)proxy);
	
	/* get next operation */
	camel_op_queue_push_op (server_op_queue, op);
	
	if (camel_op_queue_get_service_availability (server_op_queue)) {
		/* no thread is currently running, run 
		 * the next operation. */
		camel_op_queue_set_service_availability (server_op_queue, FALSE);
		/* when the operation is completed in the 
		   child thread the main thread gets 
		   notified and executes next operation 
		   (see _thread_notification_catch, case 'a')
		   so there is no need to set the service
		   availability to FALSE except here 
		*/
		_run_next_op_in_thread (proxy);		
	}
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelThreadProxy::camel_thread_proxy_push_op\n");
}
/**
 * _op_run_and_free: Run an operation and free it
 * @op: Operation object
 * 
 * Run an operation object in the current thread 
 * and free it.
 **/
static void
_op_run_and_free (CamelOp *op)
{
	CAMEL_LOG_FULL_DEBUG ("Entering CamelThreadProxy::_op_run_and_free\n");
	camel_op_run (op);
	camel_op_free (op);
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelThreadProxy::_op_run_and_free\n");
}






/* Callbacks handling */

/**
 * _run_next_cb: Run next callback pending in a proxy object 
 * @proxy: Proxy object 
 * 
 * Run next callback in the callback queue of a proxy object 
 **/
static void 
_run_next_cb (CamelThreadProxy *proxy)
{
	CamelOp *op;
	CamelOpQueue *client_op_queue;
	CamelThreadProxy *th_proxy;

	CAMEL_LOG_FULL_DEBUG ("Entering CamelThreadProxy::_run_next_cb\n");
	client_op_queue = proxy->client_op_queue;

	/* get the next pending operation */
	op = camel_op_queue_pop_op (client_op_queue);
	if (!op) return;
	
	/* run the operation in the main thread */
	_op_run_and_free (op);

	CAMEL_LOG_FULL_DEBUG ("Leaving CamelThreadProxy::_run_next_cb\n");
}


/**
 * camel_thread_proxy_push_cb: push a callback in the client queue
 * @proxy: proxy object concerned by the callback
 * @cb: callback to push
 * 
 * Push an operation in the client queue, ie the queue 
 * containing the operations (callbacks) intended to be
 * executed in the main thread.
 **/
void 
camel_thread_proxy_push_cb (CamelThreadProxy *proxy, CamelOp *cb)
{
	CamelOpQueue *client_op_queue;
	
	CAMEL_LOG_FULL_DEBUG ("Entering CamelThreadProxy::camel_thread_proxy_push_cb\n");
	client_op_queue = proxy->client_op_queue;

	/* put the proxy object in the user data
	   so that it can be notified when the 
	   operation is completed */
	camel_op_set_user_data (cb, (gpointer)proxy);
	
	/* push the callback in the client queue */
	camel_op_queue_push_op (client_op_queue, cb);
	
	/* tell the main thread a new callback is there */
	_notify_availability (proxy, 'c');
	
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelThreadProxy::camel_thread_proxy_push_cb\n");
}  



/**
 * _init_notify_system: set the notify channel up
 * @proxy: proxy object 
 * 
 * called once to set the notification channel up
 **/
static int
_init_notify_system (CamelThreadProxy *proxy)
{
	int filedes[2];

	CAMEL_LOG_FULL_DEBUG ("Entering CamelThreadProxy::_init_notify_system\n");

	/* set up the notification channel */
	if (pipe (filedes) < 0) {
		CAMEL_LOG_WARNING ("could not create pipe in CamelThreadProxy::_init_notify_system\n");
		CAMEL_LOG_FULL_DEBUG ("Full error message : %s\n", strerror(errno));
		return -1;
	}
	
	
	proxy->pipe_client_fd = filedes [0];
	proxy->pipe_server_fd = filedes [1];
	proxy->notify_source =  g_io_channel_unix_new (filedes [0]);
	proxy->notify_channel =  g_io_channel_unix_new (filedes [1]);
	
	/* the _thread_notification_catch function 
	* will be called in the main thread when the 
	* child thread writes some data in the channel */ 
	g_io_add_watch (proxy->notify_source, G_IO_IN,
			_thread_notification_catch, 
			proxy);
	
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelThreadProxy::_init_notify_system\n");
	return 1;
}

/**
 * _notify_availability: notify the main thread from an event
 * @proxy: proxy object
 * @op_name: operation name
 *
 * called by child thread  to notify the main 
 * thread  something is available for him.
 * What this thing is depends on  @op_name:
 *
 * 'a' : thread available. That means the thread is ready 
 *       to process an operation. 
 * 's' : a signal is available. Used by the signal proxy.
 *
 */
static void
_notify_availability (CamelThreadProxy *proxy, gchar op_name)
{
	GIOChannel *notification_channel;
	guint bytes_written;

	CAMEL_LOG_FULL_DEBUG ("Entering CamelThreadProxy::_notify_availability\n");

	notification_channel = proxy->notify_channel;	

	do {
		/* the write operation will trigger the
		 * watch on the main thread side */
		g_io_channel_write  (notification_channel,
				     &op_name,
				     1,
				     &bytes_written);	
	} while (bytes_written < 1);

	CAMEL_LOG_FULL_DEBUG ("Leaving CamelThreadProxy::_notify_availability\n");
}



/* signal proxying */



/**
 * _signal_marshaller_server_side: called in the child thread to proxy a signal    
 * @object: 
 * @data: 
 * @n_args: 
 * @args: 
 * 
 * 
 **/
static void
_signal_marshaller_server_side (GtkObject *object,
				gpointer data,
				guint n_args,
				GtkArg *args)
{
	CamelThreadProxy *proxy;
	guint signal_id;
	
	CAMEL_LOG_FULL_DEBUG ("Entering CamelThreadProxy::_signal_marshaller_server_side\n");
	proxy = CAMEL_THREAD_PROXY (gtk_object_get_data (object, "__proxy__"));
	signal_id = (guint)data;
	g_assert (proxy);

	g_mutex_lock (proxy->signal_data_mutex);
	
	/* we are going to wait for the main client thread 
	 * to have emitted the last signal we asked him
	 * to proxy.
	 */
	while (proxy->signal_data.args)
		g_cond_wait (proxy->signal_data_cond,
			     proxy->signal_data_mutex);

	proxy->signal_data.signal_id = signal_id;
	proxy->signal_data.args = args;

	
	g_mutex_unlock (proxy->signal_data_mutex);

	/* tell the main thread there is a signal pending */
	_notify_availability (proxy, 's');

	CAMEL_LOG_FULL_DEBUG ("Leaving CamelThreadProxy::_signal_marshaller_server_side\n");
}


static void
_signal_marshaller_client_side (CamelThreadProxy *proxy)
{
	g_mutex_lock (proxy->signal_data_mutex);
	g_assert (proxy->signal_data.args);
	
	CAMEL_LOG_FULL_DEBUG ("Entering CamelThreadProxy::_signal_marshaller_client_side\n");
	/* emit the pending signal */
	gtk_signal_emitv (GTK_OBJECT (proxy), 
			  proxy->signal_data.signal_id,
			  proxy->signal_data.args);

	proxy->signal_data.args = NULL;

	/* if waiting for the signal to be treated,
	 * awake the client thread up 
	 */ 
	g_cond_signal (proxy->signal_data_cond);
	g_mutex_unlock (proxy->signal_data_mutex);	
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelThreadProxy::_signal_marshaller_client_side\n");
}


/**
 * camel_thread_proxy_add_signals: init the signal proxy
 * @proxy: proxy 
 * @proxy_object: Proxy Gtk Object 
 * @real_object: Real Gtk Object 
 * @signal_to_proxy: NULL terminated array of signal name 
 * 
 * Add some signals to the list of signals to be 
 * proxied by the proxy object.
 * The signals emitted by the real object in the child
 * thread are reemited by the proxy object in the 
 * main thread.
 **/
void 
camel_thread_proxy_add_signals (CamelThreadProxy *proxy, 
				GtkObject *proxy_object,
				GtkObject *real_object,
				char *signal_to_proxy[])
{
	GtkType camel_folder_type;
	guint i;
 
	CAMEL_LOG_FULL_DEBUG ("Entering CamelThreadProxy::camel_thread_proxy_init_signals\n");

	for (i=0; signal_to_proxy[i]; i++) {
		/* connect the signal to the signal marshaller
		 * user_data is the signal id */
		gtk_signal_connect_full (GTK_OBJECT (real_object),
					 signal_to_proxy[i],
					 NULL,
					 _signal_marshaller_server_side,
					 (gpointer)gtk_signal_lookup (signal_to_proxy[i], 
								      GTK_OBJECT_CLASS (real_object)->type),
					 NULL,
					 TRUE,
					 FALSE);
	}
	
		
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelThreadProxy::camel_thread_proxy_init_signals\n");
	
	
}

/****   catch notification from child thread ****/
/**
 * _thread_notification_catch: call by glib loop when data is available on the thread io channel
 * @source: 
 * @condition: 
 * @data: 
 * 
 * called by watch set on the IO channel
 * 
 * Return value: TRUE because we don't want the watch to be removed
 **/
static gboolean  
_thread_notification_catch (GIOChannel *source,
			    GIOCondition condition,
			    gpointer data)
{
	CamelThreadProxy *proxy = CAMEL_THREAD_PROXY (data);	
	gchar op_name;
	guint bytes_read;
	GIOError error;

	CAMEL_LOG_FULL_DEBUG ("Entering CamelThreadProxy::_thread_notification_catch\n");


	error = g_io_channel_read (source,
				   &op_name,
				   1,
				   &bytes_read);
	
	while ((!error) && (bytes_read == 1)) {
		
		switch (op_name) { 		
		case 'a': /* the thread is OK for a new operation */
			_run_next_op_in_thread (proxy);		
			break;
		case 's': /* there is a pending signal to proxy */
			_signal_marshaller_client_side (proxy);
			break;
		case 'c': /* there is a cb pending in the main thread */
			_run_next_cb (proxy);
			break;
		}
		
		error = g_io_channel_read (source,
					   &op_name,
					   1,
					   &bytes_read);

	}

	CAMEL_LOG_FULL_DEBUG ("Leaving CamelThreadProxy::_thread_notification_catch\n");

	/* do not remove the io watch */
	return TRUE;
}











