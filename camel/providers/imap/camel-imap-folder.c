/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; fill-column: 160 -*- */
/* camel-imap-folder.c: Abstract class for an email folder */

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

#include <e-util/e-util.h>

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

#define d(x) x

#define CF_CLASS(o) (CAMEL_FOLDER_CLASS (GTK_OBJECT (o)->klass))

static CamelFolderClass *parent_class = NULL;

static void imap_init (CamelFolder *folder, CamelStore *parent_store,
		       CamelFolder *parent_folder, const gchar *name,
		       gchar *separator, gboolean path_begns_with_sep,
		       CamelException *ex);

static void imap_finalize (GtkObject *object);

static void imap_sync (CamelFolder *folder, gboolean expunge, CamelException *ex);
static void imap_expunge (CamelFolder *folder, CamelException *ex);

static gint imap_get_message_count_internal (CamelFolder *folder, CamelException *ex);
static gint imap_get_message_count (CamelFolder *folder);
static gint imap_get_unread_message_count (CamelFolder *folder);

static CamelMimeMessage *imap_get_message (CamelFolder *folder, const gchar *uid, CamelException *ex);
static void imap_append_message (CamelFolder *folder, CamelMimeMessage *message, guint32 flags, CamelException *ex);
static void imap_copy_message_to (CamelFolder *source, const char *uid, CamelFolder *destination, CamelException *ex);
static void imap_move_message_to (CamelFolder *source, const char *uid, CamelFolder *destination, CamelException *ex);

static gboolean imap_parse_subfolder_line (gchar *buf, gchar *namespace, gchar **flags, gchar **sep, gchar **folder);
static GPtrArray *imap_get_subfolder_names_internal (CamelFolder *folder, CamelException *ex);
static GPtrArray *imap_get_subfolder_names (CamelFolder *folder);

static GPtrArray *imap_get_uids (CamelFolder *folder);
static GPtrArray *imap_get_summary_internal (CamelFolder *folder, CamelException *ex);
static GPtrArray *imap_get_summary (CamelFolder *folder);
static const CamelMessageInfo *imap_get_message_info (CamelFolder *folder, const char *uid);

static GPtrArray *imap_search_by_expression (CamelFolder *folder, const char *expression, CamelException *ex);

/* flag methods */
static guint32  imap_get_message_flags   (CamelFolder *folder, const char *uid);
static void     imap_set_message_flags   (CamelFolder *folder, const char *uid, guint32 flags, guint32 set);
static gboolean imap_get_message_user_flag (CamelFolder *folder, const char *uid, const char *name);
static void     imap_set_message_user_flag (CamelFolder *folder, const char *uid, const char *name,
					    gboolean value);


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
	camel_folder_class->expunge = imap_expunge;

	camel_folder_class->get_uids = imap_get_uids;
	camel_folder_class->free_uids = camel_folder_free_nop;
	camel_folder_class->get_subfolder_names = imap_get_subfolder_names;
	camel_folder_class->free_subfolder_names = camel_folder_free_nop;

	camel_folder_class->get_message_count = imap_get_message_count;
	camel_folder_class->get_unread_message_count = imap_get_unread_message_count;
	camel_folder_class->get_message = imap_get_message;
	camel_folder_class->append_message = imap_append_message;
	camel_folder_class->copy_message_to = imap_copy_message_to;
	camel_folder_class->move_message_to = imap_move_message_to;
	
	camel_folder_class->get_summary = imap_get_summary;
	camel_folder_class->get_message_info = imap_get_message_info;
	camel_folder_class->free_summary = camel_folder_free_nop;

	camel_folder_class->search_by_expression = imap_search_by_expression;

	camel_folder_class->get_message_flags = imap_get_message_flags;
	camel_folder_class->set_message_flags = imap_set_message_flags;
	camel_folder_class->get_message_user_flag = imap_get_message_user_flag;
	camel_folder_class->set_message_user_flag = imap_set_message_user_flag;

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
	folder->has_search_capability = FALSE; /* this gets set in imap_init  */

	imap_folder->summary = NULL;
	imap_folder->lsub = NULL;
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
camel_imap_folder_new (CamelStore *parent, char *folder_name, CamelException *ex)
{
	CamelFolder *folder = CAMEL_FOLDER (gtk_object_new (camel_imap_folder_get_type (), NULL));
	CamelURL *url = CAMEL_SERVICE (parent)->url;
	char *dir_sep;

	dir_sep = CAMEL_IMAP_STORE (parent)->dir_sep;
	
	CF_CLASS (folder)->init (folder, parent, NULL, folder_name, dir_sep, FALSE, ex);
 
	if (!strcmp (folder_name, url->path + 1))
		folder->can_hold_messages = FALSE;
	
	imap_get_subfolder_names_internal (folder, ex);
	imap_get_summary_internal (folder, ex);

	return folder;
}

static void
imap_summary_free (GPtrArray *summary)
{
	CamelMessageInfo *info;
	gint i, max;
	
	if (summary) {
		max = summary->len;
		for (i = 0; i < max; i++) {
			info = g_ptr_array_index (summary, i);
			g_free (info->subject);
			g_free (info->from);
			g_free (info->to);
			g_free (info->cc);
			g_free (info->uid);
			g_free (info->message_id);
			header_references_list_clear (&info->references);
			g_free (info);
			info = NULL;
		}

		g_ptr_array_free (summary, TRUE);
		summary = NULL;
	}
}

