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


static void _init (CamelFolder *folder, CamelStore *parent_store,
		   CamelFolder *parent_folder, const gchar *name,
		   gchar separator, CamelException *ex);

static void _open (CamelFolder *folder, CamelFolderOpenMode mode, CamelException *ex);
static void _close (CamelFolder *folder, gboolean expunge, CamelException *ex);
static gboolean _exists (CamelFolder *folder, CamelException *ex);
static gboolean _create(CamelFolder *folder, CamelException *ex);
static gboolean _delete (CamelFolder *folder, gboolean recurse, CamelException *ex);
static gboolean _delete_messages (CamelFolder *folder, CamelException *ex);
static GList *_list_subfolders (CamelFolder *folder, CamelException *ex);
static CamelMimeMessage *_get_message_by_number (CamelFolder *folder, gint number, CamelException *ex);
static gint _get_message_count (CamelFolder *folder, CamelException *ex);
static void _append_message (CamelFolder *folder, CamelMimeMessage *message, CamelException *ex);
static GPtrArray *_get_uid_array  (CamelFolder *folder, CamelException *ex);
static CamelMimeMessage *_get_message_by_uid (CamelFolder *folder, const gchar *uid, CamelException *ex);
#if 0
static void _expunge (CamelFolder *folder, CamelException *ex);
static void _copy_message_to (CamelFolder *folder, CamelMimeMessage *message, CamelFolder *dest_folder, CamelException *ex);
#endif
static const gchar *_get_message_uid (CamelFolder *folder, CamelMimeMessage *message, CamelException *ex);

static void _finalize (GtkObject *object);

static void
camel_nntp_folder_class_init (CamelNNTPFolderClass *camel_nntp_folder_class)
{
	CamelFolderClass *camel_folder_class = CAMEL_FOLDER_CLASS (camel_nntp_folder_class);
	GtkObjectClass *gtk_object_class = GTK_OBJECT_CLASS (camel_folder_class);

	parent_class = gtk_type_class (camel_folder_get_type ());
		
	/* virtual method definition */

	/* virtual method overload */
	camel_folder_class->init = _init;
	camel_folder_class->open = _open;
	camel_folder_class->close = _close;
	camel_folder_class->exists = _exists;
	camel_folder_class->create = _create;
	camel_folder_class->delete = _delete;
	camel_folder_class->delete_messages = _delete_messages;
	camel_folder_class->list_subfolders = _list_subfolders;
	camel_folder_class->get_message_count = _get_message_count;
	camel_folder_class->get_uid_array = _get_uid_array;
	camel_folder_class->get_message_by_uid = _get_message_by_uid;
#if 0 
	camel_folder_class->append_message = _append_message;
	camel_folder_class->expunge = _expunge;
	camel_folder_class->copy_message_to = _copy_message_to;
	camel_folder_class->get_message_uid = _get_message_uid;
#endif

	gtk_object_class->finalize = _finalize;
	
}

