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
#include "string-utils.h"
#include "camel-stream-fs.h"
#include "camel-data-wrapper.h"
#include "camel-mime-message.h"
#include "camel-stream-filter.h"
#include "camel-mime-filter-from.h"
#include "camel-exception.h"

#define d(x)

static CamelFolderClass *parent_class = NULL;

static void imap_init (CamelFolder *folder, CamelStore *parent_store,
		   CamelFolder *parent_folder, const gchar *name,
		   gchar separator, CamelException *ex);

static void imap_open (CamelFolder *folder, CamelFolderOpenMode mode, CamelException *ex);
static void imap_close (CamelFolder *folder, gboolean expunge, CamelException *ex);
static gboolean imap_exists (CamelFolder *folder, CamelException *ex);
static gboolean imap_create(CamelFolder *folder, CamelException *ex);
static gboolean imap_delete (CamelFolder *folder, gboolean recurse, CamelException *ex);
static gboolean imap_delete_messages (CamelFolder *folder, CamelException *ex);
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

	camel_folder_class->exists = imap_exists;
	camel_folder_class->create = imap_create;
	camel_folder_class->delete = imap_delete;

	camel_folder_class->delete_messages = imap_delete_messages;
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

	CF_CLASS (folder)->init (folder, parent, NULL, "inbox", '/', ex);
	return folder;
}

static void           
imap_finalize (GtkObject *object)
{
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (object);

	g_free (imap_folder->folder_file_path);
	g_free (imap_folder->folder_dir_path);

	GTK_OBJECT_CLASS (parent_class)->finalize (object);
}

static void 
imap_init (CamelFolder *folder, CamelStore *parent_store, CamelFolder *parent_folder,
	   const gchar *name, gchar separator, CamelException *ex)
{
	CamelImapFolder *imap_folder = (CamelImapFolder *)folder;
	const gchar *root_dir_path;

	/* call parent method */
	parent_class->init (folder, parent_store, parent_folder,
			    name, separator, ex);
	if (camel_exception_get_id (ex))
		return;

	/* we assume that the parent init
	   method checks for the existance of @folder */
	folder->can_hold_messages = TRUE;
	folder->can_hold_folders = TRUE;
	folder->has_summary_capability = TRUE;
	folder->has_search_capability = TRUE;

	folder->permanent_flags = CAMEL_MESSAGE_ANSWERED |
		CAMEL_MESSAGE_DELETED |
		CAMEL_MESSAGE_DRAFT |
		CAMEL_MESSAGE_FLAGGED |
		CAMEL_MESSAGE_SEEN |
		CAMEL_MESSAGE_USER;

 	imap_folder->summary = NULL;
 	imap_folder->search = NULL;

	/* now set the name info */
	g_free (imap_folder->folder_file_path);
	g_free (imap_folder->folder_dir_path);
	g_free (imap_folder->index_file_path);

	root_dir_path = camel_imap_store_get_toplevel_dir (CAMEL_IMAP_STORE(folder->parent_store));

	imap_folder->folder_file_path = g_strdup_printf ("%s/%s", root_dir_path, folder->full_name);
	imap_folder->summary_file_path = g_strdup_printf ("%s/%s-ev-summary", root_dir_path, folder->full_name);
	imap_folder->folder_dir_path = g_strdup_printf ("%s/%s.sdb", root_dir_path, folder->full_name);
	imap_folder->index_file_path = g_strdup_printf ("%s/%s.ibex", root_dir_path, folder->full_name);
}

static void
imap_open (CamelFolder *folder, CamelFolderOpenMode mode, CamelException *ex)
{
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);

	/* TODO: code this - I believe we want to SELECT */
}