static void           
imap_finalize (GtkObject *object)
{
	/* TODO: do we need to do more cleanup here? */
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (object);
	gint max, i;
	
	imap_summary_free (imap_folder->summary);

	if (imap_folder->lsub) {
		max = imap_folder->lsub->len;

		for (i = 0; i < max; i++) {
			g_free (imap_folder->lsub->pdata[i]);
			imap_folder->lsub->pdata[i] = NULL;
		}
		
		g_ptr_array_free (imap_folder->lsub, TRUE);
	}
}

static void 
imap_init (CamelFolder *folder, CamelStore *parent_store, CamelFolder *parent_folder,
	   const gchar *name, gchar *separator, gboolean path_begins_with_sep, CamelException *ex)
{
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);
	CamelStore *store = CAMEL_STORE (parent_store);
	
	/* call parent method */
	parent_class->init (folder, parent_store, parent_folder, name, separator, path_begins_with_sep, ex);
	if (camel_exception_get_id (ex))
		return;

	/* we assume that the parent init
	   method checks for the existance of @folder */
	folder->can_hold_messages = TRUE;
	folder->can_hold_folders = TRUE;
	folder->has_summary_capability = TRUE;
	folder->has_search_capability = CAMEL_IMAP_STORE (store)->has_search_capability;
	
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
	imap_folder->summary = NULL;
}

static void
imap_sync (CamelFolder *folder, gboolean expunge, CamelException *ex)
{
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);
	gint i, max;

	if (expunge) {
		imap_expunge (folder, ex);
		return;
	}
	
	/* Set the flags on any messages that have changed this session */
	if (imap_folder->summary) {
		max = imap_folder->summary->len;
		for (i = 0; i < max; i++) {
			CamelMessageInfo *info;

			info = (CamelMessageInfo *) g_ptr_array_index (imap_folder->summary, i);
			if (info->flags & CAMEL_MESSAGE_FOLDER_FLAGGED) {
				char *flags;
				
				flags = g_strconcat (info->flags & CAMEL_MESSAGE_SEEN ? "\\Seen " : "",
						     info->flags & CAMEL_MESSAGE_DRAFT ? "\\Draft " : "",
						     info->flags & CAMEL_MESSAGE_DELETED ? "\\Deleted " : "",
						     info->flags & CAMEL_MESSAGE_ANSWERED ? "\\Answered " : "",
						     NULL);
				if (*flags) {
					gchar *result;
					gint s;
					
					*(flags + strlen (flags) - 1) = '\0';
					s = camel_imap_command_extended (CAMEL_IMAP_STORE (folder->parent_store),
									 folder, &result,
									 "UID STORE %s FLAGS.SILENT (%s)",
									 info->uid, flags);
					
					if (s != CAMEL_IMAP_OK) {
						CamelService *service = CAMEL_SERVICE (folder->parent_store);
						camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
								      "Could not set flags on message %s on IMAP "
								      "server %s: %s.", info->uid,
								      service->url->host,
								      s != CAMEL_IMAP_FAIL && result ? result :
								      "Unknown error");
						g_free (result);
						return;
					}
					
					g_free (result);
				}
				g_free (flags);
			}
		}
	}
}

static void
imap_expunge (CamelFolder *folder, CamelException *ex)
{
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);
	gchar *result;
	gint status;

	g_return_if_fail (folder != NULL);

	imap_sync (folder, FALSE, ex);
	
	status = camel_imap_command_extended (CAMEL_IMAP_STORE (folder->parent_store), folder,
					      &result, "EXPUNGE");
	
	if (status != CAMEL_IMAP_OK) {
		CamelService *service = CAMEL_SERVICE (folder->parent_store);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not EXPUNGE from IMAP server %s: %s.",
				      service->url->host,
				      status != CAMEL_IMAP_FAIL && result ? result :
				      "Unknown error");
		g_free (result);
		return;
	}
	
	g_free (result);

	/* FIXME: maybe remove the appropriate messages from the summary
	   so we don't need to refetch the entire summary? */
	imap_summary_free (imap_folder->summary);
	imap_folder->summary = NULL;

	camel_imap_folder_changed (folder, -1, ex);
}

static gint
imap_get_message_count_internal (CamelFolder *folder, CamelException *ex)
{
	CamelStore *store = CAMEL_STORE (folder->parent_store);
	CamelURL *url = CAMEL_SERVICE (store)->url;
	gchar *result, *msg_count, *folder_path, *dir_sep;
	gint status, count = 0;

	g_return_val_if_fail (folder != NULL, 0);
	g_return_val_if_fail (folder->can_hold_messages, 0);

	dir_sep = CAMEL_IMAP_STORE (folder->parent_store)->dir_sep;
	
	if (url && url->path && *(url->path + 1) && strcmp (folder->full_name, "INBOX"))
		folder_path = g_strdup_printf ("%s%s%s", url->path + 1, dir_sep, folder->full_name);
	else
		folder_path = g_strdup (folder->full_name);
	
	status = camel_imap_command_extended (CAMEL_IMAP_STORE (folder->parent_store), folder,
					      &result, "STATUS %s (MESSAGES)", folder_path);

	if (status != CAMEL_IMAP_OK) {
		CamelService *service = CAMEL_SERVICE (folder->parent_store);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not get message count for %s from IMAP "
				      "server %s: %s.", folder_path, service->url->host,
				      status != CAMEL_IMAP_FAIL && result ? result :
				      "Unknown error");
		g_free (result);
		g_free (folder_path);
		return 0;
	}
	g_free (folder_path);

	/* parse out the message count - should come in the form: "* STATUS <folder> (MESSAGES <count>)\r\n" */
	if (result && *result == '*') {
		if ((msg_count = strstr (result, "MESSAGES")) != NULL) {
			msg_count += strlen ("MESSAGES") + 1;

			for ( ; *msg_count == ' '; msg_count++);
			
			/* we should now be pointing to the message count */
			count = atoi (msg_count);
		}
	}
	g_free (result);

	return count;
}

