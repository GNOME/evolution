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
#define CNNTPF_CLASS(so) CAMEL_NNTP_FOLDER_CLASS (GTK_OBJECT(so)->klass)
#define CF_CLASS(so) CAMEL_FOLDER_CLASS (GTK_OBJECT(so)->klass)
#define CNNTPS_CLASS(so) CAMEL_STORE_CLASS (GTK_OBJECT(so)->klass)


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
	if (camel_exception_get_id (ex)) return;

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
	camel_folder_summary_save (CAMEL_NNTP_FOLDER(folder)->summary);

	/* XXX

	   loop through the messages in the summary and store out the .newsrc,
	   using something similar to this bit snipped from _set_message_flags:


	if (set & CAMEL_MESSAGE_SEEN) {
		CamelNNTPStore *store;
		CamelException *ex;
		
		ex = camel_exception_new ();
		store = CAMEL_NNTP_STORE (camel_folder_get_parent_store (folder, ex));
		camel_exception_free (ex);
		
		camel_nntp_newsrc_mark_article_read (store->newsrc,
						     nntp_folder->group_name,
						     XXX);
 	}
	*/
}

static const gchar *
nntp_folder_get_name (CamelFolder *folder)
{
	g_assert(0);
	return NULL;
}

static const gchar *
nntp_folder_get_full_name (CamelFolder *folder)
{
	g_assert(0);
	return NULL;
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
nntp_folder_get_message_count (CamelFolder *folder, CamelException *ex)
{
	CamelNNTPFolder *nntp_folder = CAMEL_NNTP_FOLDER(folder);

	g_assert (folder);
	g_assert (nntp_folder->summary);

        return camel_folder_summary_count(nntp_folder->summary);
}

static guint32
nntp_folder_get_message_flags (CamelFolder *folder, const char *uid, CamelException *ex)
{
	CamelNNTPFolder *nntp_folder = CAMEL_NNTP_FOLDER (folder);
	CamelMessageInfo *info = camel_folder_summary_uid (nntp_folder->summary, uid);

	return info->flags;
}

static void
nntp_folder_set_message_flags (CamelFolder *folder, const char *uid,
			       guint32 flags, guint32 set, CamelException *ex)
{
	CamelNNTPFolder *nntp_folder = CAMEL_NNTP_FOLDER (folder);
	CamelMessageInfo *info = camel_folder_summary_uid (nntp_folder->summary, uid);

	info->flags = set;

	camel_folder_summary_touch (nntp_folder->summary);
}

static const gchar*
nntp_folder_get_message_uid (CamelFolder *folder,
			     CamelMimeMessage *message,
			     CamelException *ex)
{
	g_assert (0);
	return NULL;
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

	/* get the parent store */
	parent_store = camel_folder_get_parent_store (folder, ex);
	if (camel_exception_get_id (ex)) {
		return NULL;
	}

	status = camel_nntp_command (CAMEL_NNTP_STORE( parent_store ), NULL, "ARTICLE %s", uid);

	nntp_istream = CAMEL_NNTP_STORE (parent_store)->istream;

	/* if the uid was not found, raise an exception and return */
	if (status != CAMEL_NNTP_OK) {
		camel_exception_setv (ex, 
				     CAMEL_EXCEPTION_FOLDER_INVALID_UID,
				     "message %s not found.",
				      uid);
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
		gtk_object_unref (GTK_OBJECT (message));
		gtk_object_unref (GTK_OBJECT (message_stream));
		camel_exception_setv (ex,
				      CAMEL_EXCEPTION_FOLDER_INVALID_UID, /* XXX */
				      "Could not create message for uid %s.", uid);

		return NULL;
	}
	gtk_object_unref (GTK_OBJECT (message_stream));

	/* init other fields? */
	gtk_object_ref (GTK_OBJECT (folder));

#if 0
	gtk_signal_connect (GTK_OBJECT (message), "message_changed", message_changed, folder);
#endif

	return message;
}

static void
nntp_folder_delete_message (CamelFolder *folder,
				   const gchar *uid,
				   CamelException *ex)
{
	g_assert (0);
}

static GPtrArray *
nntp_folder_get_uids (CamelFolder *folder,
		      CamelException *ex)
{
	CamelNNTPFolder *nntp_folder = CAMEL_NNTP_FOLDER (folder);
	GPtrArray *infoarray, *out;
	CamelMessageInfo *message_info;
	int i;

	infoarray = nntp_folder->summary->messages;

	out = g_ptr_array_new ();
	g_ptr_array_set_size (out, infoarray->len);
	
	for (i=0; i<infoarray->len; i++) {
		message_info = (CamelMessageInfo *) g_ptr_array_index (infoarray, i);
		out->pdata[i] = g_strdup (message_info->uid);
	}
	
	return out;
}

static GPtrArray *
nntp_folder_get_summary (CamelFolder *folder,
			 CamelException *ex)
{
	CamelNNTPFolder *nntp_folder = CAMEL_NNTP_FOLDER (folder);

	return nntp_folder->summary->messages;
}

static GPtrArray *
nntp_folder_get_subfolder_names (CamelFolder *folder, CamelException *ex)
{
	if (!strcmp (folder->name, "/")) {
		CamelStore *store = camel_folder_get_parent_store (folder, ex);
		GPtrArray *array = camel_nntp_newsrc_get_subscribed_group_names (CAMEL_NNTP_STORE (store)->newsrc);
		return array;
	}
	else {
		return NULL;
	}
}

static void
nntp_folder_free_subfolder_names (CamelFolder *folder, GPtrArray *subfolders)
{
	if (subfolders) {
		CamelException *ex = camel_exception_new ();
		CamelStore *store = camel_folder_get_parent_store (folder, ex);
		camel_nntp_newsrc_free_group_names (CAMEL_NNTP_STORE (store)->newsrc, subfolders);
		camel_exception_free (ex);
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
	CamelMessageInfo *info = NULL;
	int i;

	for (i = 0; i < nntp_folder->summary->messages->len; i++) {
		info = g_ptr_array_index (nntp_folder->summary->messages, i);
		if (!strcmp (info->uid, uid))
			return info;
	}

	return NULL;
}

static void           
nntp_folder_finalize (GtkObject *object)
{
	CamelNNTPFolder *nntp_folder = CAMEL_NNTP_FOLDER (object);

	g_free (nntp_folder->summary_file_path);

	GTK_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
camel_nntp_folder_class_init (CamelNNTPFolderClass *camel_nntp_folder_class)
{
	CamelFolderClass *camel_folder_class = CAMEL_FOLDER_CLASS (camel_nntp_folder_class);
	GtkObjectClass *gtk_object_class = GTK_OBJECT_CLASS (camel_folder_class);

	parent_class = gtk_type_class (camel_folder_get_type ());
		
	/* virtual method definition */

	/* virtual method overload */
	camel_folder_class->init = nntp_folder_init;
	camel_folder_class->sync = nntp_folder_sync;
	camel_folder_class->get_name = nntp_folder_get_name;
	camel_folder_class->get_full_name = nntp_folder_get_full_name;
	camel_folder_class->get_subfolder = nntp_folder_get_subfolder;
	camel_folder_class->get_message_count = nntp_folder_get_message_count;
	camel_folder_class->set_message_flags = nntp_folder_set_message_flags;
	camel_folder_class->get_message_flags = nntp_folder_get_message_flags;
	camel_folder_class->get_message_uid = nntp_folder_get_message_uid;
	camel_folder_class->get_message = nntp_folder_get_message;
	camel_folder_class->delete_message = nntp_folder_delete_message;
	camel_folder_class->get_uids = nntp_folder_get_uids;
	camel_folder_class->free_uids = camel_folder_free_deep;
	camel_folder_class->get_summary = nntp_folder_get_summary;
	camel_folder_class->free_summary = camel_folder_free_nop;
	camel_folder_class->get_subfolder_names = nntp_folder_get_subfolder_names;
	camel_folder_class->free_subfolder_names = nntp_folder_free_subfolder_names;
	camel_folder_class->search_by_expression = nntp_folder_search_by_expression;
	camel_folder_class->get_message_info = nntp_folder_get_message_info;

	gtk_object_class->finalize = nntp_folder_finalize;
	
}

GtkType
camel_nntp_folder_get_type (void)
{
	static GtkType camel_nntp_folder_type = 0;
	
	if (!camel_nntp_folder_type)	{
		GtkTypeInfo camel_nntp_folder_info =	
		{
			"CamelNNTPFolder",
			sizeof (CamelNNTPFolder),
			sizeof (CamelNNTPFolderClass),
			(GtkClassInitFunc) camel_nntp_folder_class_init,
			(GtkObjectInitFunc) NULL,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		camel_nntp_folder_type = gtk_type_unique (CAMEL_FOLDER_TYPE, &camel_nntp_folder_info);
	}
	
	return camel_nntp_folder_type;
}
