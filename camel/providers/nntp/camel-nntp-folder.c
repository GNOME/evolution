/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-nntp-folder.c : Abstract class for an email folder */

/* 
 * Author : Chris Toshok <toshok@helixcode.com> 
 *
 * Copyright (C) 2000 Helix Code .
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

#include "camel-folder-summary.h"
#include "camel-nntp-store.h"
#include "camel-nntp-folder.h"
#include "camel-nntp-store.h"
#include "camel-nntp-utils.h"

#include "string-utils.h"
#include "camel-stream-mem.h"
#include "camel-stream-buffer.h"
#include "camel-data-wrapper.h"
#include "camel-mime-message.h"
#include "camel-folder-summary.h"

#include "camel-exception.h"

static CamelFolderClass *parent_class=NULL;

/* Returns the class for a CamelNNTPFolder */
#define CNNTPF_CLASS(so) CAMEL_NNTP_FOLDER_CLASS (CAMEL_OBJECT_GET_CLASS(so))
#define CF_CLASS(so) CAMEL_FOLDER_CLASS (CAMEL_OBJECT_GET_CLASS(so))
#define CNNTPS_CLASS(so) CAMEL_STORE_CLASS (CAMEL_OBJECT_GET_CLASS(so))


static void 
nntp_folder_init (CamelFolder *folder, CamelStore *parent_store,
		  CamelFolder *parent_folder, const gchar *name,
		  gchar *separator, gboolean path_begins_with_sep,
		  CamelException *ex)
{
	const gchar *root_dir_path;
	CamelNNTPFolder *nntp_folder = CAMEL_NNTP_FOLDER (folder);

	/* call parent method */
	parent_class->init (folder, parent_store, parent_folder,
			    name, separator, path_begins_with_sep,
			    ex);
	if (camel_exception_is_set (ex)) return;

	/* set flags */

	if (!strcmp (name, "/")) {
		/* the root folder is the only folder that has "subfolders" */
		folder->can_hold_folders = TRUE;
		folder->can_hold_messages = FALSE;
	}
	else {
		folder->can_hold_folders = FALSE;
		folder->can_hold_messages = TRUE;
		folder->has_summary_capability = TRUE;
	}

	/* XX */
	nntp_folder->group_name = g_strdup (strrchr (name, '/') + 1);

#if 0
	/* get (or create) uid list */
	if (!(nntp_load_uid_list (nntp_folder) > 0))
		nntp_generate_uid_list (nntp_folder);
#endif

	root_dir_path = camel_nntp_store_get_toplevel_dir (CAMEL_NNTP_STORE(folder->parent_store));


	/* load the summary if we have that ability */
	if (folder->has_summary_capability) {
		nntp_folder->summary_file_path = g_strdup_printf ("%s/%s-ev-summary",
							  root_dir_path,
							  nntp_folder->group_name);

		nntp_folder->summary = camel_folder_summary_new ();
		camel_folder_summary_set_filename (nntp_folder->summary,
						   nntp_folder->summary_file_path);

		if (-1 == camel_folder_summary_load (nntp_folder->summary)) {
			/* Bad or nonexistant summary file */
			camel_nntp_get_headers (CAMEL_FOLDER( folder )->parent_store,
						nntp_folder, ex);
			if (camel_exception_get_id (ex))
				return;

			/* XXX check return value */
			camel_folder_summary_save (nntp_folder->summary);
		}
	}
		

}

static void
nntp_folder_sync (CamelFolder *folder, gboolean expunge, 
		  CamelException *ex)
{
	CamelNNTPStore *store;

	camel_folder_summary_save (CAMEL_NNTP_FOLDER(folder)->summary);

	store = CAMEL_NNTP_STORE (camel_folder_get_parent_store (folder));

	if (store->newsrc)
		camel_nntp_newsrc_write (store->newsrc);
}

static CamelFolder*
nntp_folder_get_subfolder (CamelFolder *folder,
			   const gchar *folder_name,
			   gboolean create,
			   CamelException *ex)
{
	g_assert (0);
	return NULL;
}

static gint
nntp_folder_get_message_count (CamelFolder *folder)
{
	CamelNNTPFolder *nntp_folder = CAMEL_NNTP_FOLDER(folder);

	g_assert (folder);
	g_assert (nntp_folder->summary);

        return camel_folder_summary_count(nntp_folder->summary);
}

