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
#include "camel-nntp-resp-codes.h"
#include "camel-nntp-store.h"
#include "camel-nntp-folder.h"
#include "camel-nntp-store.h"
#include "camel-nntp-utils.h"

#include "string-utils.h"
#include "camel-stream-mem.h"
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
nntp_folder_sync (CamelFolder *folder, gboolean expunge, 
		  CamelException *ex)
{
	CamelNNTPStore *store;

	camel_folder_summary_save (folder->summary);

	store = CAMEL_NNTP_STORE (camel_folder_get_parent_store (folder));

	if (store->newsrc)
		camel_nntp_newsrc_write (store->newsrc);
}

static void
nntp_folder_set_message_flags (CamelFolder *folder, const char *uid,
			       guint32 flags, guint32 set)
{
        ((CamelFolderClass *)parent_class)->set_message_flags(folder, uid, flags, set);

	if (flags & set & CAMEL_MESSAGE_SEEN) {
		int article_num;
		CamelNNTPStore *nntp_store = CAMEL_NNTP_STORE (camel_folder_get_parent_store (folder));

		sscanf (uid, "%d", &article_num);

		camel_nntp_newsrc_mark_article_read (nntp_store->newsrc,
						     folder->name,
						     article_num);
	}
}

static CamelMimeMessage *
nntp_folder_get_message (CamelFolder *folder, const gchar *uid, CamelException *ex)
{
	CamelStream *message_stream = NULL;
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
	status = camel_nntp_command (CAMEL_NNTP_STORE( parent_store ), ex, NULL, "ARTICLE %s", message_id);

	/* if the message_id was not found, raise an exception and return */
	if (status == NNTP_NO_SUCH_ARTICLE) {
		camel_exception_setv (ex, 
				     CAMEL_EXCEPTION_FOLDER_INVALID_UID,
				     _("Message %s not found."),
				      message_id);
		return NULL;
	}
	else if (status != NNTP_ARTICLE_FOLLOWS) {
		/* XXX */
		g_warning ("weird nntp error %d\n", status);
		return NULL;
	}

	/* this could probably done fairly easily with an nntp stream that
	   returns eof after '.' */

	/* XXX ick ick ick.  read the entire message into a buffer and
	   then create a stream_mem for it. */
	buf_alloc = 2048;
	buf_len = 0;
	buf = g_malloc(buf_alloc);
	done = FALSE;

	buf[0] = 0;

	while (!done) {
		int line_length;
		char *line;

		if (camel_remote_store_recv_line (CAMEL_REMOTE_STORE (parent_store), &line, ex) < 0) {
			g_warning ("recv_line failed while building message\n");
			break;
		}

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
	camel_data_wrapper_construct_from_stream (CAMEL_DATA_WRAPPER(message), message_stream);

	camel_object_unref (CAMEL_OBJECT (message_stream));

#if 0
	gtk_signal_connect (CAMEL_OBJECT (message), "message_changed", message_changed, folder);
#endif

	g_free (buf);

	return message;
}

static GPtrArray*
nntp_folder_search_by_expression (CamelFolder *folder, const char *expression, CamelException *ex)
{
	g_assert (0);
	return NULL;
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
	camel_folder_class->sync = nntp_folder_sync;
	camel_folder_class->set_message_flags = nntp_folder_set_message_flags;
	camel_folder_class->get_message = nntp_folder_get_message;
	camel_folder_class->search_by_expression = nntp_folder_search_by_expression;
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

CamelFolder *
camel_nntp_folder_new (CamelStore *parent, const char *folder_name, CamelException *ex)
{
	CamelFolder *folder = CAMEL_FOLDER (camel_object_new (CAMEL_NNTP_FOLDER_TYPE));
	CamelNNTPFolder *nntp_folder = CAMEL_NNTP_FOLDER (folder);
	const gchar *root_dir_path;

	camel_folder_construct (folder, parent, folder_name, folder_name);
	folder->has_summary_capability = TRUE;

	root_dir_path = camel_nntp_store_get_toplevel_dir (CAMEL_NNTP_STORE(folder->parent_store));
	nntp_folder->summary_file_path = g_strdup_printf ("%s/%s-ev-summary",
							  root_dir_path,
							  folder->name);

	folder->summary = camel_folder_summary_new ();
	camel_folder_summary_set_filename (folder->summary,
					   nntp_folder->summary_file_path);

	if (-1 == camel_folder_summary_load (folder->summary)) {
		/* Bad or nonexistant summary file */
		camel_nntp_get_headers (CAMEL_FOLDER( folder )->parent_store,
					nntp_folder, ex);
		if (camel_exception_get_id (ex)) {
			camel_object_unref (CAMEL_OBJECT (folder));
			return NULL;
		}

		/* XXX check return value */
		camel_folder_summary_save (folder->summary);
	}

	return folder;
}
