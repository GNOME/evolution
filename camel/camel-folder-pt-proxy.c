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
#include "camel-folder-pt-proxy.h"
#include "camel-log.h"
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

/* needed for proper casts of async funcs when 
 * calling pthreads_create
 */
typedef void * (*thread_call_func) (void *);

static CamelFolderClass *parent_class=NULL;

/* Returns the class for CamelFolderPtProxy and CamelFolder objects */
#define CFPP_CLASS(so) CAMEL_FOLDER_PT_PROXY_CLASS (GTK_OBJECT(so)->klass)
#define CF_CLASS(so) CAMEL_FOLDER_CLASS (GTK_OBJECT(so)->klass)

static void _init_with_store (CamelFolder *folder, CamelStore *parent_store);
static void _open (CamelFolder *folder, CamelFolderOpenMode mode);
static void _close (CamelFolder *folder, gboolean expunge);
static void _set_name (CamelFolder *folder, const gchar *name);
static const gchar *_get_name (CamelFolder *folder);
static const gchar *_get_full_name (CamelFolder *folder);
static gboolean _can_hold_folders (CamelFolder *folder);
static gboolean _can_hold_messages(CamelFolder *folder);
static gboolean _exists (CamelFolder  *folder);
static gboolean _is_open (CamelFolder *folder);
static CamelFolder *_get_folder (CamelFolder *folder, const gchar *folder_name);
static gboolean _create (CamelFolder *folder);
static gboolean _delete (CamelFolder *folder, gboolean recurse);
static gboolean _delete_messages (CamelFolder *folder);
static CamelFolder *_get_parent_folder (CamelFolder *folder);
static CamelStore *_get_parent_store (CamelFolder *folder);
static CamelFolderOpenMode _get_mode (CamelFolder *folder);
static GList *_list_subfolders (CamelFolder *folder);
static void _expunge (CamelFolder *folder);
static CamelMimeMessage *_get_message (CamelFolder *folder, gint number);
static gint _get_message_count (CamelFolder *folder);
static gint _append_message (CamelFolder *folder, CamelMimeMessage *message);
static const GList *_list_permanent_flags (CamelFolder *folder);
static void _copy_message_to (CamelFolder *folder, CamelMimeMessage *message, CamelFolder *dest_folder);

static const gchar *_get_message_uid (CamelFolder *folder, CamelMimeMessage *message);
static CamelMimeMessage *_get_message_by_uid (CamelFolder *folder, const gchar *uid);
static GList *_get_uid_list  (CamelFolder *folder);


static void _finalize (GtkObject *object);


static void
camel_folder_proxy_class_init (CamelFolderPtProxyClass *camel_folder_pt_proxy_class)
{
	GtkObjectClass *gtk_object_class = GTK_OBJECT_CLASS (camel_folder_pt_proxy_class);
	CamelFolderClass *camel_folder_class = CAMEL_FOLDER_CLASS (camel_folder_pt_proxy_class);
	
	parent_class = gtk_type_class (camel_folder_get_type ());
	
	/* virtual method definition */
	camel_folder_class->init_with_store = _init_with_store;
	camel_folder_class->open = _open;
	camel_folder_class->close = _close;
	camel_folder_class->set_name = _set_name;
	camel_folder_class->get_name = _get_name;
	camel_folder_class->can_hold_folders = _can_hold_folders;
	camel_folder_class->can_hold_messages = _can_hold_messages;
	camel_folder_class->exists = _exists;
	camel_folder_class->is_open = _is_open;
	camel_folder_class->get_folder = _get_folder;
	camel_folder_class->create = _create;
	camel_folder_class->delete = _delete;
	camel_folder_class->delete_messages = _delete_messages;
	camel_folder_class->get_parent_folder = _get_parent_folder;
	camel_folder_class->get_parent_store = _get_parent_store;
	camel_folder_class->get_mode = _get_mode;
	camel_folder_class->list_subfolders = _list_subfolders;
	camel_folder_class->expunge = _expunge;
	camel_folder_class->get_message = _get_message;
	camel_folder_class->get_message_count = _get_message_count;
	camel_folder_class->append_message = _append_message;
	camel_folder_class->list_permanent_flags = _list_permanent_flags;
	camel_folder_class->copy_message_to;
	camel_folder_class->get_message_uid = _get_message_uid;
	camel_folder_class->get_message_by_uid = _get_message_by_uid;
	camel_folder_class->get_uid_list = _get_uid_list;

	/* virtual method overload */
	gtk_object_class->finalize = _finalize;
}




static void
camel_folder_proxy_init (CamelFolderPtProxy *folder_pt_proxy)
{
	folder_pt_proxy->op_queue = camel_op_queue_new ();

}



