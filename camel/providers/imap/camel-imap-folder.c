/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; fill-column: 160 -*- */
/* camel-imap-folder.c : Abstract class for an email folder */

/* 
 * Authors: Jeffrey Stedfast <fejj@helixcode.com> 
 *
 * Copyright (C) 2000 Helix Code, Inc.
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

#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

#include "camel-imap-folder.h"
#include "camel-imap-store.h"
#include "camel-imap-stream.h"
#include "string-utils.h"
#include "camel-stream.h"
#include "camel-stream-fs.h"
#include "camel-stream-mem.h"
#include "camel-stream-buffer.h"
#include "camel-data-wrapper.h"
#include "camel-mime-message.h"
#include "camel-stream-filter.h"
#include "camel-mime-filter-from.h"
#include "camel-mime-filter-crlf.h"
#include "camel-exception.h"

#define d(x)

#define CF_CLASS(o) (CAMEL_FOLDER_CLASS (GTK_OBJECT (o)->klass))

static CamelFolderClass *parent_class = NULL;

static void imap_init (CamelFolder *folder, CamelStore *parent_store,
		       CamelFolder *parent_folder, const gchar *name,
		       gchar separator, CamelException *ex);

static void imap_open (CamelFolder *folder, CamelFolderOpenMode mode, CamelException *ex);
static void imap_close (CamelFolder *folder, gboolean expunge, CamelException *ex);
#if 0
static gboolean imap_exists (CamelFolder *folder, CamelException *ex);
static gboolean imap_delete (CamelFolder *folder, gboolean recurse, CamelException *ex);
static gboolean imap_delete_messages (CamelFolder *folder, CamelException *ex);
#endif
static gint imap_get_message_count (CamelFolder *folder, CamelException *ex);
static void imap_append_message (CamelFolder *folder, CamelMimeMessage *message, CamelException *ex);
static GPtrArray *imap_get_uids (CamelFolder *folder, CamelException *ex);
static GPtrArray *imap_get_subfolder_names (CamelFolder *folder, CamelException *ex);
static GPtrArray *imap_get_summary (CamelFolder *folder, CamelException *ex);
static void imap_free_summary (CamelFolder *folder, GPtrArray *array);
static CamelMimeMessage *imap_get_message_by_uid (CamelFolder *folder, const gchar *uid, 
						  CamelException *ex);

static void imap_expunge (CamelFolder *folder, CamelException *ex);

#if 0
static void _copy_message_to (CamelFolder *folder, CamelMimeMessage *message, 
			      CamelFolder *dest_folder, CamelException *ex);
static const gchar *_get_message_uid (CamelFolder *folder, CamelMimeMessage *message, 
				      CamelException *ex);
#endif

static void imap_delete_message_by_uid(CamelFolder *folder, const gchar *uid, CamelException *ex);

static const CamelMessageInfo *imap_summary_get_by_uid(CamelFolder *f, const char *uid);

static GList *imap_search_by_expression(CamelFolder *folder, const char *expression, CamelException *ex);

static void imap_finalize (GtkObject *object);

static void
camel_imap_folder_class_init (CamelImapFolderClass *camel_imap_folder_class)
{
	CamelFolderClass *camel_folder_class = CAMEL_FOLDER_CLASS (camel_imap_folder_class);
	GtkObjectClass *gtk_object_class = GTK_OBJECT_CLASS (camel_folder_class);

	parent_class = gtk_type_class (camel_folder_get_type ());
		
	/* virtual method definition */

	/* virtual method overload */
	camel_folder_class->init = imap_init;

	camel_folder_class->open = imap_open;
	camel_folder_class->close = imap_close;
#if 0
	camel_folder_class->exists = imap_exists;
	camel_folder_class->create = imap_create;
	camel_folder_class->delete = imap_delete;
	camel_folder_class->delete_messages = imap_delete_messages;
#endif
	camel_folder_class->get_message_count = imap_get_message_count;
	camel_folder_class->append_message = imap_append_message;
	camel_folder_class->get_uids = imap_get_uids;
	camel_folder_class->get_subfolder_names = imap_get_subfolder_names;
	camel_folder_class->get_summary = imap_get_summary;
	camel_folder_class->free_summary = imap_free_summary;
	camel_folder_class->expunge = imap_expunge;

	camel_folder_class->get_message_by_uid = imap_get_message_by_uid;
	camel_folder_class->delete_message_by_uid = imap_delete_message_by_uid;

	camel_folder_class->search_by_expression = imap_search_by_expression;

	camel_folder_class->summary_get_by_uid = imap_summary_get_by_uid;

	gtk_object_class->finalize = imap_finalize;	
}