static gint
imap_get_message_count (CamelFolder *folder)
{
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);

	if (imap_folder->summary)
		return imap_folder->summary->len;
	else
		return 0;
}

static gint
imap_get_unread_message_count (CamelFolder *folder)
{
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);
	CamelMessageInfo *info;
	GPtrArray *infolist;
	gint i, count = 0;

	g_return_val_if_fail (folder != NULL, 0);

	/* If we don't have a message count, return */
	if (!imap_folder->summary)
		return 0;

	infolist = imap_get_summary (folder);
	
	for (i = 0; i < infolist->len; i++) {
		info = (CamelMessageInfo *) g_ptr_array_index (infolist, i);
		if (!(info->flags & CAMEL_MESSAGE_SEEN))
			count++;
	}

	return count;
}

static void
imap_append_message (CamelFolder *folder, CamelMimeMessage *message, guint32 flags, CamelException *ex)
{
	CamelStore *store = CAMEL_STORE (folder->parent_store);
	CamelURL *url = CAMEL_SERVICE (store)->url;
	CamelStreamMem *mem;
	gchar *result, *folder_path, *dir_sep, *flagstr = NULL;
	gint status;

	g_return_if_fail (folder != NULL);
	g_return_if_fail (message != NULL);

	/* write the message to a CamelStreamMem so we can get it's size */
	mem = CAMEL_STREAM_MEM (camel_stream_mem_new ());
	if (camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (message), CAMEL_STREAM (mem)) == -1) {
		CamelService *service = CAMEL_SERVICE (folder->parent_store);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "Could not APPEND message to IMAP server %s: %s.",
				      service->url->host,
				      g_strerror (errno));

	        return;
	}

	mem->buffer = g_byte_array_append (mem->buffer, g_strdup ("\r\n"), 3);

	dir_sep = CAMEL_IMAP_STORE (folder->parent_store)->dir_sep;
	
	if (url && url->path && *(url->path + 1) && strcmp (folder->full_name, "INBOX"))
		folder_path = g_strdup_printf ("%s%s%s", url->path + 1, dir_sep, folder->full_name);
	else
		folder_path = g_strdup (folder->full_name);

	/* create flag string param */
	if (flags) {
		flagstr = g_strconcat (" (", flags & CAMEL_MESSAGE_SEEN ? "\\Seen " : "",
				       flags & CAMEL_MESSAGE_DRAFT ? "\\Draft " : "",
				       flags & CAMEL_MESSAGE_DELETED ? "\\Answered " : "",
				       NULL);
		if (flagstr)
			*(flagstr + strlen (flagstr) - 1) = ')';
	}
	
	/* FIXME: len isn't really correct I don't think, we need to crlf/dot filter */
	status = camel_imap_command_extended (CAMEL_IMAP_STORE (folder->parent_store),
					      folder, &result, "APPEND %s%s {%d}\r\n%s",
					      folder_path, flagstr ? flagstr : "",
					      mem->buffer->len - 1, mem->buffer->data);

	if (status != CAMEL_IMAP_OK) {
		CamelService *service = CAMEL_SERVICE (folder->parent_store);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not APPEND message to IMAP server %s: %s.",
				      service->url->host,
				      status != CAMEL_IMAP_FAIL && result ? result :
				      "Unknown error");
		g_free (result);
		g_free (folder_path);
		return;
	}

	/* FIXME: we should close/free the mem stream */

	g_free (result);
	g_free (folder_path);

	camel_imap_folder_changed (folder, 1, ex);
}

static void
imap_copy_message_to (CamelFolder *source, const char *uid, CamelFolder *destination, CamelException *ex)
{
	CamelStore *store = CAMEL_STORE (source->parent_store);
	CamelURL *url = CAMEL_SERVICE (store)->url;
	char *result, *folder_path, *dir_sep;
	int status;

	dir_sep = CAMEL_IMAP_STORE (source->parent_folder)->dir_sep;
	
	if (url && url->path && *(url->path + 1) && strcmp (destination->full_name, "INBOX"))
		folder_path = g_strdup_printf ("%s%s%s", url->path + 1, dir_sep, destination->full_name);
	else
		folder_path = g_strdup (destination->full_name);
	
	status = camel_imap_command_extended (CAMEL_IMAP_STORE (store), source, &result,
					      "UID COPY %s %s", uid, folder_path);
	
	if (status != CAMEL_IMAP_OK) {
		CamelService *service = CAMEL_SERVICE (store);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not COPY message %s to %s on IMAP server %s: %s.",
				      uid, folder_path, service->url->host,
				      status != CAMEL_IMAP_FAIL && result ? result :
				      "Unknown error");
		g_free (result);
		g_free (folder_path);
		return;
	}

	g_free (result);
	g_free (folder_path);

	camel_imap_folder_changed (destination, 1, ex);
}

