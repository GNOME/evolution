/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#include <config.h>

#include "camel-exception.h"
#include "camel-vtrash-folder.h"
#include "camel-store.h"
#include "camel-vee-store.h"
#include "camel-mime-message.h"

#include <string.h>

static CamelVeeFolderClass *camel_vtrash_folder_parent;

static void vtrash_append_message (CamelFolder *folder, CamelMimeMessage *message,
				   const CamelMessageInfo *info, CamelException *ex);
static void vtrash_copy_messages_to (CamelFolder *folder, GPtrArray *uids, CamelFolder *dest, CamelException *ex);
static void vtrash_move_messages_to (CamelFolder *folder, GPtrArray *uids, CamelFolder *dest, CamelException *ex);

static void
camel_vtrash_folder_class_init (CamelVTrashFolderClass *klass)
{
	CamelFolderClass *folder_class = (CamelFolderClass *) klass;
	
	camel_vtrash_folder_parent =
		CAMEL_VEE_FOLDER_CLASS (camel_type_get_global_classfuncs (camel_folder_get_type ()));
	
	folder_class->append_message = vtrash_append_message;
	folder_class->copy_messages_to = vtrash_copy_messages_to;
	folder_class->move_messages_to = vtrash_move_messages_to;
}

CamelType
camel_vtrash_folder_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (camel_vee_folder_get_type (),
					    "CamelVTrashFolder",
					    sizeof (CamelVTrashFolder),
					    sizeof (CamelVTrashFolderClass),
					    (CamelObjectClassInitFunc) camel_vtrash_folder_class_init,
					    NULL,
					    NULL,
					    NULL);
	}
	
	return type;
}

/**
 * camel_vee_folder_new:
 * @parent_store: the parent CamelVeeStore
 * @name: the vfolder name
 * @ex: a CamelException
 *
 * Create a new CamelVeeFolder object.
 *
 * Return value: A new CamelVeeFolder widget.
 **/
CamelFolder *
camel_vtrash_folder_new (CamelStore *parent_store, const char *name)
{
	CamelFolder *vtrash;
	char *vtrash_name;
	guint32 flags;
	
	vtrash = (CamelFolder *)camel_object_new (camel_vtrash_folder_get_type ());
	vtrash_name = g_strdup_printf ("%s?(match-all (system-flag \"Deleted\"))", name);
	flags = CAMEL_STORE_FOLDER_PRIVATE | CAMEL_STORE_FOLDER_CREATE | CAMEL_STORE_VEE_FOLDER_AUTO;
	
	camel_vee_folder_construct (CAMEL_VEE_FOLDER (vtrash), parent_store, vtrash_name, flags);
	
	return vtrash;
}

static void
vtrash_append_message (CamelFolder *folder, CamelMimeMessage *message, const CamelMessageInfo *info, CamelException *ex)
{
	/* no-op */
}

static void
vtrash_copy_messages_to (CamelFolder *source, GPtrArray *uids, CamelFolder *dest, CamelException *ex)
{
	/* don't allow the user to copy to or from the vtrash folder */
	camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
			      _("You cannot copy messages from this trash folder."));
}

static void
vtrash_move_messages_to (CamelFolder *source, GPtrArray *uids, CamelFolder *dest, CamelException *ex)
{
	CamelVeeMessageInfo *mi;
	int i;
	
	for (i = 0; i < uids->len; i++) {
		mi = (CamelVeeMessageInfo *)camel_folder_get_message_info(source, uids->pdata[i]);
		if (mi == NULL) {
			g_warning("Cannot find uid %s in source folder during move_to", (char *)uids->pdata[i]);
			continue;
		}

		if (dest == mi->folder) {
			/* Just undelete the original message */
			CAMEL_FOLDER_CLASS (dest)->set_message_flags (dest, uids->pdata[i], CAMEL_MESSAGE_DELETED, 0);
		} else {
			/* This means that the user is trying to move the message
			   from the vTrash to a folder other than the original. */
			GPtrArray *tuids;
			
			tuids = g_ptr_array_new ();
			g_ptr_array_add (tuids, uids->pdata[i]);
			/*CAMEL_FOLDER_CLASS (mi->folder)->move_messages_to (mi->folder, tuids, dest, ex);*/
			camel_folder_move_messages_to(mi->folder, tuids, dest, ex);
			g_ptr_array_free (tuids, TRUE);
		}

		camel_folder_free_message_info(source, (CamelMessageInfo *)mi);
	}
}