static guint32
nntp_folder_get_message_flags (CamelFolder *folder, const char *uid)
{
	CamelNNTPFolder *nntp_folder = CAMEL_NNTP_FOLDER (folder);
	CamelMessageInfo *info = camel_folder_summary_uid (nntp_folder->summary, uid);

	return info->flags;
}

static void
nntp_folder_set_message_flags (CamelFolder *folder, const char *uid,
			       guint32 flags, guint32 set)
{
	CamelNNTPFolder *nntp_folder = CAMEL_NNTP_FOLDER (folder);
	CamelMessageInfo *info = camel_folder_summary_uid (nntp_folder->summary, uid);

	info->flags = set;

	if (set & CAMEL_MESSAGE_SEEN) {
		int article_num;
		CamelNNTPStore *nntp_store = CAMEL_NNTP_STORE (camel_folder_get_parent_store (folder));

		sscanf (uid, "%d", &article_num);

		camel_nntp_newsrc_mark_article_read (nntp_store->newsrc,
						     nntp_folder->group_name,
						     article_num);
	}

	camel_folder_summary_touch (nntp_folder->summary);
}

static CamelMimeMessage *
nntp_folder_get_message (CamelFolder *folder, const gchar *uid, CamelException *ex)
{
	CamelStream *nntp_istream;
	CamelStream *message_stream;
	CamelMimeMessage *message = NULL;
	CamelStore *parent_store;
	char *buf;
	int buf_len;
	int buf_alloc;
	int status;
	gboolean done;
	char *message_id;

	/* get the parent store */
	parent_store = camel_folder_get_parent_store (folder);

	message_id = strchr (uid, ',') + 1;
	status = camel_nntp_command (CAMEL_NNTP_STORE( parent_store ), NULL, "ARTICLE %s", message_id);

	nntp_istream = CAMEL_NNTP_STORE (parent_store)->istream;

	/* if the message_id was not found, raise an exception and return */
	if (status != CAMEL_NNTP_OK) {
		camel_exception_setv (ex, 
				     CAMEL_EXCEPTION_FOLDER_INVALID_UID,
				     "message %s not found.",
				      message_id);
		return NULL;
	}

	/* XXX ick ick ick.  read the entire message into a buffer and
	   then create a stream_mem for it. */
	buf_alloc = 2048;
	buf_len = 0;
	buf = g_malloc(buf_alloc);
	done = FALSE;

	buf[0] = 0;

	while (!done) {
		char *line = camel_stream_buffer_read_line ( CAMEL_STREAM_BUFFER ( nntp_istream ));
		int line_length;

		/* XXX check exception */

		line_length = strlen ( line );

		if (!strcmp(line, ".")) {
			done = TRUE;
			g_free (line);
		}
		else {
			if (buf_len + line_length > buf_alloc) {
				buf_alloc *= 2;
				buf = g_realloc (buf, buf_alloc);
			}
			strcat(buf, line);
			strcat(buf, "\n");
			buf_len += strlen(line) + 1;
			g_free (line);
		}
	}

	/* create a stream bound to the message */
	message_stream = camel_stream_mem_new_with_buffer(buf, buf_len);

	message = camel_mime_message_new ();
	if (camel_data_wrapper_construct_from_stream ((CamelDataWrapper *)message, message_stream) == -1) {
		camel_object_unref (CAMEL_OBJECT (message));
		camel_object_unref (CAMEL_OBJECT (message_stream));
		camel_exception_setv (ex,
				      CAMEL_EXCEPTION_FOLDER_INVALID_UID, /* XXX */
				      "Could not create message for message_id %s.", message_id);

		return NULL;
	}
	camel_object_unref (CAMEL_OBJECT (message_stream));

	/* init other fields? */
	camel_object_ref (CAMEL_OBJECT (folder));

#if 0
	gtk_signal_connect (CAMEL_OBJECT (message), "message_changed", message_changed, folder);
#endif

	return message;
}