static void
camel_imap_folder_init (gpointer object, gpointer klass)
{
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (object);
	CamelFolder *folder = CAMEL_FOLDER (object);

	folder->can_hold_messages = TRUE;
	folder->can_hold_folders = TRUE;
	folder->has_summary_capability = FALSE;
	folder->has_search_capability = FALSE;

	imap_folder->count = -1;
}

GtkType
camel_imap_folder_get_type (void)
{
	static GtkType camel_imap_folder_type = 0;
	
	if (!camel_imap_folder_type)	{
		GtkTypeInfo camel_imap_folder_info =	
		{
			"CamelImapFolder",
			sizeof (CamelImapFolder),
			sizeof (CamelImapFolderClass),
			(GtkClassInitFunc) camel_imap_folder_class_init,
			(GtkObjectInitFunc) NULL,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		camel_imap_folder_type = gtk_type_unique (CAMEL_FOLDER_TYPE, &camel_imap_folder_info);
	}
	
	return camel_imap_folder_type;
}

CamelFolder *
camel_imap_folder_new (CamelStore *parent, CamelException *ex)
{
	CamelFolder *folder = CAMEL_FOLDER (gtk_object_new (camel_imap_folder_get_type (), NULL));
	
	CF_CLASS (folder)->init (folder, parent, NULL, "INBOX", '/', ex);

	return folder;
}

static void           
imap_finalize (GtkObject *object)
{
	GTK_OBJECT_CLASS (parent_class)->finalize (object);
}

static void 
imap_init (CamelFolder *folder, CamelStore *parent_store, CamelFolder *parent_folder,
	   const gchar *name, gchar separator, CamelException *ex)
{
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);
	
	/* call parent method */
	parent_class->init (folder, parent_store, parent_folder, name, separator, ex);
	if (camel_exception_get_id (ex))
		return;

	/* we assume that the parent init
	   method checks for the existance of @folder */
	folder->can_hold_messages = TRUE;
	folder->can_hold_folders = TRUE;
	folder->has_summary_capability = FALSE;  /* TODO: double-check this */
	folder->has_search_capability = TRUE;    /* This is really a "maybe" */

	
        /* some IMAP daemons support user-flags           *
	 * I would not, however, rely on this feature as  *
	 * most IMAP daemons are not 100% RFC compliant   */
	folder->permanent_flags = CAMEL_MESSAGE_SEEN |
		CAMEL_MESSAGE_ANSWERED |
		CAMEL_MESSAGE_FLAGGED |
		CAMEL_MESSAGE_DELETED |
		CAMEL_MESSAGE_DRAFT |
		CAMEL_MESSAGE_USER;

	
 	imap_folder->summary = NULL;
 	imap_folder->search = NULL;
}

static void
imap_open (CamelFolder *folder, CamelFolderOpenMode mode, CamelException *ex)
{
	gchar *result;
	gint status;

	camel_imap_store_open (CAMEL_IMAP_STORE (folder->parent_store), ex);
	if (camel_exception_get_id (ex) == CAMEL_EXCEPTION_NONE) {
		/* do we actually want to do this? probably not */
		parent_class->open (folder, mode, ex);

		/* SELECT the IMAP mail spool */
		status = camel_imap_command_extended (CAMEL_IMAP_STORE (folder->parent_store), folder,
						      &result, "SELECT %s", folder->full_name);

		if (status != CAMEL_IMAP_OK) {
			CamelService *service = CAMEL_SERVICE (folder->parent_store);
			camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
					      "Could not SELECT %s on IMAP server %s: %s.",
					      folder->full_name,
					      service->url->host, 
					      status == CAMEL_IMAP_ERR ? result :
					      "Unknown error");
			g_free (result);
			return;
		}

		g_free(result);
	}
}

static void
imap_close (CamelFolder *folder, gboolean expunge, CamelException *ex)
{
	/* TODO: actually code this method */
	camel_imap_store_close (CAMEL_IMAP_STORE (folder->parent_store), expunge, ex);
	if (camel_exception_get_id (ex) == CAMEL_EXCEPTION_NONE) 
		parent_class->close (folder, expunge, ex);

}