/* FIXME: Duplication of code! */
static void
imap_move_message_to (CamelFolder *source, const char *uid, CamelFolder *destination, CamelException *ex)
{
	CamelStore *store = CAMEL_STORE (source->parent_store);
	CamelURL *url = CAMEL_SERVICE (store)->url;
	CamelMessageInfo *info;
	char *result, *folder_path, *dir_sep;
	int status;

	dir_sep = CAMEL_IMAP_STORE (source->parent_store)->dir_sep;
	
	if (url && url->path && *(url->path + 1) && strcmp (destination->full_name, "INBOX"))
		folder_path = g_strdup_printf ("%s%s%s", url->path + 1, dir_sep, destination->full_name);
	else
		folder_path = g_strdup (destination->full_name);
	
	status = camel_imap_command_extended (CAMEL_IMAP_STORE (store), source, &result,
					      "UID COPY %s %s", uid, folder_path);

	if (status != CAMEL_IMAP_OK) {
		CamelService *service = CAMEL_SERVICE (store);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not COPY message %s to %s on IMAP server %s: %s.",
				      uid, folder_path, service->url->host,
				      status != CAMEL_IMAP_FAIL && result ? result :
				      "Unknown error");
		g_free (result);
		g_free (folder_path);
		return;
	}

	g_free (result);
	g_free (folder_path);
	
	if (!(info = (CamelMessageInfo *)imap_get_message_info (source, uid))) {
		CamelService *service = CAMEL_SERVICE (store);
		
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not set flags for message %s on IMAP server %s: %s",
				      uid, service->url->host, "Unknown error");
		return;
	}

	imap_set_message_flags (source, uid, CAMEL_MESSAGE_DELETED, ~(info->flags));

	camel_imap_folder_changed (destination, 1, ex);
}

static GPtrArray *
imap_get_uids (CamelFolder *folder) 
{
	CamelMessageInfo *info;
	GPtrArray *array, *infolist;
	gint i, count;
	
	infolist = imap_get_summary (folder);
	count = infolist->len;
	
	array = g_ptr_array_new ();
	g_ptr_array_set_size (array, count);
	
	for (i = 0; i < count; i++) {
		info = (CamelMessageInfo *) g_ptr_array_index (infolist, i);
		array->pdata[i] = g_strdup (info->uid);
	}
	
	return array;
}

static gboolean
imap_parse_subfolder_line (gchar *buf, gchar *namespace, gchar **flags, gchar **sep, gchar **folder)
{
	gchar *ptr, *eptr, *f;
	
	*flags = NULL;
	*sep = NULL;
	*folder = NULL;
	
	if (g_strncasecmp (buf, "* LIST", 6))
		return FALSE;
	
	ptr = strstr (buf + 6, "(");
	if (!ptr)
		return FALSE;
	
	ptr++;
	eptr = strstr (ptr, ")");
	if (!eptr)
		return FALSE;
	
	*flags = g_strndup (ptr, (gint)(eptr - ptr));
	
	ptr = strstr (eptr, "\"");
	if (!ptr)
		return FALSE;
	
	ptr++;
	eptr = strstr (ptr, "\"");
	if (!eptr)
		return FALSE;
	
	*sep = g_strndup (ptr, (gint)(eptr - ptr));
	
	ptr = eptr + 1;
	*folder = g_strdup (ptr);
	g_strstrip (*folder);
	
	/* chop out the folder prefix */
	if (*namespace && !strncmp (*folder, namespace, strlen (namespace))) {
		f = *folder + strlen (namespace) + strlen (*sep);
		memmove (*folder, f, strlen (f) + 1);
	}
	
	string_unquote (*folder);  /* unquote the mailbox if it's quoted */
	
	return TRUE;
}

static GPtrArray *
imap_get_subfolder_names_internal (CamelFolder *folder, CamelException *ex)
{
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);
	CamelStore *store = CAMEL_STORE (folder->parent_store);
	CamelURL *url = CAMEL_SERVICE (store)->url;
	GPtrArray *listing;
	gint status;
	gchar *result, *namespace, *dir_sep;

	g_return_val_if_fail (folder != NULL, g_ptr_array_new ());
	
	dir_sep = CAMEL_IMAP_STORE (folder->parent_store)->dir_sep;
	
	if (url && url->path) {
		if (!strcmp (folder->full_name, url->path + 1))
			namespace = g_strdup (url->path + 1);
		else if (!strcmp (folder->full_name, "INBOX"))
			namespace = g_strdup (url->path + 1); /* FIXME: erm...not sure */
		else
			namespace = g_strdup_printf ("%s%s%s", url->path + 1, dir_sep, folder->full_name);
	} else {
		namespace = g_strdup (folder->full_name);
	}
	
	status = camel_imap_command_extended (CAMEL_IMAP_STORE (folder->parent_store), NULL,
					      &result, "LIST \"\" \"%s%s*\"", namespace,
					      *namespace ? dir_sep : "");
	
	if (status != CAMEL_IMAP_OK) {
		CamelService *service = CAMEL_SERVICE (folder->parent_store);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not get subfolder listing from IMAP "
				      "server %s: %s.", service->url->host,
				      status != CAMEL_IMAP_FAIL && result ? result :
				      "Unknown error");
		g_free (result);
		g_free (namespace);

		imap_folder->lsub = g_ptr_array_new ();
		return imap_folder->lsub;
	}
	
	/* parse out the subfolders */
	listing = g_ptr_array_new ();
	if (result) {
		char *ptr = result;
		
		while (ptr && *ptr == '*') {
			gchar *flags, *sep, *folder, *buf, *end;
			gboolean ret;
			
			for (end = ptr; *end && *end != '\n'; end++);
			buf = g_strndup (ptr, (gint)(end - ptr));
			ptr = end;
			
			ret = imap_parse_subfolder_line (buf, namespace, &flags, &sep, &folder);
			g_free (buf);
			
			if (!ret /*|| (flags && strstr (flags, "NoSelect"))*/) {
				g_free (flags);
				g_free (sep);
				g_free (folder);
				
				if (*ptr == '\n')
					ptr++;
				
				continue;
			}
			g_free (flags);
			
			d(fprintf (stderr, "adding folder: %s\n", folder));
			
			g_ptr_array_add (listing, folder);
			
			g_free (sep);
			
			if (*ptr == '\n')
				ptr++;
		}
	}
	g_free (result);
	g_free (namespace);

	imap_folder->lsub = listing;
	
	return listing;
}

