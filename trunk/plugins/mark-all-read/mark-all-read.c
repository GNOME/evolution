/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Authors: Chenthill Palanisamy (pchenthill@novell.com)
 *
 *  Copyright 2004 Novell, Inc. (www.novell.com)
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU General Public
 *  License as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <string.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <libgnome/gnome-i18n.h>
#include <e-util/e-config.h>
#include <mail/em-popup.h>
#include <mail/mail-ops.h>
#include <mail/mail-mt.h>
#include <camel/camel-vee-folder.h>
#include "e-util/e-error.h"

void org_gnome_mark_all_read (EPlugin *ep, EMPopupTargetFolder *target);
static void mar_got_folder (char *uri, CamelFolder *folder, void *data); 
static void mar_all_sub_folders (CamelStore *store, CamelFolderInfo *fi, CamelException *ex);

void 
org_gnome_mark_all_read (EPlugin *ep, EMPopupTargetFolder *t)
{
	if (t->uri == NULL) {
		return;
	}
	
	mail_get_folder(t->uri, 0, mar_got_folder, NULL, mail_thread_new);
}

static void
mark_all_as_read (CamelFolder *folder)
{
	GPtrArray *uids;
	int i;
	
	uids =  camel_folder_get_uids (folder); 
	camel_folder_freeze(folder);
	for (i=0;i<uids->len;i++)
		camel_folder_set_message_flags(folder, uids->pdata[i], CAMEL_MESSAGE_SEEN, CAMEL_MESSAGE_SEEN);
	camel_folder_thaw(folder);
	camel_folder_free_uids (folder, uids); 
}

static void 
mar_got_folder (char *uri, CamelFolder *folder, void *data) 
{
	CamelFolderInfo *info;
	CamelStore *store;
	CamelException ex;
	gint response;
	guint32 flags = CAMEL_STORE_FOLDER_INFO_RECURSIVE | CAMEL_STORE_FOLDER_INFO_FAST;

	camel_exception_init (&ex);
	store = folder->parent_store;
	info = camel_store_get_folder_info (store, folder->full_name, flags, &ex); 

	/* FIXME we have to disable the menu item */
	if (!folder)
		return;

	if (camel_exception_is_set (&ex)) { 
		camel_exception_clear (&ex);
		return;
	}
	
	if (info && (info->child || info->next)) {
		response = e_error_run (NULL, "mail:ask-mark-read", NULL);
	} else {
		mark_all_as_read (folder);
		return;
	}
				
	if (response == GTK_RESPONSE_NO) {
		mark_all_as_read (folder);
	} else if (response == GTK_RESPONSE_YES){
		mar_all_sub_folders (store, info, &ex); 
	}
	 
}

static void
mar_all_sub_folders (CamelStore *store, CamelFolderInfo *fi, CamelException *ex)
{
	while (fi) {
		CamelFolder *folder;
		
		if (fi->child) {
			mar_all_sub_folders (store, fi->child, ex);
			if (camel_exception_is_set (ex))
				return;
		}
		
		if (!(folder = camel_store_get_folder (store, fi->full_name, 0, ex)))
			return;
		
		if (!CAMEL_IS_VEE_FOLDER (folder)) {
			mark_all_as_read (folder);	
		}
		
		fi = fi->next;
	}
}
