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

/* FIXME :
 * 
 * the current implementation lauches requests 
 * on the real folder without bothering on wether 
 * the global mutex (folder->mutex) is locked or
 * not. 
 * This means that you could have 100 threads 
 * waiting for the mutex at the same time. 
 * This is not really a bug, more a nasty feature ;)
 *
 * This will be solved when the CORBA proxy is 
 * written, as we will need to queue the requests 
 * on the client side. The same queue mechanism 
 * will be used for the pthread proxy 
 *
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



static void 
_plan_op_for_exec (CamelOp *op)
{
		

}


/* folder->init_with_store implementation */

typedef struct {
	CamelFolder *folder;
	CamelStore *parent_store;
} _InitStoreParam;

static void
_async_init_with_store (_InitStoreParam *param)
{
	CamelFolder *folder = param->folder;
	CamelFolderPtProxy *proxy_folder;
	CamelFolder *real_folder;

	proxy_folder = CAMEL_FOLDER_PT_PROXY (folder);
	real_folder = proxy_folder->real_folder;

	/* we may block here but we are actually in a 
	 * separate thread, so no problem 
	 */
	g_static_mutex_lock (&(proxy_folder->mutex));
	
	CF_CLASS (real_folder)->init_with_store (real_folder, param->parent_store);
	g_free (param);
	g_static_mutex_unlock (&(proxy_folder->mutex));
}


static void 
_init_with_store (CamelFolder *folder, CamelStore *parent_store)
{
	CamelFolderPtProxy *proxy_folder = CAMEL_FOLDER_PT_PROXY (folder);
	_InitStoreParam *param;
	pthread_t init_store_thread;
	int filedes[2];

#warning Notify io_channel initialization should be elsewhere
	/* it can not be in camel_folder_proxy_init 
	 * because of the pipe error handling */ 
	
	/* set up the notification channel */
	if (!pipe (filedes)) {
		CAMEL_LOG_WARNING ("could not create pipe in for camel_folder_proxy_init");
		CAMEL_LOG_FULL_DEBUG ("Full error message : %s\n", strerror(errno));
		return;
	}

	proxy_folder->pipe_client_fd = filedes [0];
	proxy_folder->pipe_server_fd = filedes [1];
	proxy_folder->notify_source =  g_io_channel_unix_new (filedes [0]);

	/* param will be freed in _async_init_with_store */
	param = g_new (_InitStoreParam, 1);
	param->folder = folder;
	param->parent_store = parent_store;
	
	/* 
	 * call _async_init_with_store in a separate thread 
	 * the thread may block on a mutex, but not the main
	 * thread.
	 */
	pthread_create (&init_store_thread, NULL , (thread_call_func)_async_init_with_store, param);
	
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