static GPtrArray *
imap_get_subfolder_names (CamelFolder *folder)
{
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);
	
	return imap_folder->lsub;
}

static CamelMimeMessage *
imap_get_message (CamelFolder *folder, const gchar *uid, CamelException *ex)
{
	CamelStream *msgstream = NULL;
	/*CamelStreamFilter *f_stream;*/
	/*CamelMimeFilter *filter;*/
	CamelMimeMessage *msg = NULL;
	/*CamelMimePart *part;*/
	gchar *result, *header = NULL, *body = NULL, *mesg = NULL, *p = NULL, *q = NULL;
	int status = 0, part_len = 0;
	
	status = camel_imap_command_extended (CAMEL_IMAP_STORE (folder->parent_store), folder,
					      &result, "UID FETCH %s BODY.PEEK[HEADER]", uid);

	if (!result || status != CAMEL_IMAP_OK) {
		CamelService *service = CAMEL_SERVICE (folder->parent_store);
		
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not fetch message %s on IMAP server %s: %s",
				      uid, service->url->host,
				      status != CAMEL_IMAP_FAIL && result ? result :
				      "Unknown error");
		g_free (result);
		return camel_mime_message_new ();
	}
	
	for (p = result; *p && *p != '{' && *p != '\n'; p++);
	if (*p != '{') {
		g_free (result);
		return camel_mime_message_new ();
	}
	
	part_len = atoi (p + 1);
	for ( ; *p && *p != '\n'; p++);
	if (*p != '\n') {
		g_free (result);
		return camel_mime_message_new ();
	}
	
	/* calculate the new part-length */
	for (q = p; *q && (q - p) <= part_len; q++) {
		if (*q == '\n')
			part_len--;
	}
	/* FIXME: This is a hack for IMAP daemons that send us a UID at the end of each FETCH */
	for (q--, part_len--; q > p && *(q-1) != '\n'; q--, part_len--);

	header = g_strndup (p, part_len + 1);
	
	g_free (result);
	d(fprintf (stderr, "*** We got the header ***\n"));
	
	status = camel_imap_command_extended (CAMEL_IMAP_STORE (folder->parent_store), folder,
					      &result, "UID FETCH %s BODY[TEXT]", uid);
	
	if (!result || status != CAMEL_IMAP_OK) {
		CamelService *service = CAMEL_SERVICE (folder->parent_store);
		
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not fetch message %s on IMAP server %s: %s",
				      uid, service->url->host,
				      status != CAMEL_IMAP_FAIL && result ? result :
				      "Unknown error");
		g_free (result);
		g_free (header);
		return camel_mime_message_new ();
	}
	
	for (p = result; *p && *p != '{' && *p != '\n'; p++);
	if (*p != '{') {
		g_free (result);
		g_free (header);
		return camel_mime_message_new ();
	}
	
        part_len = atoi (p + 1);
	for ( ; *p && *p != '\n'; p++);
	if (*p != '\n') {
		g_free (result);
		g_free (header);
		return camel_mime_message_new ();
	}
	
	/* calculate the new part-length */
	for (q = p; *q && (q - p) <= part_len; q++) {
		if (*q == '\n')
			part_len--;
	}
	/* FIXME: This is a hack for IMAP daemons that send us a UID at the end of each FETCH */
	for ( ; q > p && *(q-1) != '\n'; q--, part_len--);
	
	body = g_strndup (p, part_len + 1);
	
	g_free (result);
	d(fprintf (stderr, "*** We got the body ***\n"));
	
	mesg = g_strdup_printf ("%s\n%s", header, body);
	g_free (header);
	g_free (body);
	d(fprintf (stderr, "*** We got the mesg ***\n"));
	
	d(fprintf (stderr, "Message:\n%s\n", mesg));
	
	msgstream = camel_stream_mem_new_with_buffer (mesg, strlen (mesg) + 1);
#if 0
	f_stream = camel_stream_filter_new_with_stream (msgstream);
	filter = camel_mime_filter_crlf_new (CAMEL_MIME_FILTER_CRLF_DECODE, CAMEL_MIME_FILTER_CRLF_MODE_CRLF_DOTS);
	id = camel_stream_filter_add (f_stream, CAMEL_MIME_FILTER (filter));
