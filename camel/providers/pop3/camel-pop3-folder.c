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
#include "camel-mime-filter-crlf.h"

#include <stdlib.h>
#include <string.h>

#define CF_CLASS(o) (CAMEL_FOLDER_CLASS (GTK_OBJECT (o)->klass))
static CamelFolderClass *parent_class;

static void pop3_open (CamelFolder *folder, CamelFolderOpenMode mode,
		       CamelException *ex);
static void pop3_close (CamelFolder *folder, gboolean expunge,
			CamelException *ex);

static gint get_message_count (CamelFolder *folder, CamelException *ex);
static GPtrArray *get_uids (CamelFolder *folder, CamelException *ex);
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

	parent_class = gtk_type_class (camel_folder_get_type ());

	/* virtual method overload */
	camel_folder_class->open = pop3_open;
	camel_folder_class->close = pop3_close;

	camel_folder_class->get_message_count = get_message_count;
	camel_folder_class->get_uids = get_uids;

	camel_folder_class->get_message_by_uid = get_message_by_uid;
	camel_folder_class->delete_message_by_uid = delete_message_by_uid;
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

	pop3_folder->count = -1;
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


CamelFolder *camel_pop3_folder_new (CamelStore *parent, CamelException *ex)
{
	CamelFolder *folder =
		CAMEL_FOLDER (gtk_object_new (camel_pop3_folder_get_type (),
					      NULL));

	CF_CLASS (folder)->init (folder, parent, NULL, "inbox", '/', ex);
	return folder;
}

static void
pop3_open (CamelFolder *folder, CamelFolderOpenMode mode, CamelException *ex)
{
	camel_pop3_store_open (CAMEL_POP3_STORE (folder->parent_store), ex);
	if (camel_exception_get_id (ex) == CAMEL_EXCEPTION_NONE)
		parent_class->open (folder, mode, ex);
}

static void
pop3_close (CamelFolder *folder, gboolean expunge, CamelException *ex)
{
	camel_pop3_store_close (CAMEL_POP3_STORE (folder->parent_store),
				expunge, ex);
	if (camel_exception_get_id (ex) == CAMEL_EXCEPTION_NONE)
		parent_class->close (folder, expunge, ex);
}
				

static CamelMimeMessage *
get_message_by_uid (CamelFolder *folder, const char *uid, CamelException *ex)
{
	int status, id;
	char *result, *body;
	CamelStream *msgstream;
	CamelStreamFilter *f_stream;
	CamelMimeMessage *msg;
	CamelMimeFilter *filter;

	status = camel_pop3_command (CAMEL_POP3_STORE (folder->parent_store),
				     &result, "RETR %d", atoi (uid));
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

	f_stream = camel_stream_filter_new_with_stream (msgstream);
	filter = camel_mime_filter_crlf_new (CAMEL_MIME_FILTER_CRLF_DECODE, CAMEL_MIME_FILTER_CRLF_MODE_CRLF_DOTS);
	id = camel_stream_filter_add (f_stream, CAMEL_MIME_FILTER (filter));
	
	msg = camel_mime_message_new ();
	camel_data_wrapper_construct_from_stream (CAMEL_DATA_WRAPPER (msg), CAMEL_STREAM (f_stream));

	camel_stream_filter_remove (f_stream, id);
	camel_stream_close (CAMEL_STREAM (f_stream));
	gtk_object_unref (GTK_OBJECT (msgstream));
	gtk_object_unref (GTK_OBJECT (f_stream));

	return msg;
}

static void
delete_message_by_uid (CamelFolder *folder, const char *uid,
		       CamelException *ex)
{
	int status;
	char *resp;

	status = camel_pop3_command (CAMEL_POP3_STORE (folder->parent_store),
				     &resp, "DELE %d", atoi (uid));
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
	int status;
	char *result;

	if (pop3_folder->count != -1)
		return pop3_folder->count;

	status = camel_pop3_command (CAMEL_POP3_STORE (folder->parent_store),
				     &result, "STAT");
	if (status != CAMEL_POP3_OK) {
		CamelService *service = CAMEL_SERVICE (folder->parent_store);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not get message count from POP "
				      "server %s: %s.", service->url->host,
				      status == CAMEL_POP3_ERR ? result :
				      "Unknown error");
		g_free (result);
		return -1;
	}

	pop3_folder->count = atoi (result);
	g_free (result);
	return pop3_folder->count;
}

static GPtrArray *
get_uids (CamelFolder *folder, CamelException *ex)
{
	int count, i;
	GPtrArray *array;

	count = get_message_count (folder, ex);
	if (count == -1)
		return NULL;

	array = g_ptr_array_new ();
	g_ptr_array_set_size (array, count);
	for (i = 0; i < count; i++)
		array->pdata[i] = g_strdup_printf ("%d", i + 1);

	return array;
}