static void
imap_close (CamelFolder *folder, gboolean expunge, CamelException *ex)
{
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);

	/* call parent implementation */
	parent_class->close (folder, expunge, ex);

	if (expunge) {
		imap_expunge(folder, ex);
	}

	/* save index */
	if (imap_folder->index) {
		ibex_close(imap_folder->index);
		imap_folder->index = NULL;
	}
	if (imap_folder->summary) {
		camel_folder_summary_save ((CamelFolderSummary *)imap_folder->summary);
		gtk_object_unref((GtkObject *)imap_folder->summary);
		imap_folder->summary = NULL;
	}
	if (imap_folder->search) {
		gtk_object_unref((GtkObject *)imap_folder->search);
		imap_folder->search = NULL;
	}
}

static void
imap_expunge (CamelFolder *folder, CamelException *ex)
{
	CamelImapFolder *imap = (CamelImapFolder *)folder;

	if (camel_imap_summary_expunge(imap->summary) == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_FOLDER_INVALID, /* FIXME: right error code */
				      "Could not expunge: %s", strerror(errno));
	}

	/* TODO: check it actually changed */
	gtk_signal_emit_by_name((GtkObject *)folder, "folder_changed", 0);
}

static gboolean
imap_exists (CamelFolder *folder, CamelException *ex)
{
	CamelImapFolder *imap_folder;
	struct stat stat_buf;
	gint stat_error;
	gboolean exists;

	g_assert(folder != NULL);

	imap_folder = CAMEL_IMAP_FOLDER (folder);

	/* check if the imap file path is determined */
	if (!imap_folder->folder_file_path) {
		camel_exception_set (ex, 
				     CAMEL_EXCEPTION_FOLDER_INVALID,
				     "undetermined folder file path. Maybe use set_name ?");
		return FALSE;
	}

	/* check if the imap dir path is determined */
	if (!imap_folder->folder_dir_path) {
		camel_exception_set (ex, 
				     CAMEL_EXCEPTION_FOLDER_INVALID,
				     "undetermined folder directory path. Maybe use set_name ?");
		return FALSE;
	}

	/* TODO: Finish coding this. */
}

static gboolean
imap_create (CamelFolder *folder, CamelException *ex)
{
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);
	const gchar *folder_file_path, *folder_dir_path;
	gboolean folder_already_exists;

	g_assert(folder != NULL);

	/* call default implementation */
	parent_class->create (folder, ex);

	/* get the paths of what we need to create */
	folder_file_path = imap_folder->folder_file_path;
	folder_dir_path = imap_folder->folder_dir_path;
	
	if (!(folder_file_path || folder_dir_path)) {
		camel_exception_set (ex, 
				     CAMEL_EXCEPTION_FOLDER_INVALID,
				     "invalid folder path. Use set_name ?");
		return FALSE;
	}

	
	/* if the folder already exists, simply return */
	folder_already_exists = camel_folder_exists (folder, ex);
	if (camel_exception_get_id (ex))
		return FALSE;

	if (folder_already_exists)
		return TRUE;


	/* create the directory for the subfolder */
	/* TODO: actually code this */
}


static gboolean
imap_delete (CamelFolder *folder, gboolean recurse, CamelException *ex)
{
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);
	const gchar *folder_file_path, *folder_dir_path;
	gint rmdir_error = 0;
	gint unlink_error = 0;
	gboolean folder_already_exists;

	g_assert(folder != NULL);

	/* check if the folder object exists
	 * in the case where the folder does not exist, 
	 * return immediatly */
	folder_already_exists = camel_folder_exists (folder, ex);
	if (camel_exception_get_id (ex))
		return FALSE;

	if (!folder_already_exists)
		return TRUE;


	/* call default implementation.
	 * It should delete the messages in the folder
	 * and recurse the operation to subfolders */
	parent_class->delete (folder, recurse, ex);
	

	/* get the paths of what we need to be deleted */
	folder_file_path = imap_folder->folder_file_path;
	folder_dir_path = imap_folder->folder_file_path;
	
	if (!(folder_file_path || folder_dir_path)) {
		camel_exception_set (ex, 
				     CAMEL_EXCEPTION_FOLDER_INVALID,
				     "invalid folder path. Use set_name ?");
		return FALSE;
	}

	/* physically delete the directory */
	/* TODO: actually code this */

	return TRUE;
}