#endif	
	msg = camel_mime_message_new ();
	d(fprintf (stderr, "*** We created the camel_mime_message ***\n"));
	
	camel_data_wrapper_construct_from_stream (CAMEL_DATA_WRAPPER (msg), msgstream);
#if 0	
	camel_stream_filter_remove (f_stream, id);
	camel_stream_close (CAMEL_STREAM (f_stream));
#endif
	gtk_object_unref (GTK_OBJECT (msgstream));
	/*gtk_object_unref (GTK_OBJECT (f_stream));*/
	
	d(fprintf (stderr, "*** We're returning... ***\n"));
	
	return msg;
	
#if 0
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
	camel_stream_write_to_stream (CAMEL_STREAM (imap_stream), msgstream);
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
#endif
}

/* This probably shouldn't go here...but it will for now */
static gchar *
get_header_field (gchar *header, gchar *field)
{
	gchar *part, *index, *p, *q;

	index = e_strstrcase (header, field);
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
			memmove (p, p + 1, strlen (p) - 1);
		else
			p++;
	}
	
	return part;
}

static char *header_fields[] = { "subject", "from", "to", "cc", "date",
				 "received", "message-id", "references",
				 "in-reply-to", "" };

static GPtrArray *
imap_get_summary_internal (CamelFolder *folder, CamelException *ex)
{
	/* This ALWAYS updates the summary except on fail */
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);
	GPtrArray *summary = NULL, *headers = NULL;
	gint num, i, j, status = 0;
	char *result, *q, *node;
	const char *received;
	struct _header_raw *h, *tail = NULL;
	
	num = imap_get_message_count_internal (folder, ex);

	/* sync any previously set/changed message flags */
	imap_sync (folder, FALSE, ex);

	switch (num) {
	case 0:
		/* clean up any previous summary data */
		imap_summary_free (imap_folder->summary);
		
		imap_folder->summary = g_ptr_array_new ();
		
		return imap_folder->summary;
	case 1:
		status = camel_imap_command_extended (CAMEL_IMAP_STORE (folder->parent_store), folder,
						      &result, "FETCH 1 (UID FLAGS BODY[HEADER.FIELDS "
						      "(SUBJECT FROM TO CC DATE MESSAGE-ID "
						      "REFERENCES IN-REPLY-TO)])");
		break;
	default:
		status = camel_imap_command_extended (CAMEL_IMAP_STORE (folder->parent_store), folder,
						      &result, "FETCH 1:%d (UID FLAGS BODY[HEADER.FIELDS "
						      "(SUBJECT FROM TO CC DATE MESSAGE-ID "
						      "REFERENCES IN-REPLY-TO)])", num);
	}
	
	if (status != CAMEL_IMAP_OK) {
		CamelService *service = CAMEL_SERVICE (folder->parent_store);
		
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not get summary for %s on IMAP server %s: %s",
				      folder->full_name, service->url->host,
				      status != CAMEL_IMAP_FAIL && result ? result :
				      "Unknown error");
		g_free (result);

		g_ptr_array_free (summary, TRUE);
		
		return imap_folder->summary;
	}

	/* initialize our new summary-to-be */
	summary = g_ptr_array_new ();
	
	/* create our array of headers from the server response */
	headers = g_ptr_array_new ();
	node = result;
	for (i = 1; node; i++) {
		char *end;

		if ((end = strstr (node + 2, "\n*"))) {
			g_ptr_array_add (headers, g_strndup (node, (gint)(end - node)));
		} else {
			g_ptr_array_add (headers, g_strdup (node));
		}
		node = end;
	}
	if (i < num) {
		d(fprintf (stderr, "IMAP server didn't respond with as many headers as we expected...\n"));
		/* should we error?? */
	}

	g_free (result);
	result = NULL;
	
	for (i = 0; i < headers->len; i++) {
		CamelMessageInfo *info;
		char *uid, *flags, *header;

		info = g_malloc0 (sizeof (CamelMessageInfo));

		/* lets grab the UID... */
		if (!(uid = strstr (headers->pdata[i], "(UID "))) {
			d(fprintf (stderr, "Cannot get a uid for %d\n\n%s\n\n", i+1, (char *) headers->pdata[i]));
			g_free (info);
			break;
		}
		
		for (uid += 5; *uid && (*uid < '0' || *uid > '9'); uid++); /* advance to <uid> */
		for (q = uid; *q && *q >= '0' && *q <= '9'; q++); /* find the end of the <uid> */
		info->uid = g_strndup (uid, (gint)(q - uid));
		d(fprintf (stderr, "*** info->uid = %s\n", info->uid));

		/* now lets grab the FLAGS */
		if (!(flags = strstr (q, "FLAGS "))) {
			d(fprintf (stderr, "We didn't seem to get any flags for %d...\n", i));
			g_free (info->uid);
			g_free (info);
			break;
		}

		for (flags += 6; *flags && *flags != '('; flags++); /* advance to <flags> */
		for (q = flags; *q && *q != ')'; q++);         /* find the end of <flags> */
		flags = g_strndup (flags, (gint)(q - flags + 1));
		d(fprintf (stderr, "*** info->flags = %s\n", flags));

		/* now we gotta parse for the flags */
		info->flags = 0;
		if (strstr (flags, "\\Seen"))
			info->flags |= CAMEL_MESSAGE_SEEN;
		if (strstr (flags, "\\Answered"))
			info->flags |= CAMEL_MESSAGE_ANSWERED;
		if (strstr (flags, "\\Flagged"))
			info->flags |= CAMEL_MESSAGE_FLAGGED;
		if (strstr (flags, "\\Deleted"))
			info->flags |= CAMEL_MESSAGE_DELETED;
		if (strstr (flags, "\\Draft"))
			info->flags |= CAMEL_MESSAGE_DRAFT;
		g_free (flags);
		flags = NULL;
		
		/* construct the header list */
		/* fast-forward to beginning of header info... */
		for (header = q; *header && *header != '\n'; header++);
		h = NULL;
		for (j = 0; *header_fields[j]; j++) {
			struct _header_raw *raw;
			char *field, *value;

			field = g_strdup_printf ("\n%s:", header_fields[j]);
			value = get_header_field (header, field);
			g_free (field);
			if (!value)
				continue;
			
			raw = g_malloc0 (sizeof (struct _header_raw));
			raw->next = NULL;
			raw->name = g_strdup (header_fields[j]);
			raw->value = value;
			raw->offset = -1;

			if (!h) {
				h = raw;
				tail = h;
			} else {
				tail->next = raw;
				tail = raw;
			}
		}

		/* construct the CamelMessageInfo */
		info->subject = camel_summary_format_string (h, "subject");
		info->from = camel_summary_format_address (h, "from");
		info->to = camel_summary_format_address (h, "to");
		info->cc = camel_summary_format_address (h, "cc");
		info->user_flags = NULL;
		info->date_sent = header_decode_date (header_raw_find (&h, "date", NULL), NULL);
		received = header_raw_find (&h, "received", NULL);
		if (received)
			received = strrchr (received, ';');
		if (received)
			info->date_received = header_decode_date (received + 1, NULL);
		else
			info->date_received = 0;
		info->message_id = header_msgid_decode (header_raw_find (&h, "message-id", NULL));
		/* if we have a references, use that, otherwise, see if we have an in-reply-to
		   header, with parsable content, otherwise *shrug* */
		info->references = header_references_decode (header_raw_find (&h, "references", NULL));
		if (info->references == NULL)
			info->references = header_references_decode (header_raw_find (&h, "in-reply-to", NULL));

		while (h->next) {
			struct _header_raw *next = h->next;
			
			g_free (h->name);
			g_free (h->value);
			g_free (h);
			h = next;
		}

		g_ptr_array_add (summary, info);
	}

	g_ptr_array_free (headers, TRUE);

	/* clean up any previous summary data */
	imap_summary_free (imap_folder->summary);
	
	imap_folder->summary = summary;
	
	return imap_folder->summary;
}

