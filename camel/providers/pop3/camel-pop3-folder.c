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

#define CF_CLASS(o) (CAMEL_FOLDER_CLASS (GTK_OBJECT (o)->klass))
static CamelFolderClass *parent_class;

static void finalize (GtkObject *object);

static void pop3_sync (CamelFolder *folder, gboolean expunge,
		       CamelException *ex);

static gint get_message_count (CamelFolder *folder, CamelException *ex);
static GPtrArray *get_uids (CamelFolder *folder, CamelException *ex);
static void free_uids (CamelFolder *folder, GPtrArray *uids);
static CamelMimeMessage *get_message_by_uid (CamelFolder *folder, 
					     const char *uid,
					     CamelException *ex);
static void delete_message_by_uid (CamelFolder *folder, const char *uid,
				   CamelException *ex);


static void
camel_pop3_folder_class_init (CamelPop3FolderClass *camel_pop3_folder_class)
{
	CamelFolderClass *camel_folder_class =
		CAMEL_FOLDER_CLASS (camel_pop3_folder_class);
	GtkObjectClass *object_class =
		GTK_OBJECT_CLASS (camel_pop3_folder_class);

	parent_class = gtk_type_class (camel_folder_get_type ());

	/* virtual method overload */
	camel_folder_class->sync = pop3_sync;

	camel_folder_class->get_message_count = get_message_count;
	camel_folder_class->get_uids = get_uids;
	camel_folder_class->free_uids = free_uids;

	camel_folder_class->get_message_by_uid = get_message_by_uid;
	camel_folder_class->delete_message_by_uid = delete_message_by_uid;

	object_class->finalize = finalize;
}


static void
camel_pop3_folder_init (gpointer object, gpointer klass)
{
	CamelPop3Folder *pop3_folder = CAMEL_POP3_FOLDER (object);
	CamelFolder *folder = CAMEL_FOLDER (object);

	folder->can_hold_messages = TRUE;
	folder->can_hold_folders = FALSE;
	folder->has_summary_capability = FALSE;
	folder->has_search_capability = FALSE;

	pop3_folder->uids = NULL;
}


GtkType
camel_pop3_folder_get_type (void)
{
	static GtkType camel_pop3_folder_type = 0;

	if (!camel_pop3_folder_type) {
		GtkTypeInfo camel_pop3_folder_info =	
		{
			"CamelPop3Folder",
			sizeof (CamelPop3Folder),
			sizeof (CamelPop3FolderClass),
			(GtkClassInitFunc) camel_pop3_folder_class_init,
			(GtkObjectInitFunc) camel_pop3_folder_init,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		camel_pop3_folder_type = gtk_type_unique (CAMEL_FOLDER_TYPE, &camel_pop3_folder_info);
	}

	return camel_pop3_folder_type;
}


void
finalize (GtkObject *object)
{
	CamelPop3Folder *pop3_folder = CAMEL_POP3_FOLDER (object);

	g_ptr_array_free (pop3_folder->uids, TRUE);
}

CamelFolder *
camel_pop3_folder_new (CamelStore *parent, CamelException *ex)
{
	CamelFolder *folder =
		CAMEL_FOLDER (gtk_object_new (camel_pop3_folder_get_type (),
					      NULL));

	CF_CLASS (folder)->init (folder, parent, NULL, "inbox", "/", TRUE, ex);
	return folder;
}

static void
pop3_sync (CamelFolder *folder, gboolean expunge, CamelException *ex)
{
	if (expunge)
		camel_pop3_store_expunge (CAMEL_POP3_STORE (folder->parent_store), ex);
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
uid_to_number (CamelFolder *folder, const char *uid, CamelException *ex)
{
	CamelPop3Folder *pop3_folder = CAMEL_POP3_FOLDER (folder);
	int i;

	if (!get_uids (folder, ex))
		return -1;

	for (i = 0; i < pop3_folder->uids->len; i++) {
		if (!strcmp (uid, pop3_folder->uids->pdata[i]))
			return i + 1;
	}

	return -1;
}


static CamelMimeMessage *
get_message_by_uid (CamelFolder *folder, const char *uid, CamelException *ex)
{
	int status, num;
	char *result, *body;
	CamelStream *msgstream;
	CamelMimeMessage *msg;

	num = uid_to_number (folder, uid, ex);
	if (num == -1)
		return NULL;

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
	camel_data_wrapper_construct_from_stream (CAMEL_DATA_WRAPPER (msg), CAMEL_STREAM (msgstream));

	gtk_object_unref (GTK_OBJECT (msgstream));

	return msg;
}

static void
delete_message_by_uid (CamelFolder *folder, const char *uid,
		       CamelException *ex)
{
	int status, num;
	char *resp;

	num = uid_to_number (folder, uid, ex);
	if (num == -1)
		return;

	status = camel_pop3_command (CAMEL_POP3_STORE (folder->parent_store),
				     &resp, "DELE %d", num);
	if (status != CAMEL_POP3_OK) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_FOLDER_INVALID_UID,
				      "Unable to delete message %s%s%s",
				      uid, resp ? ": " : "",
				      resp ? resp : "");
	}
	g_free (resp);
}

static gint
get_message_count (CamelFolder *folder, CamelException *ex)
{
	CamelPop3Folder *pop3_folder = CAMEL_POP3_FOLDER (folder);

	if (!get_uids (folder, ex))
		return -1;

	return pop3_folder->uids->len;
}

static GPtrArray *
get_uids (CamelFolder *folder, CamelException *ex)
{
	CamelPop3Store *pop3_store = CAMEL_POP3_STORE (folder->parent_store);
	CamelPop3Folder *pop3_folder = CAMEL_POP3_FOLDER (folder);
	int count, status;
	char *data;

	if (pop3_folder->uids)
		return pop3_folder->uids;

	status = camel_pop3_command (pop3_store, &data, "STAT");
	if (status != CAMEL_POP3_OK) {
		CamelService *service = CAMEL_SERVICE (folder->parent_store);
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

		pop3_folder->uids = g_ptr_array_new ();
		g_ptr_array_set_size (pop3_folder->uids, count);

		for (i = 0; i < count; i++) {
			pop3_folder->uids->pdata[i] =
				g_strdup_printf ("%d", i + 1);
		}

		return pop3_folder->uids;
	}

	data = camel_pop3_command_get_additional_data (pop3_store, ex);
	if (camel_exception_is_set (ex))
		return NULL;

	pop3_folder->uids = parse_listing (count, data);
	g_free (data);

	if (!pop3_folder->uids) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "UID listing from server was "
				      "incomplete.");
	}

	return pop3_folder->uids;
}

static void
free_uids (CamelFolder *folder, GPtrArray *uids)
{
	;
}