static GPtrArray *
nntp_folder_get_uids (CamelFolder *folder)
{
	CamelNNTPFolder *nntp_folder = CAMEL_NNTP_FOLDER (folder);
	GPtrArray *out;
	CamelMessageInfo *message_info;
	int i;
	int count = camel_folder_summary_count (nntp_folder->summary);

	out = g_ptr_array_new ();
	g_ptr_array_set_size (out, count);
	
	for (i = 0; i < count; i++) {
		message_info = camel_folder_summary_index (nntp_folder->summary, i);
		out->pdata[i] = g_strdup (message_info->uid);
	}
	
	return out;
}

static GPtrArray *
nntp_folder_get_summary (CamelFolder *folder)
{
	CamelNNTPFolder *nntp_folder = CAMEL_NNTP_FOLDER (folder);

	return nntp_folder->summary->messages;
}

static GPtrArray *
nntp_folder_get_subfolder_names (CamelFolder *folder)
{
	if (!strcmp (folder->name, "/")) {
		CamelStore *store = camel_folder_get_parent_store (folder);
		
		if (CAMEL_NNTP_STORE (store)->newsrc) {
			GPtrArray *array = camel_nntp_newsrc_get_subscribed_group_names (CAMEL_NNTP_STORE (store)->newsrc);
			return array;
		}
	}
	
	return NULL;
}

static void
nntp_folder_free_subfolder_names (CamelFolder *folder, GPtrArray *subfolders)
{
	if (subfolders) {
		CamelStore *store = camel_folder_get_parent_store (folder);
		camel_nntp_newsrc_free_group_names (CAMEL_NNTP_STORE (store)->newsrc, subfolders);
	}
}

static GPtrArray*
nntp_folder_search_by_expression (CamelFolder *folder, const char *expression, CamelException *ex)
{
	g_assert (0);
	return NULL;
}

static const CamelMessageInfo*
nntp_folder_get_message_info (CamelFolder *folder, const char *uid)
{
	CamelNNTPFolder *nntp_folder = CAMEL_NNTP_FOLDER (folder);

	return camel_folder_summary_uid (nntp_folder->summary, uid);
}

static void           
nntp_folder_finalize (CamelObject *object)
{
	CamelNNTPFolder *nntp_folder = CAMEL_NNTP_FOLDER (object);

	g_free (nntp_folder->summary_file_path);
}

static void
camel_nntp_folder_class_init (CamelNNTPFolderClass *camel_nntp_folder_class)
{
	CamelFolderClass *camel_folder_class = CAMEL_FOLDER_CLASS (camel_nntp_folder_class);

	parent_class = CAMEL_FOLDER_CLASS (camel_type_get_global_classfuncs (camel_folder_get_type ()));
		
	/* virtual method definition */

	/* virtual method overload */
	camel_folder_class->init = nntp_folder_init;
	camel_folder_class->sync = nntp_folder_sync;
	camel_folder_class->get_subfolder = nntp_folder_get_subfolder;
	camel_folder_class->get_message_count = nntp_folder_get_message_count;
	camel_folder_class->set_message_flags = nntp_folder_set_message_flags;
	camel_folder_class->get_message_flags = nntp_folder_get_message_flags;
	camel_folder_class->get_message = nntp_folder_get_message;
	camel_folder_class->get_uids = nntp_folder_get_uids;
	camel_folder_class->free_uids = camel_folder_free_deep;
	camel_folder_class->get_summary = nntp_folder_get_summary;
	camel_folder_class->free_summary = camel_folder_free_nop;
	camel_folder_class->get_subfolder_names = nntp_folder_get_subfolder_names;
	camel_folder_class->free_subfolder_names = nntp_folder_free_subfolder_names;
	camel_folder_class->search_by_expression = nntp_folder_search_by_expression;
	camel_folder_class->get_message_info = nntp_folder_get_message_info;
}

CamelType
camel_nntp_folder_get_type (void)
{
	static CamelType camel_nntp_folder_type = CAMEL_INVALID_TYPE;
	
	if (camel_nntp_folder_type == CAMEL_INVALID_TYPE)	{
		camel_nntp_folder_type = camel_type_register (CAMEL_FOLDER_TYPE, "CamelNNTPFolder",
							      sizeof (CamelNNTPFolder),
							      sizeof (CamelNNTPFolderClass),
							      (CamelObjectClassInitFunc) camel_nntp_folder_class_init,
							      NULL,
							      (CamelObjectInitFunc) NULL,
							      (CamelObjectFinalizeFunc) nntp_folder_finalize);
	}
	
	return camel_nntp_folder_type;
}