static void           
_finalize (GtkObject *object)
{
	CamelNNTPFolder *nntp_folder = CAMEL_NNTP_FOLDER (object);

	g_free (nntp_folder->summary_file_path);

	GTK_OBJECT_CLASS (parent_class)->finalize (object);
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

static void 
_init (CamelFolder *folder, CamelStore *parent_store,
       CamelFolder *parent_folder, const gchar *name, gchar separator,
       CamelException *ex)
{
	/* call parent method */
	parent_class->init (folder, parent_store, parent_folder,
			    name, separator, ex);
	if (camel_exception_get_id (ex)) return;

	/* we assume that the parent init
	   method checks for the existance of @folder */

	if (!strcmp(name, "/"))
	  {
	    folder->has_summary_capability = FALSE;
	    folder->can_hold_messages = FALSE;
	    folder->can_hold_folders = TRUE;
	  }
	else
	  {
	    folder->has_summary_capability = TRUE;
	    folder->can_hold_messages = TRUE;
	    folder->can_hold_folders = TRUE;
	  }

	folder->has_uid_capability = TRUE;
	folder->has_search_capability = FALSE;
}

/* internal method used to : 
   - test for the existence of a summary file 
   - test the sync between the summary and the newsgroup
   - load the summary or create it if necessary 
*/ 
static void
_check_get_or_maybe_generate_summary_file (CamelNNTPFolder *nntp_folder,
					   CamelException *ex)
{
	CamelFolder *folder = CAMEL_FOLDER (nntp_folder);

	nntp_folder->summary = camel_folder_summary_new ();
	camel_folder_summary_set_filename (nntp_folder->summary, nntp_folder->summary_file_path);

	if (-1 == camel_folder_summary_load (nntp_folder->summary)) {
		/* Bad or nonexistant summary file */
		camel_nntp_get_headers (CAMEL_FOLDER( folder )->parent_store, nntp_folder, ex);
		if (camel_exception_get_id (ex))
			return;

		/* XXX check return value */
		camel_folder_summary_save (nntp_folder->summary);
	}
}


static void
_open (CamelFolder *folder, CamelFolderOpenMode mode, CamelException *ex)
{
	CamelNNTPFolder *nntp_folder = CAMEL_NNTP_FOLDER (folder);
	const gchar *root_dir_path;

	/* call parent class */
	parent_class->open (folder, mode, ex);
	if (camel_exception_get_id(ex))
		return;

#if 0
	/* get (or create) uid list */
	if (!(nntp_load_uid_list (nntp_folder) > 0))
		nntp_generate_uid_list (nntp_folder);
#endif

	root_dir_path = camel_nntp_store_get_toplevel_dir (CAMEL_NNTP_STORE(folder->parent_store));

	nntp_folder->summary_file_path = g_strdup_printf ("%s/%s-ev-summary", root_dir_path, folder->name);

	_check_get_or_maybe_generate_summary_file (nntp_folder, ex);
}


static void
_close (CamelFolder *folder, gboolean expunge, CamelException *ex)
{
	CamelNNTPFolder *nntp_folder = CAMEL_NNTP_FOLDER (folder);
	CamelFolderSummary *summary = nntp_folder->summary;

	/* call parent implementation */
	parent_class->close (folder, expunge, ex);

	/* XXX only if dirty? */
	camel_folder_summary_save (summary);
}

static gboolean
_exists (CamelFolder *folder, CamelException *ex)
{
#if 0
	CamelNNTPFolder *nntp_folder;
	struct stat stat_buf;
	gint stat_error;
	gboolean exists;

	g_assert(folder != NULL);

	nntp_folder = CAMEL_NNTP_FOLDER (folder);

	/* check if the nntp summary path is determined */
	if (!nntp_folder->summary_file_path) {
		camel_exception_set (ex, 
				     CAMEL_EXCEPTION_FOLDER_INVALID,
				     "undetermined folder summary path. Maybe use set_name ?");
		return FALSE;
	}

	/* check if the nntp file exists */
	stat_error = stat (nntp_folder->summary_file_path, &stat_buf);
	if (stat_error == -1)
		return FALSE;
	
	exists = S_ISREG (stat_buf.st_mode);
	/* we should  check the rights here  */
	
	return exists;
#endif
	return TRUE;
}

static gboolean
_create (CamelFolder *folder, CamelException *ex)
{
#if 0
	CamelNNTPSummary *summary;
	g_assert(folder != NULL);

	/* call default implementation */
	parent_class->create (folder, ex);

	/* create the summary object */
	summary = CAMEL_NNTP_SUMMARY (gtk_object_new (camel_nntp_summary_get_type (), NULL));
	summary->nb_message = 0;
	summary->next_uid = 1;
	summary->nntp_file_size = 0;
	summary->message_info = g_array_new (FALSE, FALSE, sizeof (CamelNNTPSummaryInformation));
#endif
	
	return TRUE;
}

static gboolean
_delete (CamelFolder *folder, gboolean recurse, CamelException *ex)
{
#if 0
	gboolean folder_already_exists;

	g_assert(folder != NULL);

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
#endif
	return TRUE;
}

gboolean
_delete_messages (CamelFolder *folder, CamelException *ex)
{
	
	gboolean folder_already_exists;

	g_assert(folder!=NULL);
	
	/* in the case where the folder does not exist, 
	   return immediatly */
	folder_already_exists = camel_folder_exists (folder, ex);
	if (camel_exception_get_id (ex)) return FALSE;

	if (!folder_already_exists) return TRUE;

	return TRUE;
}

static GList *
_list_subfolders (CamelFolder *folder, CamelException *ex)
{
	/* newsgroups don't have subfolders */
	return NULL;
}

static gint
_get_message_count (CamelFolder *folder, CamelException *ex)
{
	CamelNNTPFolder *nntp_folder = CAMEL_NNTP_FOLDER(folder);

	g_assert (folder);
	g_assert (nntp_folder->summary);

        return camel_folder_summary_count(nntp_folder->summary);
}

#if 0
static void
_append_message (CamelFolder *folder, CamelMimeMessage *message, CamelException *ex)
{
	CamelNNTPFolder *nntp_folder = CAMEL_NNTP_FOLDER (folder);
#if 0
	CamelNNTPSummary *summary = CAMEL_NNTP_SUMMARY (folder->summary);
#endif
	CamelStream *output_stream;
	guint32 tmp_file_size;
	guint32 next_uid;
	gint tmp_file_fd;
	GArray *message_info_array;
#if 0
	GArray *nntp_summary_info;
#endif
	gchar *tmp_message_filename;
	gint fd1, fd2;
	int i;

	/* write the message itself */
	output_stream = camel_stream_fs_new_with_name (tmp_message_filename,
						       CAMEL_STREAM_FS_WRITE);
	if (output_stream != NULL) {
		camel_stream_write_string (output_stream, "From - \n");
		camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (message), output_stream);
	}
	camel_stream_flush (output_stream);
	gtk_object_unref (GTK_OBJECT (output_stream));

	/* at this point we have saved the message to a
	   temporary file, now, we have to add the x-evolution 
	   field and also update the main summary */

	/* 
	   First : parse the nntp file, but only from the 
	   position where the message has been added, 
	   wich happens to be the last postion in the 
	   nntp file before we added the message.
	   This position is still stored in the summary 
	   for the moment 
	*/
	next_uid = summary->next_uid;
	tmp_file_fd = open (tmp_message_filename, O_RDONLY);
	message_info_array =
		camel_nntp_parse_file (tmp_file_fd, "From - ", 0,
				       &tmp_file_size, &next_uid, TRUE,
				       NULL, 0, ex); 
	
	close (tmp_file_fd);

	/* get the value of the last available UID
	   as saved in the summary file, again */
	next_uid = summary->next_uid;

	/* make sure all our of message info's have 0 uid - ignore any
	   set elsewhere */
	for (i=0;i<message_info_array->len;i++) {
		g_array_index(message_info_array, CamelNNTPParserMessageInfo, i).uid = 0;
	}

	/* 
	   OK, this is not very efficient, we should not use the same
	   method as for parsing an entire mail file, 
	   but I have no time to write a simpler parser 
	*/
#if 0
	next_uid = camel_nntp_write_xev (nntp_folder, tmp_message_filename, 
					 message_info_array, &tmp_file_size, next_uid, ex);
	
#endif
	if (camel_exception_get_id (ex)) { 
		/* ** FIXME : free the preparsed information */
		return;
	}

#if 0
	nntp_summary_info =
		parsed_information_to_nntp_summary (message_info_array);
#endif

	/* store the number of messages as well as the summary array */
	summary->nb_message += 1;		
	summary->next_uid = next_uid;	

	((CamelNNTPSummaryInformation *)(nntp_summary_info->data))->position +=
		summary->nntp_file_size;
	summary->nntp_file_size += tmp_file_size;		

	camel_nntp_summary_append_entries (summary, nntp_summary_info);
	g_array_free (nntp_summary_info, TRUE); 
	

	/* append the temporary file message to the nntp file */
	fd1 = open (tmp_message_filename, O_RDONLY);
	fd2 = open (nntp_folder->folder_file_path, 
		    O_WRONLY | O_CREAT | O_APPEND,
		    0600);

	if (fd2 == -1) {
		camel_exception_setv (ex, 
				      CAMEL_EXCEPTION_FOLDER_INSUFFICIENT_PERMISSION,
				      "could not open the nntp folder file for appending the message\n"
				      "\t%s\n"
				      "Full error is : %s\n",
				      nntp_folder->folder_file_path,
				      strerror (errno));
		return;
	}

	camel_nntp_copy_file_chunk (fd1,
				    fd2, 
				    tmp_file_size, 
				    ex);
	close (fd1);
	close (fd2);

	/* remove the temporary file */
	unlink (tmp_message_filename);

	g_free (tmp_message_filename);
}
#endif



