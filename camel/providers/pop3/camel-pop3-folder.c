/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-pop3-folder.c : class for a pop3 folder */

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

#include "camel-pop3-folder.h"
#include "camel-pop3-store.h"
#include "camel-exception.h"
#include "camel-stream-mem.h"
#include "camel-stream-filter.h"
#include "camel-mime-message.h"

#include <stdlib.h>
#include <string.h>

#define CF_CLASS(o) (CAMEL_FOLDER_CLASS (CAMEL_OBJECT_GET_CLASS(o)))
static CamelFolderClass *parent_class;

static void pop3_finalize (CamelObject *object);

static void pop3_sync (CamelFolder *folder, gboolean expunge,
		       CamelException *ex);

static gint pop3_get_message_count (CamelFolder *folder);
static GPtrArray *pop3_get_uids (CamelFolder *folder);
static CamelMimeMessage *pop3_get_message (CamelFolder *folder, 
					   const char *uid,
					   CamelException *ex);
static void pop3_set_message_flags (CamelFolder *folder, const char *uid,
				    guint32 flags, guint32 set);

static GPtrArray *parse_listing (int count, char *data);

static void
camel_pop3_folder_class_init (CamelPop3FolderClass *camel_pop3_folder_class)
{
	CamelFolderClass *camel_folder_class =
		CAMEL_FOLDER_CLASS (camel_pop3_folder_class);

	parent_class = CAMEL_FOLDER_CLASS(camel_type_get_global_classfuncs (camel_folder_get_type ()));

	/* virtual method overload */
	camel_folder_class->sync = pop3_sync;

	camel_folder_class->get_message_count = pop3_get_message_count;
	camel_folder_class->get_uids = pop3_get_uids;
	camel_folder_class->free_uids = camel_folder_free_nop;

	camel_folder_class->get_message = pop3_get_message;
	camel_folder_class->set_message_flags = pop3_set_message_flags;
}

static void
camel_pop3_folder_init (gpointer object)
{
	CamelFolder *folder = CAMEL_FOLDER (object);

	folder->can_hold_messages = TRUE;
	folder->can_hold_folders = FALSE;
	folder->has_summary_capability = FALSE;
	folder->has_search_capability = FALSE;
}

CamelType
camel_pop3_folder_get_type (void)
{
	static CamelType camel_pop3_folder_type = CAMEL_INVALID_TYPE;

	if (!camel_pop3_folder_type) {
		camel_pop3_folder_type = camel_type_register (CAMEL_FOLDER_TYPE, "CamelPop3Folder",
							      sizeof (CamelPop3Folder),
							      sizeof (CamelPop3FolderClass),
							      (CamelObjectClassInitFunc) camel_pop3_folder_class_init,
							      NULL,
							      (CamelObjectInitFunc) camel_pop3_folder_init,
							      (CamelObjectFinalizeFunc) pop3_finalize);
	}

	return camel_pop3_folder_type;
}

void
pop3_finalize (CamelObject *object)
{
	CamelPop3Folder *pop3_folder = CAMEL_POP3_FOLDER (object);

	camel_folder_free_deep (NULL, pop3_folder->uids);
	g_free (pop3_folder->flags);
}

CamelFolder *
camel_pop3_folder_new (CamelStore *parent, CamelException *ex)
{
	CamelPop3Store *pop3_store = CAMEL_POP3_STORE (parent);
	CamelPop3Folder *pop3_folder;
	GPtrArray *uids;
	int status, count;
	char *data;

	status = camel_pop3_command (pop3_store, &data, "STAT");
	if (status != CAMEL_POP3_OK) {
		CamelService *service = CAMEL_SERVICE (parent);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not get message count from POP "
				      "server %s: %s.", service->url->host,
				      data ? data : "Unknown error");
		g_free (data);
		return NULL;
	}

	count = atoi (data);
	g_free (data);

	if (pop3_store->supports_uidl != FALSE) {
		status = camel_pop3_command (pop3_store, NULL, "UIDL");
		if (status != CAMEL_POP3_OK)
			pop3_store->supports_uidl = FALSE;
	}

	if (pop3_store->supports_uidl == FALSE) {
		int i;

		uids = g_ptr_array_new ();
		g_ptr_array_set_size (uids, count);

		for (i = 0; i < count; i++)
			uids->pdata[i] = g_strdup_printf ("%d", i + 1);
	} else {
		data = camel_pop3_command_get_additional_data (pop3_store, ex);
		if (camel_exception_is_set (ex))
			return NULL;

		uids = parse_listing (count, data);
		g_free (data);

		if (!uids) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      "Could not open folder: message "
					      "listing was incomplete.");
			return NULL;
		}
	}

	pop3_folder = CAMEL_POP3_FOLDER(camel_object_new (CAMEL_POP3_FOLDER_TYPE));
	CF_CLASS (pop3_folder)->init ((CamelFolder *)pop3_folder, parent,
				      NULL, "inbox", "/", TRUE, ex);
	pop3_folder->uids = uids;
	pop3_folder->flags = g_new0 (guint32, uids->len);

	return (CamelFolder *)pop3_folder;
}

