/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-folder-pt-proxy.c : proxy folder using posix threads */

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



#include <config.h>
#include "camel-folder-pt-proxy.h"
#include "camel-log.h"
#include "camel-marshal-utils.h"
#include "camel-exception.h"
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>


static CamelFolderClass *parent_class=NULL;

/* Returns the class for CamelFolderPtProxy and CamelFolder objects */
#define CFPP_CLASS(so) CAMEL_FOLDER_PT_PROXY_CLASS (GTK_OBJECT(so)->klass)
#define CF_CLASS(so) CAMEL_FOLDER_CLASS (GTK_OBJECT(so)->klass)


enum CamelFolderFunc {
	CAMEL_FOLDER_OPEN,
	CAMEL_FOLDER_CLOSE,
	CAMEL_FOLDER__LAST_FUNC
};

static CamelFuncDef _camel_func_def [CAMEL_FOLDER__LAST_FUNC];


static void _init_with_store (CamelFolder *folder, 
			      CamelStore *parent_store, 
			      CamelException *ex);
static void _open_async (CamelFolder *folder, 
			 CamelFolderOpenMode mode, 
			 CamelFolderAsyncCallback callback, 
			 gpointer user_data, 
			 CamelException *ex);
static void _close_async (CamelFolder *folder, 
		    gboolean expunge, 
		    CamelFolderAsyncCallback callback, 
		    gpointer user_data, 
		    CamelException *ex);
static void _open (CamelFolder *folder, 
		   CamelFolderOpenMode mode, 
		   CamelException *ex);
static void _close (CamelFolder *folder, 
		    gboolean expunge, 
		    CamelException *ex);
static void _set_name (CamelFolder *folder, 
		       const gchar *name, 
		       CamelException *ex);

static const gchar *_get_name (CamelFolder *folder, CamelException *ex);
static const gchar *_get_full_name (CamelFolder *folder, CamelException *ex);
static gboolean _can_hold_folders (CamelFolder *folder, CamelException *ex);
static gboolean _can_hold_messages(CamelFolder *folder, CamelException *ex);
static gboolean _exists (CamelFolder  *folder, CamelException *ex);
static gboolean _is_open (CamelFolder *folder, CamelException *ex);
static CamelFolder *_get_subfolder (CamelFolder *folder, const gchar *folder_name, CamelException *ex);
static gboolean _create (CamelFolder *folder, CamelException *ex);
static gboolean _delete (CamelFolder *folder, gboolean recurse, CamelException *ex);
static gboolean _delete_messages (CamelFolder *folder, CamelException *ex);
static CamelFolder *_get_parent_folder (CamelFolder *folder, CamelException *ex);
static CamelStore *_get_parent_store (CamelFolder *folder, CamelException *ex);
static CamelFolderOpenMode _get_mode (CamelFolder *folder, CamelException *ex);
static GList *_list_subfolders (CamelFolder *folder, CamelException *ex);
static void _expunge (CamelFolder *folder, CamelException *ex);
static CamelMimeMessage *_get_message_by_number (CamelFolder *folder, gint number, CamelException *ex);
static gint _get_message_count (CamelFolder *folder, CamelException *ex);
static gint _append_message (CamelFolder *folder, CamelMimeMessage *message, CamelException *ex);
static const GList *_list_permanent_flags (CamelFolder *folder, CamelException *ex);
static void _copy_message_to (CamelFolder *folder, CamelMimeMessage *message, CamelFolder *dest_folder, CamelException *ex);

static const gchar *_get_message_uid (CamelFolder *folder, CamelMimeMessage *message, CamelException *ex);
static CamelMimeMessage *_get_message_by_uid (CamelFolder *folder, const gchar *uid, CamelException *ex);
static GList *_get_uid_list  (CamelFolder *folder, CamelException *ex);

static void _finalize (GtkObject *object);