GtkType
camel_folder_proxy_get_type (void)
{
	static GtkType camel_folder_pt_proxy_type = 0;
	
	if (!camel_folder_pt_proxy_type)	{
		GtkTypeInfo camel_folder_pt_proxy_info =	
		{
			"CamelFolderPtProxy",
			sizeof (CamelFolderPtProxy),
			sizeof (CamelFolderPtProxyClass),
			(GtkClassInitFunc) camel_folder_proxy_class_init,
			(GtkObjectInitFunc) camel_folder_proxy_init,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		camel_folder_pt_proxy_type = gtk_type_unique (gtk_object_get_type (), &camel_folder_pt_proxy_info);
	}
	
	return camel_folder_pt_proxy_type;
}


static void           
_finalize (GtkObject *object)
{
	CamelFolder *camel_folder = CAMEL_FOLDER (object);
	GList *message_node;

	CAMEL_LOG_FULL_DEBUG ("Entering CamelFolderPtProxy::finalize\n");

	
	GTK_OBJECT_CLASS (parent_class)->finalize (object);
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelFolderPtProxy::finalize\n");
}


/* generic operation handling */


/**
 * _op_exec_or_plan_for_exec:
 * @proxy_folder: 
 * @op: 
 * 
 * if no thread is currently running, executes
 * op, otherwise push the operation in the operation 
 * queue.
 **/
static void 
_op_exec_or_plan_for_exec (CamelFolderPtProxy *proxy_folder, CamelOp *op)
{
	CamelOpQueue *op_queue;
	pthread_t thread;
	
	op_queue = proxy_folder->op_queue;

	if (camel_op_queue_get_service_availability (op_queue)) {
		camel_op_queue_set_service_availability (op_queue, FALSE);
		pthread_create (&thread, NULL , (thread_call_func)(op->func), op->param);
		camel_op_free (op);
	} else {
		camel_op_queue_push_op (op_queue, op);
	}

}



/**
 * _maybe_run_next_op: run next operation in queue, if any
 * @proxy_folder: 
 * 
 * 
 **/
static void 
_maybe_run_next_op (CamelFolderPtProxy *proxy_folder)
{
	CamelOp *op;
	CamelOpQueue *op_queue;
	pthread_t thread;

	op_queue = proxy_folder->op_queue;
	/* get the next pending operation */
	op = camel_op_queue_pop_op (op_queue);
	if (!op) {
		camel_op_queue_set_service_availability (op_queue, TRUE);
		return;
	}
	
	pthread_create (&thread, NULL , (thread_call_func)(op->func), op->param);
	camel_op_free (op);
}


/**
 * _thread_notification_catch: call by glib loop when data is available on the thread io channel
 * @source: 
 * @condition: 
 * @data: 
 * 
 * called by watch set on the IO channel
 * 
 * Return value: 
 **/
static gboolean  
_thread_notification_catch (GIOChannel *source,
			    GIOCondition condition,
			    gpointer data)
{
	CamelFolderPtProxy *proxy_folder = (CamelFolderPtProxy *)data;	
	gchar op_name;
	guint bytes_read;
	GIOError error;

	error = g_io_channel_read (source,
				   &op_name,
				   1,
				   &bytes_read);
	if (op_name == 'a')
		_maybe_run_next_op (proxy_folder);		

	/* do not remove the io watch */
	return TRUE;
}



/**
 * _init_notify_system: set the notify channel up
 * @proxy_folder: 
 * 
 * called once to set the notification channel
 **/
static void 
_init_notify_system (CamelFolderPtProxy *proxy_folder)
{
	int filedes[2];

	/* set up the notification channel */
	if (!pipe (filedes)) {
		CAMEL_LOG_WARNING ("could not create pipe in for camel_folder_proxy_init");
		CAMEL_LOG_FULL_DEBUG ("Full error message : %s\n", strerror(errno));
		return;
	}
	
	proxy_folder->pipe_client_fd = filedes [0];
	proxy_folder->pipe_server_fd = filedes [1];
	proxy_folder->notify_source =  g_io_channel_unix_new (filedes [0]);
	
	g_io_add_watch (proxy_folder->notify_source, G_IO_IN, _thread_notification_catch, proxy_folder);
	
}

/**
 * notify_availability: notify thread completion
 * @proxy_folder: 
 * 
 * called by child thread before completion 
 **/
static void
notify_availability(CamelFolderPtProxy *proxy_folder)
{
	GIOChannel *notification_channel;
	gchar op_name = 'a';
	guint bytes_written;

	notification_channel = proxy_folder->notify_source;	
	do {
		g_io_channel_write  (notification_channel,
				     &op_name,
				     1,
				     &bytes_written);
	} while (bytes_written == 1);

}



/* folder->init_with_store implementation */

typedef struct {
	CamelFolder *folder;
	CamelStore *parent_store;
} _InitStoreParam;

static void
_async_init_with_store (gpointer param)
{
	_InitStoreParam *init_store_param = (_InitStoreParam *)param;
	CamelFolder *folder = init_store_param->folder;
	CamelFolderPtProxy *proxy_folder;
	CamelFolder *real_folder;

	proxy_folder = CAMEL_FOLDER_PT_PROXY (folder);
	real_folder = proxy_folder->real_folder;

	/* we may block here but we are actually in a 
	 * separate thread, so no problem 
	 */
	/*  g_static_mutex_lock (&(proxy_folder->mutex)); */
	
	CF_CLASS (real_folder)->init_with_store (real_folder, init_store_param->parent_store);
	g_free (param);
	/*  g_static_mutex_unlock (&(proxy_folder->mutex)); */
}


static void 
_init_with_store (CamelFolder *folder, CamelStore *parent_store)
{
	CamelFolderPtProxy *proxy_folder = CAMEL_FOLDER_PT_PROXY (folder);
	_InitStoreParam *param;
	CamelOp *op;

#warning Notify io_channel initialization should be elsewhere
	/* it can not be in camel_folder_proxy_init 
	 * because of the pipe error handling */ 
	_init_notify_system (proxy_folder);

	op = camel_op_new ();
	/* param will be freed in _async_init_with_store */
	param = g_new (_InitStoreParam, 1);
	param->folder = folder;
	param->parent_store = parent_store;
	
	op->func = _async_init_with_store;
	op->param =  param;
	
	_op_exec_or_plan_for_exec (proxy_folder, op);
	
}




/* folder->open implementation */
typedef struct {
	CamelFolder *folder;
	CamelFolderOpenMode mode;
} _openFolderParam;

static void
_open (CamelFolder *folder, CamelFolderOpenMode mode)
{

}




static void
_close (CamelFolder *folder, gboolean expunge)
{
	if (expunge) camel_folder_expunge (folder, FALSE);
	folder->open_state = FOLDER_CLOSE;
}




static void
_set_name (CamelFolder *folder, const gchar *name)
{

}



static const gchar *
_get_name (CamelFolder *folder)
{
	return folder->name;
}




static const gchar *
_get_full_name (CamelFolder *folder)
{
	return folder->full_name;
}




static gboolean
_can_hold_folders (CamelFolder *folder)
{
	return folder->can_hold_folders;
}




static gboolean
_can_hold_messages (CamelFolder *folder)
{
	return folder->can_hold_messages;
}



static gboolean
_exists (CamelFolder *folder)
{
	return FALSE;
}




static gboolean
_is_open (CamelFolder *folder)
{
	return (folder->open_state == FOLDER_OPEN);
} 





static CamelFolder *
_get_folder (CamelFolder *folder, const gchar *folder_name)
{

	return NULL;
}






static gboolean
_create(CamelFolder *folder)
{
	
	return FALSE;
}








static gboolean
_delete (CamelFolder *folder, gboolean recurse)
{
	return FALSE;
}







static gboolean 
_delete_messages (CamelFolder *folder)
{
	return TRUE;
}






static CamelFolder *
_get_parent_folder (CamelFolder *folder)
{
	return folder->parent_folder;
}





static CamelStore *
_get_parent_store (CamelFolder *folder)
{
	return folder->parent_store;
}




static CamelFolderOpenMode
_get_mode (CamelFolder *folder)
{
	return folder->open_mode;
}




static GList *
_list_subfolders (CamelFolder *folder)
{
	return NULL;
}




static void
_expunge (CamelFolder *folder)
{

}




static CamelMimeMessage *
_get_message (CamelFolder *folder, gint number)
{
	return NULL;
}





static gint
_get_message_count (CamelFolder *folder)
{
	return -1;
}




static gint
_append_message (CamelFolder *folder, CamelMimeMessage *message)
{
	return -1;
}



static const GList *
_list_permanent_flags (CamelFolder *folder)
{
	return folder->permanent_flags;
}



static void
_copy_message_to (CamelFolder *folder, CamelMimeMessage *message, CamelFolder *dest_folder)
{
	camel_folder_append_message (dest_folder, message);
}






/* UIDs stuff */


static const gchar *
_get_message_uid (CamelFolder *folder, CamelMimeMessage *message)
{
	return NULL;
}


/* the next two func are left there temporarily */
static const gchar *
_get_message_uid_by_number (CamelFolder *folder, gint message_number)
{
	return NULL;
}



static CamelMimeMessage *
_get_message_by_uid (CamelFolder *folder, const gchar *uid)
{
	return NULL;
}


static GList *
_get_uid_list  (CamelFolder *folder)
{
	return NULL;
}


/* **** */