static GPtrArray *
imap_get_summary (CamelFolder *folder)
{
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);

	return imap_folder->summary;
}

/* get a single message info from the server */
static CamelMessageInfo *
imap_get_message_info_internal (CamelFolder *folder, guint id)
{
	CamelMessageInfo *info = NULL;
	struct _header_raw *h, *tail = NULL;
	const char *received;
	char *result, *uid, *flags, *header, *q;
	int j, status;

	/* we don't have a cached copy, so fetch it */
	status = camel_imap_command_extended (CAMEL_IMAP_STORE (folder->parent_store), folder,
					      &result, "FETCH %d (UID FLAGS BODY[HEADER.FIELDS "
					      "(SUBJECT FROM TO CC DATE MESSAGE-ID "
					      "REFERENCES IN-REPLY-TO)])", id);

	if (status != CAMEL_IMAP_OK) {
		g_free (result);
		return NULL;
	}
	
	/* lets grab the UID... */
	if (!(uid = e_strstrcase (result, "(UID "))) {
		d(fprintf (stderr, "Cannot get a uid for %d\n\n%s\n\n", id, result));
		g_free (result);
		return NULL;
	}
		
	for (uid += 5; *uid && (*uid < '0' || *uid > '9'); uid++); /* advance to <uid> */
	for (q = uid; *q && *q >= '0' && *q <= '9'; q++); /* find the end of the <uid> */
	uid = g_strndup (uid, (gint)(q - uid));

	info = g_malloc0 (sizeof (CamelMessageInfo));
	info->uid = uid;
	d(fprintf (stderr, "*** info->uid = %s\n", info->uid));
	
	/* now lets grab the FLAGS */
	if (!(flags = strstr (q, "FLAGS "))) {
		d(fprintf (stderr, "We didn't seem to get any flags for %s...\n", uid));
		g_free (info->uid);
		g_free (info);
		g_free (result);
		return NULL;
	}
	
	for (flags += 6; *flags && *flags != '('; flags++); /* advance to <flags> */
	for (q = flags; *q && *q != ')'; q++);              /* find the end of <flags> */
	flags = g_strndup (flags, (gint)(q - flags + 1));
	d(fprintf (stderr, "*** info->flags = %s\n", flags));
	
	/* now we gotta parse for the flags */
	info->flags = 0;
	if (strstr (flags, "\\Seen"))
		info->flags |= CAMEL_MESSAGE_SEEN;
	if (strstr (flags, "\\Answered"))
		info->flags |= CAMEL_MESSAGE_ANSWERED;
	if (strstr (flags, "\\Flagged"))
		info->flags |= CAMEL_MESSAGE_FLAGGED;
	if (strstr (flags, "\\Deleted"))
		info->flags |= CAMEL_MESSAGE_DELETED;
	if (strstr (flags, "\\Draft"))
		info->flags |= CAMEL_MESSAGE_DRAFT;
	g_free (flags);
	flags = NULL;
	
	/* construct the header list */
	/* fast-forward to beginning of header info... */
	for (header = q; *header && *header != '\n'; header++);
	h = NULL;
	for (j = 0; *header_fields[j]; j++) {
		struct _header_raw *raw;
		char *field, *value;
		
		field = g_strdup_printf ("\n%s:", header_fields[j]);
		value = get_header_field (header, field);
		g_free (field);
		if (!value)
			continue;
		
		raw = g_malloc0 (sizeof (struct _header_raw));
		raw->next = NULL;
		raw->name = g_strdup (header_fields[j]);
		raw->value = value;
		raw->offset = -1;
		
		if (!h) {
			h = raw;
			tail = h;
		} else {
			tail->next = raw;
			tail = raw;
		}
	}
	
	/* construct the CamelMessageInfo */
	info->subject = camel_summary_format_string (h, "subject");
	info->from = camel_summary_format_address (h, "from");
	info->to = camel_summary_format_address (h, "to");
	info->cc = camel_summary_format_address (h, "cc");
	info->user_flags = NULL;
	info->date_sent = header_decode_date (header_raw_find (&h, "date", NULL), NULL);
	received = header_raw_find (&h, "received", NULL);
	if (received)
		received = strrchr (received, ';');
	if (received)
		info->date_received = header_decode_date (received + 1, NULL);
	else
		info->date_received = 0;
	info->message_id = header_msgid_decode (header_raw_find (&h, "message-id", NULL));
	/* if we have a references, use that, otherwise, see if we have an in-reply-to
	   header, with parsable content, otherwise *shrug* */
	info->references = header_references_decode (header_raw_find (&h, "references", NULL));
	if (info->references == NULL)
		info->references = header_references_decode (header_raw_find (&h, "in-reply-to", NULL));

	while (h->next) {
		struct _header_raw *next = h->next;
		
		g_free (h->name);
		g_free (h->value);
		g_free (h);
		h = next;
	}

	g_free (result);
	
	return info;
}