static void
camel_folder_pt_proxy_class_init (CamelFolderPtProxyClass *camel_folder_pt_proxy_class)
{
	GtkObjectClass *gtk_object_class = GTK_OBJECT_CLASS (camel_folder_pt_proxy_class);
	CamelFolderClass *camel_folder_class = CAMEL_FOLDER_CLASS (camel_folder_pt_proxy_class);
	CamelFolderPtProxyClass *proxy_class = camel_folder_pt_proxy_class;

	parent_class = gtk_type_class (camel_folder_get_type ());
	
	/* virtual method definition */
	camel_folder_class->init_with_store = _init_with_store;
	camel_folder_class->open = _open;
	camel_folder_class->close = _close;
	camel_folder_class->open_async = _open_async;
	camel_folder_class->close_async = _close_async;
	camel_folder_class->set_name = _set_name;
	camel_folder_class->get_name = _get_name;
	camel_folder_class->can_hold_folders = _can_hold_folders;
 	camel_folder_class->can_hold_messages = _can_hold_messages;
	camel_folder_class->exists = _exists;
	camel_folder_class->is_open = _is_open;
	camel_folder_class->get_subfolder = _get_subfolder;
	camel_folder_class->create = _create;
	camel_folder_class->delete = _delete;
	camel_folder_class->delete_messages = _delete_messages;
	camel_folder_class->get_parent_folder = _get_parent_folder;
	camel_folder_class->get_parent_store = _get_parent_store;
	camel_folder_class->get_mode = _get_mode;
	camel_folder_class->list_subfolders = _list_subfolders;
	camel_folder_class->expunge = _expunge;
	camel_folder_class->get_message_by_number = _get_message_by_number;
	camel_folder_class->get_message_count = _get_message_count;
	camel_folder_class->append_message = _append_message;
	camel_folder_class->list_permanent_flags = _list_permanent_flags;
	camel_folder_class->copy_message_to;
	camel_folder_class->get_message_uid = _get_message_uid;
	camel_folder_class->get_message_by_uid = _get_message_by_uid;
	camel_folder_class->get_uid_list = _get_uid_list;

	/* virtual method overload */
	gtk_object_class->finalize = _finalize;

	/* function definition for proxying */
	proxy_class->open_func_def = 
		camel_func_def_new (camel_marshal_NONE__POINTER_INT_POINTER_POINTER, 
				    4, 
				    GTK_TYPE_POINTER,
				    GTK_TYPE_INT,
				    GTK_TYPE_POINTER,
				    GTK_TYPE_POINTER);
	proxy_class->open_cb_def = 
		camel_func_def_new (camel_marshal_NONE__POINTER_POINTER_POINTER, 
				    3, 
				    GTK_TYPE_POINTER,
				    GTK_TYPE_POINTER,
				    GTK_TYPE_POINTER);

	proxy_class->close_func_def = 
		camel_func_def_new (camel_marshal_NONE__POINTER_BOOL_POINTER_POINTER, 
				    4, 
				    GTK_TYPE_POINTER,
				    GTK_TYPE_BOOL,
				    GTK_TYPE_POINTER,
				    GTK_TYPE_POINTER);
	proxy_class->close_cb_def = 
		camel_func_def_new (camel_marshal_NONE__POINTER_POINTER_POINTER, 
				    3, 
				    GTK_TYPE_POINTER,
				    GTK_TYPE_POINTER,
				    GTK_TYPE_POINTER);

	proxy_class->set_name_func_def = 
		camel_func_def_new (camel_marshal_NONE__POINTER_BOOL_POINTER_POINTER, 
				    4, 
				    GTK_TYPE_POINTER,
				    GTK_TYPE_BOOL,
				    GTK_TYPE_POINTER,
				    GTK_TYPE_POINTER);
	proxy_class->set_name_cb_def = 
		camel_func_def_new (camel_marshal_NONE__POINTER_POINTER_POINTER, 
				    3, 
				    GTK_TYPE_POINTER,
				    GTK_TYPE_POINTER,
				    GTK_TYPE_POINTER);

}




static void
camel_folder_pt_proxy_init (CamelFolderPtProxy *folder_pt_proxy)
{
	folder_pt_proxy->thread_ex = camel_exception_new ();	
	folder_pt_proxy->pud = g_new (_ProxyCbUserData, 1);	
}




