/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-folder-pt-proxy.h : proxy folder using posix threads */

/* 
 *
 * Author : 
 *  Bertrand Guiheneuf <bertrand@helixcode.com>
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





#ifndef CAMEL_FOLDER_PT_PROXY_H
#define CAMEL_FOLDER_PT_PROXY_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include "camel-folder.h"
#include "camel-op-queue.h"
#include "camel-thread-proxy.h"


#define CAMEL_FOLDER_PT_PROXY_TYPE     (camel_folder_pt_proxy_get_type ())
#define CAMEL_FOLDER_PT_PROXY(obj)     (GTK_CHECK_CAST((obj), CAMEL_FOLDER_PT_PROXY_TYPE, CamelFolderPtProxy))
#define CAMEL_FOLDER_PT_PROXY_CLASS(k) (GTK_CHECK_CLASS_CAST ((k), CAMEL_FOLDER_PT_PROXY_TYPE, CamelFolderPtProxyClass))
#define CAMEL_IS_FOLDER_PT_PROXY(o)    (GTK_CHECK_TYPE((o), CAMEL_FOLDER_PT_PROXY_TYPE))

typedef struct _CamelFolderPtProxy CamelFolderPtProxy;

typedef struct {
	CamelFolderAsyncCallback real_callback;
	CamelFolderPtProxy *proxy_folder;
	CamelException *ex;
	gpointer real_user_data;
} _ProxyCbUserData;

struct _CamelFolderPtProxy {
	CamelFolder parent;
	
	/* private fields */ 
	CamelFolder *real_folder;
	CamelThreadProxy *proxy_object;
	CamelException *thread_ex;
	_ProxyCbUserData *pud;
	
};



typedef struct {
	CamelFolderClass parent_class;
	
	/* functions and callbacks definition (for marshalling) */
	CamelFuncDef *open_func_def;
	CamelFuncDef *open_cb_def;
	CamelFuncDef *close_func_def;
	CamelFuncDef *close_cb_def;
	CamelFuncDef *set_name_func_def;
	CamelFuncDef *set_name_cb_def;

} CamelFolderPtProxyClass;


GtkType       camel_folder_pt_proxy_get_type         (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_FOLDER_PT_PROXY_H */