/* get a single message info, by uid */
static const CamelMessageInfo *
imap_get_message_info (CamelFolder *folder, const char *uid)
{
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);
	CamelMessageInfo *info = NULL;

	g_return_val_if_fail (*uid != '\0', NULL);

	if (imap_folder->summary) {
		int max, i;

		/* FIXME: use a hash table like the mbox provider does */
		max = imap_folder->summary->len;
		for (i = 0; i < max; i++) {
			info = g_ptr_array_index (imap_folder->summary, i);
			if (!strcmp (info->uid, uid))
				return info;
		}
	}

	return NULL;
}

static GPtrArray *
imap_search_by_expression (CamelFolder *folder, const char *expression, CamelException *ex)
{
	return g_ptr_array_new ();
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
				      status != CAMEL_IMAP_FAIL && result ? result :
				      "Unknown error");
		g_free (result);
		return NULL;
	}

	/* now to parse @result */
#endif	
}

static guint32
imap_get_message_flags (CamelFolder *folder, const char *uid)
{
	const CamelMessageInfo *info;

	info = imap_get_message_info (folder, uid);
	g_return_val_if_fail (info != NULL, 0);

	return info->flags;
}

static void
imap_set_message_flags (CamelFolder *folder, const char *uid, guint32 flags, guint32 set)
{
	CamelMessageInfo *info;

	info = (CamelMessageInfo*)imap_get_message_info (folder, uid);
	g_return_if_fail (info != NULL);

	info->flags = (info->flags & ~flags) | (set & flags) | CAMEL_MESSAGE_FOLDER_FLAGGED;

	gtk_signal_emit_by_name (GTK_OBJECT (folder), "message_changed", uid);
}

static gboolean
imap_get_message_user_flag (CamelFolder *folder, const char *uid, const char *name)
{
	return FALSE;
}

static void
imap_set_message_user_flag (CamelFolder *folder, const char *uid, const char *name, gboolean value)
{
	gtk_signal_emit_by_name (GTK_OBJECT (folder), "message_changed", uid);
}

void
camel_imap_folder_changed (CamelFolder *folder, gint recent, CamelException *ex)
{
	d(fprintf (stderr, "camel_imap_folder_changed: recent = %d\n", recent));
	
	if (recent > 0) {
		CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);
		CamelMessageInfo *info;
		gint i, last;

		if (!imap_folder->summary)
			imap_folder->summary = g_ptr_array_new ();

		last = imap_folder->summary->len + 1;

		for (i = last; i < last + recent; i++) {
			info = imap_get_message_info_internal (folder, i);
			if (info) {
				g_ptr_array_add (imap_folder->summary, info);
			} else {
				/* our hack failed so now we need to do it the old fashioned way */
				imap_get_summary_internal (folder, ex);
				break;
			}
		}
	} else {
		/* We either expunged messages or something... */
		imap_get_summary_internal (folder, ex);
	}
	
	gtk_signal_emit_by_name (GTK_OBJECT (folder), "folder_changed", 0);
}