static void
pop3_sync (CamelFolder *folder, gboolean expunge, CamelException *ex)
{
	CamelPop3Folder *pop3_folder;
	CamelPop3Store *pop3_store;
	int i, status;
	char *resp;

	if (!expunge)
		return;

	pop3_folder = CAMEL_POP3_FOLDER (folder);
	pop3_store = CAMEL_POP3_STORE (folder->parent_store);

	for (i = 0; i < pop3_folder->uids->len; i++) {
		if (pop3_folder->flags[i] & CAMEL_MESSAGE_DELETED) {
			status = camel_pop3_command (pop3_store, &resp,
						     "DELE %d", i + 1);
			if (status != CAMEL_POP3_OK) {
				camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
						      "Unable to sync folder"
						      "%s%s", resp ? ": " : "",
						      resp ? resp : "");
				g_free (resp);
				return;
			}
		}
	}

	camel_pop3_store_expunge (pop3_store, ex);
}


static GPtrArray *
parse_listing (int count, char *data)
{
	GPtrArray *ans;
	char *p;
	int index, len;

	ans = g_ptr_array_new ();
	g_ptr_array_set_size (ans, count);

	p = data;
	while (*p) {
		index = strtoul (p, &p, 10);
		len = strcspn (p, "\n");
		if (index <= count && *p == ' ')
			ans->pdata[index - 1] = g_strndup (p + 1, len - 1);
		p += len;
		if (*p == '\n')
			p++;
	}

	for (index = 0; index < count; index++) {
		if (ans->pdata[index] == NULL) {
			g_ptr_array_free (ans, TRUE);
			return NULL;
		}
	}

	return ans;
}

static int
uid_to_number (CamelPop3Folder *pop3_folder, const char *uid)
{
	int i;

	for (i = 0; i < pop3_folder->uids->len; i++) {
		if (!strcmp (uid, pop3_folder->uids->pdata[i]))
			return i + 1;
	}

	return -1;
}


static CamelMimeMessage *
pop3_get_message (CamelFolder *folder, const char *uid, CamelException *ex)
{
	int status, num;
	char *result, *body;
	CamelStream *msgstream;
	CamelMimeMessage *msg;

	num = uid_to_number (CAMEL_POP3_FOLDER (folder), uid);
	if (num == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_FOLDER_INVALID_UID,
				      "No message with uid %s", uid);
		return NULL;
	}

	status = camel_pop3_command (CAMEL_POP3_STORE (folder->parent_store),
				     &result, "RETR %d", num);
	if (status != CAMEL_POP3_OK) {
		CamelService *service = CAMEL_SERVICE (folder->parent_store);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not retrieve message from POP "
				      "server %s: %s.", service->url->host,
				      status == CAMEL_POP3_ERR ? result :
				      "Unknown error");
		g_free (result);
		return NULL;
	}
	g_free (result);

	body = camel_pop3_command_get_additional_data (CAMEL_POP3_STORE (folder->parent_store), ex);
	if (!body) {
		CamelService *service = CAMEL_SERVICE (folder->parent_store);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not retrieve message from POP "
				      "server %s: %s", service->url->host,
				      camel_exception_get_description (ex));
		return NULL;
	}

	msgstream = camel_stream_mem_new_with_buffer (body, strlen (body));
	g_free (body);
	
	msg = camel_mime_message_new ();
	camel_data_wrapper_construct_from_stream (CAMEL_DATA_WRAPPER (msg),
						  CAMEL_STREAM (msgstream));

	camel_object_unref (CAMEL_OBJECT (msgstream));

	return msg;
}

static void
pop3_set_message_flags (CamelFolder *folder, const char *uid,
			guint32 flags, guint32 set)
{
	CamelPop3Folder *pop3_folder = CAMEL_POP3_FOLDER (folder);
	int num;

	num = uid_to_number (pop3_folder, uid);
	if (num == -1)
		return;

	pop3_folder->flags[num - 1] =
		(pop3_folder->flags[num] & ~flags) | (set & flags);
}

static gint
pop3_get_message_count (CamelFolder *folder)
{
	CamelPop3Folder *pop3_folder = CAMEL_POP3_FOLDER (folder);

	return pop3_folder->uids->len;
}

static GPtrArray *
pop3_get_uids (CamelFolder *folder)
{
	CamelPop3Folder *pop3_folder = CAMEL_POP3_FOLDER (folder);

	return pop3_folder->uids;
}
