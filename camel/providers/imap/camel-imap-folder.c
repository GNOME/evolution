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
#include "camel-mime-utils.h"

#define d(x)

#define CF_CLASS(o) (CAMEL_FOLDER_CLASS (GTK_OBJECT (o)->klass))

static CamelFolderClass *parent_class = NULL;

static void imap_init (CamelFolder *folder, CamelStore *parent_store,
		       CamelFolder *parent_folder, const gchar *name,
		       gchar *separator, gboolean path_begns_with_sep,
		       CamelException *ex);

static void imap_sync (CamelFolder *folder, gboolean expunge, CamelException *ex);
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

static void imap_delete_message_by_uid (CamelFolder *folder, const gchar *uid, CamelException *ex);

static const CamelMessageInfo *imap_summary_get_by_uid (CamelFolder *f, const char *uid);

static GList *imap_search_by_expression (CamelFolder *folder, const char *expression, CamelException *ex);

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

	camel_folder_class->sync = imap_sync;
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
	folder->has_summary_capability = TRUE;
	folder->has_search_capability = FALSE; /* default -  we have to query IMAP to know for sure */

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
			(GtkObjectInitFunc) camel_imap_folder_init,
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
	
	CF_CLASS (folder)->init (folder, parent, NULL, "INBOX", "/", FALSE, ex);

	return folder;
}

static void           
imap_finalize (GtkObject *object)
{
	/* TODO: do we need to do more here? */
	GTK_OBJECT_CLASS (parent_class)->finalize (object);
}

static void 
imap_init (CamelFolder *folder, CamelStore *parent_store, CamelFolder *parent_folder,
	   const gchar *name, gchar *separator, gboolean path_begins_with_sep, CamelException *ex)
{
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);
	int status;
	char *result;
	
	/* call parent method */
	parent_class->init (folder, parent_store, parent_folder, name, separator, path_begins_with_sep, ex);
	if (camel_exception_get_id (ex))
		return;

	/* we assume that the parent init
	   method checks for the existance of @folder */
	folder->can_hold_messages = TRUE;
	folder->can_hold_folders = TRUE;
	folder->has_summary_capability = TRUE;

	/* now lets find out if we can do searches... */
	status = camel_imap_command_extended (CAMEL_IMAP_STORE (folder->parent_store), folder,
					      &result, "CAPABILITY");
	
	/* ugh, I forgot that CAPABILITY doesn't have a response code */
	if (status != CAMEL_IMAP_OK) {
		CamelService *service = CAMEL_SERVICE (folder->parent_store);
		
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not get capabilities on IMAP server %s: %s.",
				      service->url->host, 
				      status == CAMEL_IMAP_ERR ? result :
				      "Unknown error");
	}
	
	if (strstrcase (result, "SEARCH"))
		folder->has_search_capability = TRUE;
	else
		folder->has_search_capability = FALSE;
	
	g_free (result);

	fprintf (stderr, "IMAP provider does%shave SEARCH support\n", folder->has_search_capability ? " " : "n't ");
	
        /* some IMAP daemons support user-flags           *
	 * I would not, however, rely on this feature as  *
	 * most IMAP daemons are not 100% RFC compliant   */
	folder->permanent_flags = CAMEL_MESSAGE_SEEN |
		CAMEL_MESSAGE_ANSWERED |
		CAMEL_MESSAGE_FLAGGED |
		CAMEL_MESSAGE_DELETED |
		CAMEL_MESSAGE_DRAFT |
		CAMEL_MESSAGE_USER;

	
 	imap_folder->search = NULL;

	/* SELECT the IMAP mail spool */
	status = camel_imap_command_extended (CAMEL_IMAP_STORE (folder->parent_store), folder,
					      &result, "SELECT %s", folder->full_name);
	if (status != CAMEL_IMAP_OK) {
		CamelService *service = CAMEL_SERVICE (folder->parent_store);
		
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not SELECT %s on IMAP server %s: %s.",
				      folder->full_name, service->url->host, 
				      status == CAMEL_IMAP_ERR ? result :
				      "Unknown error");
	}
	g_free (result);
}