static void
imap_expunge (CamelFolder *folder, CamelException *ex)
{
	gchar *result;
	gint status;

	g_return_if_fail (folder != NULL);
	
	status = camel_imap_command_extended (CAMEL_IMAP_STORE (folder->parent_store), folder,
					      &result, "EXPUNGE");
	
	if (status != CAMEL_IMAP_OK) {
		CamelService *service = CAMEL_SERVICE (folder->parent_store);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not EXPUNGE from IMAP server %s: %s.",
				      service->url->host,
				      status == CAMEL_IMAP_ERR ? result :
				      "Unknown error");
		g_free (result);
		return;
	}
	
	g_free(result);
}

#if 0
static gboolean
imap_exists (CamelFolder *folder, CamelException *ex)
{
	/* make sure the folder exists */
	GPtrArray *lsub;
	gboolean exists = FALSE;
	int i, max;

	g_return_val_if_fail (folder != NULL, FALSE);

	/* check if the imap file path is determined */
	if (!folder->full_name) {
		camel_exception_set (ex, CAMEL_EXCEPTION_FOLDER_INVALID,
				     "undetermined folder file path. Maybe use set_name ?");
		return FALSE;
	}

	/* check if the imap dir path is determined */
	if (!folder->full_name) {
		camel_exception_set (ex, CAMEL_EXCEPTION_FOLDER_INVALID,
				     "undetermined folder directory path. Maybe use set_name ?");
		return FALSE;
	}

	/* Get a listing of the folders that exist */
	lsub = imap_get_subfolder_names (folder, ex);

	/* look to see if any of those subfolders match... */
	max = lsub->len;
	for (i = 0; i < max; i++)
		if (!strcmp(g_ptr_array_index(lsub, i), folder->full_name))
		{
			exists = TRUE;
			break;
		}

	g_ptr_array_free (lsub, TRUE);

	return exists;
}
#endif

#if 0
static gboolean
imap_delete (CamelFolder *folder, gboolean recurse, CamelException *ex)
{
	/* TODO: code this & what should this do? delete messages or the folder? */
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);
	gboolean folder_already_exists;

	g_return_val_if_fail (folder != NULL, FALSE);

	/* check if the folder object exists */

	/* in the case where the folder does not exist, 
	   return immediatly */
	folder_already_exists = camel_folder_exists (folder, ex);
	if (camel_exception_get_id (ex))
		return FALSE;

	if (!folder_already_exists)
		return TRUE;


	/* call default implementation.
	   It should delete the messages in the folder
	   and recurse the operation to subfolders */
	parent_class->delete (folder, recurse, ex);
	if (camel_exception_get_id (ex))
		return FALSE;

	if (!(folder->full_name || folder->name)) {
		camel_exception_set (ex, CAMEL_EXCEPTION_FOLDER_INVALID,
				     "invalid folder path. Use set_name ?");
		return FALSE;
	}
	
        /* delete the directory - we must start with the leaves and work
	   back to the root if we are to delete recursively */

	/* TODO: Finish this... */

	return TRUE;
}
#endif

static gint
imap_get_message_count (CamelFolder *folder, CamelException *ex)
{
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);
	gchar *result, *msg_count;
	gint status;

	g_return_val_if_fail (folder != NULL, -1);

	/* If we already have a count, return */
	if (imap_folder->count != -1)
		return imap_folder->count;

	status = camel_imap_command_extended (CAMEL_IMAP_STORE (folder->parent_store), folder,
					      &result, "STATUS %s (MESSAGES)", folder->full_name);

	if (status != CAMEL_IMAP_OK) {
		CamelService *service = CAMEL_SERVICE (folder->parent_store);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not get message count from IMAP "
				      "server %s: %s.", service->url->host,
				      status == CAMEL_IMAP_ERR ? result :
				      "Unknown error");
		g_free (result);
		return -1;
	}

	/* parse out the message count - should come in the form: "* STATUS <folder> (MESSAGES <count>)\r\n" */
	if (result && *result == '*') {
		if ((msg_count = strstr(result, "MESSAGES")) != NULL) {
			msg_count += strlen("MESSAGES") + 1;

			for ( ; *msg_count == ' '; msg_count++);
			
			/* we should now be pointing to the message count */
			imap_folder->count = atoi(msg_count);
		}
	}
	g_free(result);

	return imap_folder->count;
}

/* TODO: Optimize this later - there may be times when moving/copying a message from the
   same IMAP store in which case we'd want to use IMAP's COPY command */