GtkType
camel_folder_pt_proxy_get_type (void)
{
	static GtkType camel_folder_pt_proxy_type = 0;
	
	if (!camel_folder_pt_proxy_type)	{
		GtkTypeInfo camel_folder_pt_proxy_info =	
		{
			"CamelFolderPtProxy",
			sizeof (CamelFolderPtProxy),
			sizeof (CamelFolderPtProxyClass),
			(GtkClassInitFunc) camel_folder_pt_proxy_class_init,
			(GtkObjectInitFunc) camel_folder_pt_proxy_init,
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
	CamelFolderPtProxy *camel_folder_pt_proxy = CAMEL_FOLDER_PT_PROXY (camel_folder);
	GList *message_node;

	CAMEL_LOG_FULL_DEBUG ("Entering CamelFolderPtProxy::finalize\n");

	camel_exception_free (camel_folder_pt_proxy->thread_ex);
	g_free (camel_folder_pt_proxy->pud);
	GTK_OBJECT_CLASS (parent_class)->finalize (object);
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelFolderPtProxy::finalize\n");
}






/*********/

/**** Operations implementation ****/



static gpointer
_proxy_cb_user_data (_ProxyCbUserData *pud,
		     CamelFolderAsyncCallback real_callback, 
		     CamelFolderPtProxy *proxy_folder, 
		     CamelException *ex,
		     gpointer real_user_data)
{
	pud->real_callback = real_callback;
	pud->proxy_folder = proxy_folder;
	pud->ex = ex;
	pud->real_user_data = real_user_data;
	return (gpointer)pud;
}


/* ******** */

/* thread->init_with_store implementation */
static void 
_init_with_store (CamelFolder *folder, 
		  CamelStore *parent_store, 
		  CamelException *ex)
{

	parent_class->init_with_store (folder, parent_store, ex);
	if (ex->id != CAMEL_EXCEPTION_NONE)
		return;
#warning use proxy store here  
	CF_CLASS (folder)->init_with_store (CAMEL_FOLDER_PT_PROXY (folder)->real_folder, 
					    parent_store, 
					    ex);
}



/* a little bit of explanation for the folder_class->open 
 * method implementation : 
 * 
 * the proxy object "open" method is called by the client 
 * program in the main thread. This method creates a 
 * CamelOp object containing all the necessary informations 
 * to call the corresponding "open" method on the real 
 * folder object in the child thread. This CamelOp object 
 * is thus pushed in a queue in the main thread (see the 
 * CamelThreadProxy structure for more details). 
 * The operations in this queue are executed one by one
 * in a child thread. 
 * Once the "open" method of the real object is finished, 
 * it calls a callback. This callback is not the one supplied
 * by the client object. Instead, the _folder_open_cb()
 * function is called (in the child thread) which pushes
 * the real callback function in another operation queue.
 * The real callback is then called in the main thread. 
 */

/* folder->open implementation */

/* 
 * proxy callback. Called in the child thread by the 
 * real folder "open" method when it is completed 
 */
static void 
_folder_open_cb (CamelFolder *folder,
		 gpointer user_data,
		 CamelException *ex)
{
	CamelOp *cb;
	_ProxyCbUserData *pud;
	CamelFuncDef *cb_def;

	/* transfer the exception information from "ex" to the 
	 * client supplied exception (kept in pud->ex) */ 	 
	camel_exception_xfer (pud->ex, ex);

	/* create an operation which will call the real client
	 * supplied callback in the main thread */
	cb_def = CAMEL_FOLDER_PT_PROXY_CLASS(pud->proxy_folder)->open_cb_def;
	cb = camel_marshal_create_op (cb_def,
				      pud->real_callback, 
				      pud->proxy_folder, 
				      pud->real_user_data,
				      pud->ex);
	camel_thread_proxy_push_cb (pud->proxy_folder->proxy_object, cb);

}

static void
_open_async (CamelFolder *folder, 
	     CamelFolderOpenMode mode, 
	     CamelFolderAsyncCallback callback, 
	     gpointer user_data, 
	     CamelException *ex)
{
	CamelFolderPtProxy *proxy_folder;
	CamelOp *op;
	CamelFuncDef *func_def;

	proxy_folder = CAMEL_FOLDER_PT_PROXY (folder);
	
	/* create an operation corresponding to the "open"
	 * method of the real object. The operation definition
	 * is common to all instances of the CamelFolderPtProxy 
	 * class so it is contained in the CamelFolderPtProxyClass
	 * structure. */
	func_def = CAMEL_FOLDER_PT_PROXY_CLASS(proxy_folder)->open_func_def;
	if (callback)		
		op = camel_marshal_create_op (func_def, 
					      CAMEL_FOLDER_CLASS (proxy_folder->real_folder)->open_async,
					      proxy_folder->real_folder,
					      mode,
					      _folder_open_cb,
					      _proxy_cb_user_data (proxy_folder->pud, callback, proxy_folder, ex, user_data),
					      proxy_folder->thread_ex);
	else 
		op = camel_marshal_create_op (func_def, 
					      CAMEL_FOLDER_CLASS (proxy_folder->real_folder)->open_async,
					      proxy_folder->real_folder,
					      mode,
					      NULL,
					      NULL,
					      NULL);
	/* push the operation in the operation queue. This operation 
	 * will be executed in a child thread but only one operation 
	 * will be executed at a time, so that folder implementations
	 * don't have to be MultiThread safe. */
	camel_thread_proxy_push_op (proxy_folder->proxy_object, op);				      	
}



static void _open (CamelFolder *folder, 
		   CamelFolderOpenMode mode, 
		   CamelException *ex)
{
	CamelFolderPtProxy *proxy_folder;

	proxy_folder = CAMEL_FOLDER_PT_PROXY (folder);
	CF_CLASS (proxy_folder->real_folder)->
		open (proxy_folder->real_folder, mode, ex);
}



/* folder->close implementation */

static void 
_folder_close_cb (CamelFolder *folder,
		  gpointer user_data,
		  CamelException *ex)
{
	CamelOp *cb;
	_ProxyCbUserData *pud;
	CamelFuncDef *cb_def;

	camel_exception_xfer (pud->ex, ex);
	cb_def = CAMEL_FOLDER_PT_PROXY_CLASS(pud->proxy_folder)->close_cb_def;
	cb = camel_marshal_create_op (cb_def,
				      pud->real_callback, 
				      pud->proxy_folder, 
				      pud->real_user_data,
				      pud->ex);
	camel_thread_proxy_push_cb (pud->proxy_folder->proxy_object, cb);

}

static void
_close_async (CamelFolder *folder, 
	      gboolean expunge, 
	      CamelFolderAsyncCallback callback, 
	      gpointer user_data, 
	      CamelException *ex)
{
	CamelFolderPtProxy *proxy_folder;
	CamelOp *op;
	CamelFuncDef *func_def;

	proxy_folder = CAMEL_FOLDER_PT_PROXY (folder);
	
	func_def = CAMEL_FOLDER_PT_PROXY_CLASS(proxy_folder)->close_func_def;
	if (callback)		
		op = camel_marshal_create_op (func_def, 
					      CAMEL_FOLDER_CLASS (proxy_folder->real_folder)->close_async,
					      proxy_folder->real_folder,
					      expunge,
					      _folder_close_cb,
					      _proxy_cb_user_data (proxy_folder->pud, callback, proxy_folder, ex, user_data),
					      proxy_folder->thread_ex);
	else 
		op = camel_marshal_create_op (func_def, 
					      CAMEL_FOLDER_CLASS (proxy_folder->real_folder)->close_async,
					      proxy_folder->real_folder,
					      expunge,
					      NULL,
					      NULL,
					      NULL);
	camel_thread_proxy_push_op (proxy_folder->proxy_object, op);

}


static void _close (CamelFolder *folder, 
		    gboolean expunge, 
		    CamelException *ex)
{
	CamelFolderPtProxy *proxy_folder;

	proxy_folder = CAMEL_FOLDER_PT_PROXY (folder);
	CF_CLASS (proxy_folder->real_folder)->
		close (proxy_folder->real_folder, expunge, ex);
}





/* folder->set_name implementation */

static void
_set_name (CamelFolder *folder, 
	   const gchar *name, 
	   CamelException *ex)
{
	CamelFolderPtProxy *proxy_folder;

	proxy_folder = CAMEL_FOLDER_PT_PROXY (folder);
	CF_CLASS (proxy_folder->real_folder)->
		set_name (proxy_folder->real_folder, name, ex);
	
}


/* folder->get_name implementation */
/* this one is not executed in a thread */
static const gchar *
_get_name (CamelFolder *folder, CamelException *ex)
{
	CamelFolderPtProxy *proxy_folder;

	proxy_folder = CAMEL_FOLDER_PT_PROXY (folder);
	return CF_CLASS (proxy_folder->real_folder)->
		get_name (proxy_folder->real_folder, ex);
}



/* folder->get_full_name implementation */
/* this one is not executed in a thread */

static const gchar *
_get_full_name (CamelFolder *folder, CamelException *ex)
{
	CamelFolderPtProxy *proxy_folder;

	proxy_folder = CAMEL_FOLDER_PT_PROXY (folder);
	return CF_CLASS (proxy_folder->real_folder)->
		get_full_name (proxy_folder->real_folder, ex);
}




static gboolean
_can_hold_folders (CamelFolder *folder, CamelException *ex)
{
	CamelFolderPtProxy *proxy_folder;

	proxy_folder = CAMEL_FOLDER_PT_PROXY (folder);
	return CF_CLASS (proxy_folder->real_folder)->
		can_hold_folders (proxy_folder->real_folder, ex);
}




static gboolean
_can_hold_messages (CamelFolder *folder, CamelException *ex)
{
	CamelFolderPtProxy *proxy_folder;

	proxy_folder = CAMEL_FOLDER_PT_PROXY (folder);
	return CF_CLASS (proxy_folder->real_folder)->
		can_hold_messages (proxy_folder->real_folder, ex);
}



static gboolean
_exists (CamelFolder *folder, CamelException *ex)
{
	CamelFolderPtProxy *proxy_folder;

	proxy_folder = CAMEL_FOLDER_PT_PROXY (folder);
	return CF_CLASS (proxy_folder->real_folder)->
		exists (proxy_folder->real_folder, ex);
}




static gboolean
_is_open (CamelFolder *folder, CamelException *ex)
{
	CamelFolderPtProxy *proxy_folder;

	proxy_folder = CAMEL_FOLDER_PT_PROXY (folder);
	return CF_CLASS (proxy_folder->real_folder)->
		is_open (proxy_folder->real_folder, ex);
} 





static CamelFolder *
_get_subfolder (CamelFolder *folder, const gchar *folder_name, CamelException *ex)
{
	CamelFolderPtProxy *proxy_folder;

	proxy_folder = CAMEL_FOLDER_PT_PROXY (folder);
	return CF_CLASS (proxy_folder->real_folder)->
		get_subfolder (proxy_folder->real_folder, folder_name, ex);
}






static gboolean
_create(CamelFolder *folder, CamelException *ex)
{
	CamelFolderPtProxy *proxy_folder;

	proxy_folder = CAMEL_FOLDER_PT_PROXY (folder);
	return CF_CLASS (proxy_folder->real_folder)->
		create (proxy_folder->real_folder, ex);
}








static gboolean
_delete (CamelFolder *folder, gboolean recurse, CamelException *ex)
{
	CamelFolderPtProxy *proxy_folder;

	proxy_folder = CAMEL_FOLDER_PT_PROXY (folder);
	return CF_CLASS (proxy_folder->real_folder)->
		delete (proxy_folder->real_folder, recurse, ex);
}







static gboolean 
_delete_messages (CamelFolder *folder, CamelException *ex)
{
	CamelFolderPtProxy *proxy_folder;

	proxy_folder = CAMEL_FOLDER_PT_PROXY (folder);
	return CF_CLASS (proxy_folder->real_folder)->
		delete_messages (proxy_folder->real_folder, ex);
}






static CamelFolder *
_get_parent_folder (CamelFolder *folder, CamelException *ex)
{
	CamelFolderPtProxy *proxy_folder;
#warning return proxy parent folder if any
	proxy_folder = CAMEL_FOLDER_PT_PROXY (folder);
	return CF_CLASS (proxy_folder->real_folder)->
		get_parent_folder (proxy_folder->real_folder, ex);
}





static CamelStore *
_get_parent_store (CamelFolder *folder, CamelException *ex)
{
	CamelFolderPtProxy *proxy_folder;

	proxy_folder = CAMEL_FOLDER_PT_PROXY (folder);
	return CF_CLASS (proxy_folder->real_folder)->
		get_parent_store (proxy_folder->real_folder, ex);
}




static CamelFolderOpenMode
_get_mode (CamelFolder *folder, CamelException *ex)
{
	CamelFolderPtProxy *proxy_folder;

	proxy_folder = CAMEL_FOLDER_PT_PROXY (folder);
	return CF_CLASS (proxy_folder->real_folder)->
		get_mode (proxy_folder->real_folder, ex);
}




static GList *
_list_subfolders (CamelFolder *folder, CamelException *ex)
{
	CamelFolderPtProxy *proxy_folder;

	proxy_folder = CAMEL_FOLDER_PT_PROXY (folder);
	return CF_CLASS (proxy_folder->real_folder)->
		list_subfolders (proxy_folder->real_folder, ex);
}




static void
_expunge (CamelFolder *folder, CamelException *ex)
{
	CamelFolderPtProxy *proxy_folder;

	proxy_folder = CAMEL_FOLDER_PT_PROXY (folder);
	CF_CLASS (proxy_folder->real_folder)->
		expunge (proxy_folder->real_folder, ex);
}




static CamelMimeMessage *
_get_message_by_number (CamelFolder *folder, gint number, CamelException *ex)
{
	CamelFolderPtProxy *proxy_folder;

	proxy_folder = CAMEL_FOLDER_PT_PROXY (folder);
	return CF_CLASS (proxy_folder->real_folder)->
		get_message_by_number (proxy_folder->real_folder, number, ex);
}





static gint
_get_message_count (CamelFolder *folder, CamelException *ex)
{
	CamelFolderPtProxy *proxy_folder;

	proxy_folder = CAMEL_FOLDER_PT_PROXY (folder);
	return CF_CLASS (proxy_folder->real_folder)->
		get_message_count (proxy_folder->real_folder, ex);
}




static gint
_append_message (CamelFolder *folder, CamelMimeMessage *message, CamelException *ex)
{
	CamelFolderPtProxy *proxy_folder;

	proxy_folder = CAMEL_FOLDER_PT_PROXY (folder);
	return CF_CLASS (proxy_folder->real_folder)->
		append_message (proxy_folder->real_folder, message, ex);
}



static const GList *
_list_permanent_flags (CamelFolder *folder, CamelException *ex)
{
	CamelFolderPtProxy *proxy_folder;

	proxy_folder = CAMEL_FOLDER_PT_PROXY (folder);
	return CF_CLASS (proxy_folder->real_folder)->
		list_permanent_flags (proxy_folder->real_folder, ex);
}



static void
_copy_message_to (CamelFolder *folder, CamelMimeMessage *message, CamelFolder *dest_folder, CamelException *ex)
{
	CamelFolderPtProxy *proxy_folder;

	proxy_folder = CAMEL_FOLDER_PT_PROXY (folder);
	CF_CLASS (proxy_folder->real_folder)->
		copy_message_to (proxy_folder->real_folder, message, dest_folder, ex);
}






/* UIDs stuff */


static const gchar *
_get_message_uid (CamelFolder *folder, CamelMimeMessage *message, CamelException *ex)
{
	CamelFolderPtProxy *proxy_folder;

	proxy_folder = CAMEL_FOLDER_PT_PROXY (folder);
	return CF_CLASS (proxy_folder->real_folder)->
		get_message_uid (proxy_folder->real_folder, message, ex);
}


/* the next two func are left there temporarily */
#if 0
static const gchar *
_get_message_uid_by_number (CamelFolder *folder, gint message_number, CamelException *ex)
{
	CamelFolderPtProxy *proxy_folder;

	proxy_folder = CAMEL_FOLDER_PT_PROXY (folder);
	return CF_CLASS (proxy_folder->real_folder)->
		get_message_uid_by_number (proxy_folder->real_folder, message_number, ex);
}

#endif

static CamelMimeMessage *
_get_message_by_uid (CamelFolder *folder, const gchar *uid, CamelException *ex)
{
	CamelFolderPtProxy *proxy_folder;

	proxy_folder = CAMEL_FOLDER_PT_PROXY (folder);
	return CF_CLASS (proxy_folder->real_folder)->
		get_message_by_uid (proxy_folder->real_folder, uid, ex);
}


static GList *
_get_uid_list  (CamelFolder *folder, CamelException *ex)
{
	CamelFolderPtProxy *proxy_folder;

	proxy_folder = CAMEL_FOLDER_PT_PROXY (folder);
	return CF_CLASS (proxy_folder->real_folder)->
		get_uid_list (proxy_folder->real_folder, ex);
}


/* **** */