static void
imap_sync (CamelFolder *folder, gboolean expunge, CamelException *ex)
{
	/* TODO: actually code this method */
	if (expunge)
		imap_expunge (folder, ex);
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
	for (i = 0; i < max; i++) {
		if (!strcmp(g_ptr_array_index (lsub, i), folder->full_name)) {
			exists = TRUE;
			break;
		}
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
		if ((msg_count = strstr (result, "MESSAGES")) != NULL) {
			msg_count += strlen ("MESSAGES") + 1;

			for ( ; *msg_count == ' '; msg_count++);
			
			/* we should now be pointing to the message count */
			imap_folder->count = atoi (msg_count);
		}
	}
	g_free (result);

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

	mem->buffer = g_byte_array_append (mem->buffer, g_strdup("\r\n"), 3);
	status = camel_imap_command (CAMEL_IMAP_STORE (folder->parent_store),
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
	CamelMessageInfo *info;
	GPtrArray *array, *infolist;
	gint i, count;

	infolist = imap_get_summary (folder, ex);
	count = infolist->len;
	
	array = g_ptr_array_new ();
	g_ptr_array_set_size (array, count);
	for (i = 0; i < count; i++) {
		info = (CamelMessageInfo *) g_ptr_array_index (infolist, i);
		array->pdata[i] = g_strdup (info->uid);
	}

	imap_free_summary (folder, infolist);
	
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
	
	status = camel_imap_command_extended (CAMEL_IMAP_STORE (folder->parent_store), folder,
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
	g_ptr_array_add (listing, g_strdup("INBOX"));
	if (result) {
		char *ptr = result;
		
		while (*ptr == '*') {
			gchar *flags, *end, *dir_sep, *param = NULL;
			
			ptr = flags = strchr (ptr, '(') + 1;    /* jump to the flags section */
			end = strchr (flags, ')');              /* locate end of flags */
			flags = g_strndup(flags, (gint)(end - flags));
			
			if (strstr (flags, "\\NoSelect")) {
				g_free (flags);
				continue;
			}
			g_free (flags);

			ptr = dir_sep = strchr (ptr, '"') + 1;  /* jump to the first param */
			end = strchr (param, '"');              /* locate the end of the param */
			dir_sep = g_strndup (dir_sep, (gint)(end - param));

			/* skip to the actual directory parameter */
			for (ptr = end++; *ptr == ' '; ptr++);
                        for (end = ptr; *end && *end != '\n'; end++);
			param = g_strndup (ptr, (gint)(end - ptr));

			g_ptr_array_add (listing, param);

			g_free (dir_sep);  /* TODO: decide if we really need dir_sep */

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

	status = camel_imap_command_extended (CAMEL_IMAP_STORE (folder->parent_store), folder,
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

/* This probably shouldn't go here...but it will for now */
static gchar *
get_header_field (gchar *header, gchar *field)
{
	gchar *part, *index, *p, *q;

	index = strstrcase (header, field);
	if (index == NULL)
		return NULL;

	p = index + strlen (field) + 1;
	for (q = p; *q; q++)
		if (*q == '\n' && (*(q + 1) != ' ' || *(q + 1) != '\t'))
			break;

	part = g_strndup (p, (gint)(q - p));

	/* it may be wrapped on multiple lines, so lets strip out \n's */
	for (p = part; *p; ) {
		if (*p == '\r' || *p == '\n')
			memmove(p, p + 1, strlen (p) - 1);
		else
			p++;
	}
	
	return part;
}

GPtrArray *
imap_get_summary (CamelFolder *folder, CamelException *ex)
{
	/* TODO: code this - loop: "FETCH <i> BODY.PEEK[HEADER]" and parse */
	GPtrArray *array = NULL;
	CamelMessageInfo *info;
	int i, num, status;
	char *result, *datestr;
	
	num = imap_get_message_count (folder, ex);
	
	array = g_ptr_array_new ();
	
	for (i = 0; i < num; i++) {
		status = camel_imap_command_extended (CAMEL_IMAP_STORE (folder->parent_store), folder,
						      &result, "FETCH %d BODY.PEEK[HEADER]", i);
		
		if (status != CAMEL_IMAP_OK) {
			CamelService *service = CAMEL_SERVICE (folder->parent_store);
		
			camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
					      "Could not get summary for %s on IMAP server %s: %s",
					      folder->full_name, service->url->host,
					      status == CAMEL_IMAP_ERR ? result :
					      "Unknown error");
			g_free (result);
			break;
		}
		
		info = g_malloc0 (sizeof (CamelMessageInfo));
		info->subject = get_header_field (result, "\nSubject:");
		info->to = get_header_field (result, "\nTo:");
		info->from = get_header_field (result, "\nFrom:");

		datestr = get_header_field (result, "\nDate:");
		info->date_sent = header_decode_date (datestr, NULL);
		g_free (datestr);
		
		info->uid = NULL;  /* FIXME: how can we get the UID? */
		g_free (result);
		
		/* still need to get flags */

		g_ptr_array_add (array, info);
	}

	return array;
}

void
imap_free_summary (CamelFolder *folder, GPtrArray *array)
{
	CamelMessageInfo *info;
	gint i, max;

	max = array->len;
	for (i = 0; i < max; i++) {
		info = g_ptr_array_index (array, i);
		g_free (info->subject);
		g_free (info->to);
		g_free (info->from);
		g_free (info->uid);
	}

	g_ptr_array_free (array, TRUE);
	
	return;
}

/* get a single message info, by uid */
static const CamelMessageInfo *
imap_summary_get_by_uid (CamelFolder *folder, const char *uid)
{
	/* TODO: code this - do a "UID FETCH <uid> BODY.PEEK[HEADER]" and parse */
	CamelMessageInfo *info = NULL;
	char *result, *datestr;
	int status;

	status = camel_imap_command_extended (CAMEL_IMAP_STORE (folder->parent_store), folder,
					      &result, "UID FETCH %s BODY.PEEK[HEADER]", uid);

	if (status != CAMEL_IMAP_OK) {
		g_free (result);
		return NULL;
	}

	info = g_malloc0 (sizeof (CamelMessageInfo));
	info->subject = get_header_field (result, "\nSubject:");
	info->to = get_header_field (result, "\nTo:");
	info->from = get_header_field (result, "\nFrom:");
	
	datestr = get_header_field (result, "\nDate:");
	info->date_sent = header_decode_date (datestr, NULL);
	g_free (datestr);
	
	info->uid = g_strdup (uid);
	g_free (result);

	/* still need to get flags */

	return info;
}

static GList *
imap_search_by_expression (CamelFolder *folder, const char *expression, CamelException *ex)
{
	return NULL;
#if 0
	/* NOTE: This is experimental code... */
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);
	char *result;
	int status;

	if (!imap_folder->has_search_capability)
		return NULL;

	status = camel_imap_command_extended (CAMEL_IMAP_STORE (folder->parent_store), folder,
					      &result, "SEARCH %s", expression);
	
	if (status != CAMEL_IMAP_OK) {
		CamelService *service = CAMEL_SERVICE (folder->parent_store);
		
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not get summary for %s on IMAP server %s: %s",
				      folder->full_name, service->url->host,
				      status == CAMEL_IMAP_ERR ? result :
				      "Unknown error");
		g_free (result);
		return NULL;
	}

	/* now to parse @result */
#endif	
}