static void
imap_append_message (CamelFolder *folder, CamelMimeMessage *message, CamelException *ex)
{
	CamelStreamMem *mem;
	gchar *result;
	gint status;

	g_return_if_fail (folder != NULL);
	g_return_if_fail (message != NULL);

	/* write the message to a CamelStreamMem so we can get it's size */
	mem = CAMEL_STREAM_MEM (camel_stream_mem_new());
	if (camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (message), CAMEL_STREAM (mem)) == -1) {
		CamelService *service = CAMEL_SERVICE (folder->parent_store);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "Could not APPEND message to IMAP server %s: %s.",
				      service->url->host,
				      g_strerror (errno));

	        return;
	}

	mem->buffer = g_byte_array_append(mem->buffer, g_strdup("\r\n"), 3);
	status = camel_imap_command(CAMEL_IMAP_STORE (folder->parent_store),
				    folder, &result,
				    "APPEND %s (\\Seen) {%d}\r\n%s",
				    folder->full_name,
				    mem->buffer->len,
				    mem->buffer->data);

	if (status != CAMEL_IMAP_OK) {
		CamelService *service = CAMEL_SERVICE (folder->parent_store);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not APPEND message to IMAP server %s: %s.",
				      service->url->host,
				      status == CAMEL_IMAP_ERR ? result :
				      "Unknown error");
		g_free (result);
		return;
	}

	g_free(result);
	return;
}

static GPtrArray *
imap_get_uids (CamelFolder *folder, CamelException *ex) 
{
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);
	CamelImapMessageInfo *info;
	GPtrArray *array;
	gint i, count;

	count = camel_folder_summary_count(CAMEL_FOLDER_SUMMARY (imap_folder->summary));
	array = g_ptr_array_new ();
	g_ptr_array_set_size (array, count);
	for (i = 0; i < count; i++) {
		info = (CamelImapMessageInfo *) camel_folder_summary_index(CAMEL_FOLDER_SUMMARY (imap_folder->summary), i);
		array->pdata[i] = g_strdup(info->info.uid);
	}
	
	return array;
}

static GPtrArray *
imap_get_subfolder_names (CamelFolder *folder, CamelException *ex)
{
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);
	GPtrArray *listing;
	gint status;
	gchar *result;
	
	g_return_val_if_fail (folder != NULL, g_ptr_array_new());
	
	if (imap_folder->count != -1)
		return g_ptr_array_new ();
	
	status = camel_imap_command_extended(CAMEL_IMAP_STORE (folder->parent_store), folder,
					     &result, "LSUB \"\" \"%s\"", folder->full_name);
	
	if (status != CAMEL_IMAP_OK) {
		CamelService *service = CAMEL_SERVICE (folder->parent_store);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not get subfolder listing from IMAP "
				      "server %s: %s.", service->url->host,
				      status == CAMEL_IMAP_ERR ? result :
				      "Unknown error");
		g_free (result);
		return g_ptr_array_new ();
	}
	
	/* parse out the subfolders */
	listing = g_ptr_array_new ();
	g_ptr_array_add(listing, g_strdup("INBOX"));
	if (result) {
		char *ptr = result;
		
		while (*ptr == '*') {
			gchar *flags, *end, *dir_sep, *param = NULL;
			
			ptr = flags = strchr(ptr, '(') + 1;    /* jump to the flags section */
			end = strchr(flags, ')');              /* locate end of flags */
			flags = g_strndup(flags, (gint)(end - flags));
			
			if (strstr(flags, "\\NoSelect")) {
				g_free(flags);
				continue;
			}
			g_free(flags);

			ptr = dir_sep = strchr(ptr, '"') + 1;  /* jump to the first param */
			end = strchr(param, '"');              /* locate the end of the param */
			dir_sep = g_strndup(dir_sep, (gint)(end - param));

			/* skip to the actual directory parameter */
			for (ptr = end++; *ptr == ' '; ptr++);
                        for (end = ptr; *end && *end != '\n'; end++);
			param = g_strndup(ptr, (gint)(end - ptr));

			g_ptr_array_add(listing, param);

			g_free(dir_sep);  /* TODO: decide if we really need dir_sep */

			if (*end)
				ptr = end + 1;
			else
				ptr = end;
		}
	}
	g_free(result);

	return listing;
}