static GPtrArray *
_get_uid_array (CamelFolder *folder, CamelException *ex) 
{
	CamelNNTPFolder *nntp_folder = CAMEL_NNTP_FOLDER (folder);
	GPtrArray *message_info_array, *out;
	CamelMessageInfo *message_info;
	int i;

	message_info_array = nntp_folder->summary->messages;

	out = g_ptr_array_new ();
	g_ptr_array_set_size (out, message_info_array->len);
	
	for (i=0; i<message_info_array->len; i++) {
		message_info = (CamelMessageInfo *)(message_info_array->pdata) + i;
		out->pdata[i] = g_strdup (message_info->uid);
	}
	
	return out;
}

static CamelMimeMessage *
_get_message_by_uid (CamelFolder *folder, const gchar *uid, CamelException *ex)
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
	buf = malloc(buf_alloc);
	done = FALSE;

	buf[0] = 0;

	while (!done) {
		char *line = camel_stream_buffer_read_line ( CAMEL_STREAM_BUFFER ( nntp_istream ), ex);
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
				buf = realloc (buf, buf_alloc);
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
		gtk_object_unref ((GtkObject *)message);
		gtk_object_unref ((GtkObject *)message_stream);
		camel_exception_setv (ex,
				      CAMEL_EXCEPTION_FOLDER_INVALID_UID, /* XXX */
				      "Could not create message for uid %s.", uid);

		return NULL;
	}
	gtk_object_unref ((GtkObject *)message_stream);

	/* init other fields? */
	message->folder = folder;
	gtk_object_ref((GtkObject *)folder);
	message->message_uid = g_strdup(uid);

#if 0
	gtk_signal_connect((GtkObject *)message, "message_changed", message_changed, folder);
#endif

	return message;
}

/* get message info for a range of messages */
static GPtrArray *
summary_get_message_info (CamelFolder *folder, int first, int count)
{
	GPtrArray *array = g_ptr_array_new();
	int i, maxcount;
	CamelNNTPFolder *nntp_folder = (CamelNNTPFolder *)folder;

        maxcount = camel_folder_summary_count(nntp_folder->summary);
	maxcount = MIN(first + count, maxcount);
	for (i=first;i<maxcount;i++)
		g_ptr_array_add(array, g_ptr_array_index(nntp_folder->summary->messages, i));

	return array;
}