/* TODO: remove this */
gboolean
imap_delete_messages (CamelFolder *folder, CamelException *ex)
{
	
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);
	const gchar *folder_file_path;
	gboolean folder_already_exists;

	g_assert(folder!=NULL);
	
	/* in the case where the folder does not exist, 
	   return immediatly */
	folder_already_exists = camel_folder_exists (folder, ex);
	if (camel_exception_get_id (ex))
		return FALSE;

	if (!folder_already_exists)
		return TRUE;


	/* get the paths of the imap file we need to delete */
	folder_file_path = imap_folder->folder_file_path;
	
	if (!folder_file_path) {
		camel_exception_set (ex, 
				     CAMEL_EXCEPTION_FOLDER_INVALID,
				     "invalid folder path. Use set_name ?");
		return FALSE;
	}

		
	/* create the imap file */ 
	/* TODO: delete the messages (mark as deleted/whatever) */
	
	return TRUE;
}

static gint
imap_get_message_count (CamelFolder *folder, CamelException *ex)
{
	CamelImapFolder *imap_folder = (CamelImapFolder *)folder;

	g_assert (folder);
	g_assert (imap_folder->summary);
	
	return camel_folder_summary_count((CamelFolderSummary *)imap_folder->summary);
}

/* FIXME: this may need some tweaking for performance? */
static void
imap_append_message (CamelFolder *folder, CamelMimeMessage *message, CamelException *ex)
{
	g_warning("CamelImapFolder::imap_append_message(): This feature not supported by IMAP\n");
	
	camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
			      "This feature not supported by IMAP");
}

static GPtrArray *
imap_get_uids (CamelFolder *folder, CamelException *ex) 
{
	GPtrArray *array;
	CamelImapFolder *imap_folder = (CamelImapFolder *)folder;
	int i, count;

	count = camel_folder_summary_count((CamelFolderSummary *)imap_folder->summary);
	array = g_ptr_array_new ();
	g_ptr_array_set_size (array, count);
	for (i = 0; i < count; i++) {
		CamelImapMessageInfo *info = 
			(CamelImapMessageInfo *)camel_folder_summary_index((CamelFolderSummary *)imap_folder->summary, i);
		array->pdata[i] = g_strdup(info->info.uid);
	}
	
	return array;
}

static GPtrArray *
imap_get_subfolder_names (CamelFolder *folder, CamelException *ex)
{
	/* TODO: LSUB or LIST */

	/* No subfolders. */
	return g_ptr_array_new ();
}

static void
imap_delete_message_by_uid(CamelFolder *folder, const gchar *uid, CamelException *ex)
{
	CamelMessageInfo *info;
	CamelImapFolder *mf = (CamelImapFolder *)folder;

	info = camel_folder_summary_uid((CamelFolderSummary *)mf->summary, uid);
	if (info) {
		info->flags |=  CAMEL_MESSAGE_DELETED | CAMEL_MESSAGE_FOLDER_FLAGGED;
		camel_folder_summary_touch((CamelFolderSummary *)mf->summary);
	}
}