static void
imap_delete_message_by_uid (CamelFolder *folder, const gchar *uid, CamelException *ex)
{
	gchar *result;
	gint status;

	status = camel_imap_command_extended(CAMEL_IMAP_STORE (folder->parent_store), folder,
					     &result, "UID STORE %s +FLAGS.SILENT (\\Deleted)", uid);

	if (status != CAMEL_IMAP_OK) {
		CamelService *service = CAMEL_SERVICE (folder->parent_store);
		
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not mark message %s as 'Deleted' on IMAP server %s: %s",
				      uid, service->url->host,
				      status == CAMEL_IMAP_ERR ? result :
				      "Unknown error");
		g_free (result);
		return;
	}

	g_free (result);
	return;
}

/* track flag changes in the summary */
static void
message_changed (CamelMimeMessage *m, int type, CamelImapFolder *mf)
{
	CamelMessageInfo *info;
	CamelFlag *flag;

	printf("Message changed: %s: %d\n", m->message_uid, type);
	switch (type) {
	case MESSAGE_FLAGS_CHANGED:
		info = camel_folder_summary_uid(CAMEL_FOLDER_SUMMARY (mf->summary), m->message_uid);
		if (info) {
			info->flags = m->flags | CAMEL_MESSAGE_FOLDER_FLAGGED;
			camel_flag_list_free(&info->user_flags);
			flag = m->user_flags;
			while (flag) {
				camel_flag_set(&info->user_flags, flag->name, TRUE);
				flag = flag->next;
			}
			camel_folder_summary_touch(CAMEL_FOLDER_SUMMARY (mf->summary));
		} else
			g_warning("Message changed event on message not in summary: %s", m->message_uid);
		break;
	default:
		printf("Unhandled message change event: %d\n", type);
		break;
	}
}

static CamelMimeMessage *
imap_get_message_by_uid (CamelFolder *folder, const gchar *uid, CamelException *ex)
{
	CamelStream *imap_stream;
	CamelStream *msgstream;
	CamelStreamFilter *f_stream;   /* will be used later w/ crlf filter */
	CamelMimeFilter *filter;       /* crlf/dot filter */
	CamelMimeMessage *msg;
	CamelMimePart *part;
	CamelDataWrapper *cdw;
	gchar *cmdbuf;
	int id;

	/* TODO: fetch the correct part, get rid of the hard-coded stuff */
	cmdbuf = g_strdup_printf ("UID FETCH %s BODY[TEXT]", uid);
	imap_stream = camel_imap_stream_new (CAMEL_IMAP_FOLDER (folder), cmdbuf);
	g_free (cmdbuf);


	/* Temp hack - basically we read in the entire message instead of getting a part as it's needed */
	msgstream = camel_stream_mem_new ();
	camel_stream_write_to_stream (msgstream, CAMEL_STREAM (imap_stream));
	gtk_object_unref (GTK_OBJECT (imap_stream));
	
	f_stream = camel_stream_filter_new_with_stream (msgstream);
	filter = camel_mime_filter_crlf_new (CAMEL_MIME_FILTER_CRLF_DECODE, CAMEL_MIME_FILTER_CRLF_MODE_CRLF_DOTS);
	id = camel_stream_filter_add (f_stream, CAMEL_MIME_FILTER (filter));
	
	msg = camel_mime_message_new ();
	
	/*cdw = camel_data_wrapper_new ();*/
	/*camel_data_wrapper_construct_from_stream (cdw, CAMEL_STREAM (f_stream));*/
	camel_data_wrapper_construct_from_stream (CAMEL_DATA_WRAPPER (msg), CAMEL_STREAM (f_stream));
	
	camel_stream_filter_remove (f_stream, id);
	camel_stream_close (CAMEL_STREAM (f_stream));
	gtk_object_unref (GTK_OBJECT (msgstream));
	gtk_object_unref (GTK_OBJECT (f_stream));
	
	/*camel_data_wrapper_set_mime_type (cdw, "text/plain");*/

	/*camel_medium_set_content_object (CAMEL_MEDIUM (msg), CAMEL_DATA_WRAPPER (cdw));*/
	/*gtk_object_unref (GTK_OBJECT (cdw));*/
	
	return msg;
}