/* track flag changes in the summary */
static void
message_changed(CamelMimeMessage *m, int type, CamelImapFolder *mf)
{
	CamelMessageInfo *info;
	CamelFlag *flag;

	printf("Message changed: %s: %d\n", m->message_uid, type);
	switch (type) {
	case MESSAGE_FLAGS_CHANGED:
		info = camel_folder_summary_uid((CamelFolderSummary *)mf->summary, m->message_uid);
		if (info) {
			info->flags = m->flags | CAMEL_MESSAGE_FOLDER_FLAGGED;
			camel_flag_list_free(&info->user_flags);
			flag = m->user_flags;
			while (flag) {
				camel_flag_set(&info->user_flags, flag->name, TRUE);
				flag = flag->next;
			}
			camel_folder_summary_touch((CamelFolderSummary *)mf->summary);
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
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);
	CamelStream *message_stream = NULL;
	CamelMimeMessage *message = NULL;
	CamelImapMessageInfo *info;
	CamelMimeParser *parser = NULL;
	char *buffer;
	int len;

	/* get the message summary info */
	info = (CamelImapMessageInfo *)camel_folder_summary_uid((CamelFolderSummary *)imap_folder->summary, uid);

	if (info == NULL) {
		errno = ENOENT;
		goto fail;
	}

	/* if this has no content, its an error in the library */
	g_assert(info->info.content);
	g_assert(info->frompos != -1);

	/* where we read from */
	message_stream = camel_stream_fs_new_with_name (imap_folder->folder_file_path, O_RDONLY, 0);
	if (message_stream == NULL)
		goto fail;

	/* we use a parser to verify the message is correct, and in the correct position */
	parser = camel_mime_parser_new();
	camel_mime_parser_init_with_stream(parser, message_stream);
	gtk_object_unref((GtkObject *)message_stream);
	camel_mime_parser_scan_from(parser, TRUE);

	camel_mime_parser_seek(parser, info->frompos, SEEK_SET);
	if (camel_mime_parser_step(parser, &buffer, &len) != HSCAN_FROM) {
		g_warning("File appears truncated");
		goto fail;
	}

	if (camel_mime_parser_tell_start_from(parser) != info->frompos) {
		g_warning("Summary doesn't match the folder contents!  eek!");
		errno = EINVAL;
		goto fail;
	}

	message = camel_mime_message_new();
	if (camel_mime_part_construct_from_parser((CamelMimePart *)message, parser) == -1) {
		g_warning("Construction failed");
		goto fail;
	}

	/* we're constructed, finish setup and clean up */
	message->folder = folder;
	gtk_object_ref((GtkObject *)folder);
	message->message_uid = g_strdup(uid);
	message->flags = info->info.flags;
	gtk_signal_connect((GtkObject *)message, "message_changed", message_changed, folder);

	gtk_object_unref((GtkObject *)parser);

	return message;

fail:
	camel_exception_setv (ex, CAMEL_EXCEPTION_FOLDER_INVALID_UID,
			      "Cannot get message: %s",
			      g_strerror(errno));

	if (parser)
		gtk_object_unref((GtkObject *)parser);
	if (message)
		gtk_object_unref((GtkObject *)message);

	return NULL;
}

GPtrArray *
imap_get_summary (CamelFolder *folder, CamelException *ex)
{
	CamelImapFolder *imap_folder = (CamelImapFolder *)folder;

	return ((CamelFolderSummary *)imap_folder->summary)->messages;
}

void
imap_free_summary (CamelFolder *folder, GPtrArray *array)
{
	/* no-op */
}

/* get a single message info, by uid */
static const CamelMessageInfo *
imap_summary_get_by_uid(CamelFolder *f, const char *uid)
{
	CamelImapFolder *imap_folder = (CamelImapFolder *)f;

	return camel_folder_summary_uid((CamelFolderSummary *)imap_folder->summary, uid);
}

static GList *
imap_search_by_expression(CamelFolder *folder, const char *expression, CamelException *ex)
{
	CamelImapFolder *imap_folder = (CamelImapFolder *)folder;

	if (imap_folder->search == NULL) {
		imap_folder->search = camel_folder_search_new();
	}

	camel_folder_search_set_folder(imap_folder->search, folder);
	if (imap_folder->summary)
		/* FIXME: dont access summary array directly? */
		camel_folder_search_set_summary(imap_folder->search, ((CamelFolderSummary *)imap_folder->summary)->messages);
	camel_folder_search_set_body_index(imap_folder->search, imap_folder->index);

	return camel_folder_search_execute_expression(imap_folder->search, expression, ex);
}