#if 0
static CamelMimeMessage *
imap_get_message_by_uid (CamelFolder *folder, const gchar *uid, CamelException *ex)
{
	/* NOTE: oh boy, this is gonna be complicated */
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);
	CamelStreamMem *message_stream = NULL;
	CamelMimeMessage *message = NULL;
	CamelImapMessageInfo *info;
	CamelMimeParser *parser = NULL;
	gchar *buffer, *result;
	gint len, status;

	/* get the message summary info */
	info = (CamelImapMessageInfo *)camel_folder_summary_uid(CAMEL_FOLDER_SUMMARY (imap_folder->summary), uid);

	if (info == NULL) {
		errno = ENOENT;
		goto fail;
	}

	/* if this has no content, its an error in the library */
	g_assert(info->info.content);
	g_assert(info->frompos != -1);

	/* get our message buffer */
	status = camel_imap_command_extended(CAMEL_IMAP_STORE (folder->parent_store), folder,
					     &result, "UID FETCH %s (FLAGS BODY[])", uid);

	if (status != CAMEL_IMAP_OK) {
		CamelService *service = CAMEL_SERVICE (folder->parent_store);
		
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not mark message %s as 'Deleted' on IMAP server %s: %s",
				      uid, service->url->host,
				      status == CAMEL_IMAP_ERR ? result :
				      "Unknown error");
		g_free (result);
		goto fail;
	}
	
	/* where we read from */
	message_stream = CAMEL_STREAM_MEM (camel_stream_mem_new_with_buffer (result, strlen(result)));
	if (message_stream == NULL)
		goto fail;

	/* we use a parser to verify the message is correct, and in the correct position */
	parser = camel_mime_parser_new();
	camel_mime_parser_init_with_stream(parser, CAMEL_STREAM (message_stream));
	gtk_object_unref(GTK_OBJECT (message_stream));
	camel_mime_parser_scan_from(parser, TRUE);

	camel_mime_parser_seek(parser, info->frompos, SEEK_SET);
	if (camel_mime_parser_step(parser, &buffer, &len) != HSCAN_FROM) {
		g_warning("File appears truncated");
		goto fail;
	}

	if (camel_mime_parser_tell_start_from(parser) != info->frompos) {
		g_warning("Summary doesn't match the folder contents!  eek!"
			  "  expecting offset %ld got %ld", (long int)info->frompos,
			  (long int)camel_mime_parser_tell_start_from(parser));
		errno = EINVAL;
		goto fail;
	}

	message = camel_mime_message_new();
	if (camel_mime_part_construct_from_parser(CAMEL_MIME_PART (message), parser) == -1) {
		g_warning("Construction failed");
		goto fail;
	}

	/* we're constructed, finish setup and clean up */
	message->folder = folder;
	gtk_object_ref(GTK_OBJECT (folder));
	message->message_uid = g_strdup(uid);
	message->flags = info->info.flags;
	gtk_signal_connect(GTK_OBJECT (message), "message_changed", message_changed, folder);

	gtk_object_unref(GTK_OBJECT (parser));

	return message;

 fail:
	camel_exception_setv (ex, CAMEL_EXCEPTION_FOLDER_INVALID_UID,
			      "Cannot get message: %s",
			      g_strerror(errno));

	if (parser)
		gtk_object_unref(GTK_OBJECT (parser));
	if (message)
		gtk_object_unref(GTK_OBJECT (message));

	return NULL;
}
#endif

GPtrArray *
imap_get_summary (CamelFolder *folder, CamelException *ex)
{
	/* TODO: what should we do here?? */
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);

	return CAMEL_FOLDER_SUMMARY (imap_folder->summary)->messages;
}

void
imap_free_summary (CamelFolder *folder, GPtrArray *array)
{
	/* no-op */
	return;
}

/* get a single message info, by uid */
static const CamelMessageInfo *
imap_summary_get_by_uid (CamelFolder *f, const char *uid)
{
	/* TODO: what do we do here? */
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (f);

	return camel_folder_summary_uid(CAMEL_FOLDER_SUMMARY (imap_folder->summary), uid);
}

static GList *
imap_search_by_expression (CamelFolder *folder, const char *expression, CamelException *ex)
{
	/* TODO: find a good way of doing this */
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);

	if (imap_folder->search == NULL) {
		imap_folder->search = camel_folder_search_new();
	}

	camel_folder_search_set_folder(imap_folder->search, folder);
	if (imap_folder->summary)
		/* FIXME: dont access summary array directly? */
		camel_folder_search_set_summary(imap_folder->search,
						CAMEL_FOLDER_SUMMARY (imap_folder->summary)->messages);
	camel_folder_search_set_body_index(imap_folder->search, imap_folder->index);

	return camel_folder_search_execute_expression(imap_folder->search, expression, ex);
}





