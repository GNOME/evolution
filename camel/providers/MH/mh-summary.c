/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 *
 * Copyright (C) 1999 Bertrand Guiheneuf <Bertrand.Guiheneuf@aful.org> .
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

#include "mh-uid.h"
#include "camel-log.h"
#include "camel-stream.h"
#include "camel-stream-fs.h"
#include "camel-stream-buffered-fs.h"
#include "gmime-utils.h"
#include "mh-utils.h"

#include <sys/stat.h> 
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

void 
mh_generate_summary (CamelFolder *folder)
{
	CamelMhFolder *mh_folder = CAMEL_MH_FOLDER (folder);
	CamelFolderSummary *summary;
	CamelMessageInfo *message_info;
	CamelFolderInfo *subfolder_info;
	CamelStream *message_stream;
	guint file_number;
	gchar *message_fullpath;
	gchar *directory_path;
	GArray *header_array;
	MhUidCouple *uid_couple;
	Rfc822Header *cur_header;
	int i;
	int n_file;
	GArray *uid_array;

	CAMEL_LOG_FULL_DEBUG ("CamelMhFolder::generate_summary entering \n");

	g_assert (folder);

	directory_path = mh_folder->directory_path;
	if (!directory_path) {
		CAMEL_LOG_FULL_DEBUG ("CamelMhFolder::generate_summary folder has no directory path\n");
		return;
	}

	summary = camel_folder_summary_new ();
	folder->summary = summary;

	uid_array = mh_folder->uid_array;
	
	if (!uid_array) {
		CAMEL_LOG_FULL_DEBUG ("CamelMhFolder::generate_summary "
				      "no uid list, that probably means there is "
				      "no message in this folder, exiting \n");
		return;
	}
	uid_couple = (MhUidCouple *)uid_array->data;

	for (n_file=0; n_file<uid_array->len; n_file++) {

		file_number = uid_couple->file_number;

		message_info = g_new0 (CamelMessageInfo, 1);
		message_info->uid = g_new0 (guchar, 17);
		strncpy (message_info->uid, uid_couple->uid, 16);


		message_fullpath = g_strdup_printf ("%s/%d", directory_path, file_number);
		message_stream = camel_stream_buffered_fs_new_with_name (message_fullpath, 
									 CAMEL_STREAM_BUFFERED_FS_READ);
		if (!message_stream) {
			CAMEL_LOG_FULL_DEBUG ("CamelMhFolder::generate_summary "
					      "could not open %d for reading\n", message_fullpath);
			g_free (message_fullpath);
			return;
		}
		g_free (message_fullpath);
		
		header_array = get_header_array_from_stream (message_stream);
		gtk_object_unref (GTK_OBJECT (message_stream));
		
		for (i=0; i<header_array->len; i++) {
			cur_header = (Rfc822Header *)header_array->data + i;
			if (!g_strcasecmp (cur_header->name, "subject")) {
				message_info->subject = cur_header->value;
				g_free (cur_header->name);
			} else if (!g_strcasecmp (cur_header->name, "sender")) {
				message_info->date = cur_header->value;
				g_free (cur_header->name);
			} else if (!g_strcasecmp (cur_header->name, "date")) {
				message_info->date = cur_header->value;
				g_free (cur_header->name);
			} else {
				g_free (cur_header->name);
				g_free (cur_header->value);
			}
		}		
		g_array_free (header_array, TRUE);

		summary->message_info_list = g_list_append (summary->message_info_list, message_info);

		/* next message in the uid list */
		uid_couple++;
	}	
	CAMEL_LOG_FULL_DEBUG ("CamelMhFolder::generate_summary leaving \n");

}


void 
mh_save_summary (CamelMhFolder *mh_folder)
{
	GArray *uid_array;
	MhUidCouple *first_uid_couple;
	CamelFolderSummary *summary;
	GList *msg_info_list;
	CamelMessageInfo *msg_info;
	gchar *directory_path = mh_folder->directory_path;
	gchar *summary_file_path;
	gint fd;
	gint i;
	gint field_lgth;
	
	CAMEL_LOG_FULL_DEBUG ("CamelMhFolder::save_summary entering \n");

	summary = CAMEL_FOLDER (mh_folder)->summary;
	if (!summary) return;
	
	summary_file_path = g_strdup_printf ("%s/%s", directory_path, ".camel-summary");
	CAMEL_LOG_FULL_DEBUG ("In the process of writing %s\n", summary_file_path);
	fd = open (summary_file_path, O_WRONLY | O_CREAT );
	
	if (!fd) {
		CAMEL_LOG_FULL_DEBUG ("could not open file %s for writing. Exiting.\n", summary_file_path);
		g_free (summary_file_path);
		return;
	}
	g_free (summary_file_path);

	msg_info_list = summary->message_info_list;
	while (msg_info_list) {
		msg_info = msg_info_list->data;
		/* write subject */
		field_lgth = msg_info->subject ? strlen (msg_info->subject) : 0;
		write (fd, &field_lgth, sizeof (gint));
		if (field_lgth)
			write (fd, msg_info->subject, field_lgth);

		/* write uid */
		field_lgth = msg_info->uid ? strlen (msg_info->uid) : 0;
		write (fd, &field_lgth, sizeof (gint));
		if (field_lgth)
			write (fd, msg_info->uid, field_lgth);

		/* write date */
		field_lgth = msg_info->date ? strlen (msg_info->date) : 0;
		write (fd, &field_lgth, sizeof (gint));
		if (field_lgth)
			write (fd, msg_info->date, field_lgth);

		/* write sender */
		field_lgth = msg_info->sender ? strlen (msg_info->sender) : 0;
		write (fd, &field_lgth, sizeof (gint));
		if (field_lgth)
			write (fd, msg_info->sender, field_lgth);

		msg_info_list = msg_info_list->next;
		
	}
			
	close (fd);

	CAMEL_LOG_FULL_DEBUG ("CamelMhFolder::save_summary leaving \n");
		
}





gint
mh_load_summary (CamelMhFolder *mh_folder)
{
	GArray *uid_array;
	MhUidCouple *first_uid_couple;
	CamelFolderSummary *summary;
	CamelMessageInfo *msg_info;
	gchar *directory_path = mh_folder->directory_path;
	gchar *summary_file_path;
	gint fd;
	gint i;
	gint field_lgth;
	gboolean file_eof;
	gint stat_error;
	struct stat stat_buf;

	summary = CAMEL_FOLDER (mh_folder)->summary;
	if (summary) return 1; /* should we regenerate it ? */
	
	summary_file_path = g_strdup_printf ("%s/%s", directory_path, ".camel-summary");
	CAMEL_LOG_FULL_DEBUG ("In the process of reading %s\n", summary_file_path);
	fd = open (summary_file_path, O_RDONLY);
	/* tests if file exists */
	stat_error = stat (summary_file_path, &stat_buf);

	if (!((stat_error != -1) && S_ISREG (stat_buf.st_mode))) {		
		CAMEL_LOG_FULL_DEBUG ("could not open file %s for reading. Exiting.\n", summary_file_path);
		g_free (summary_file_path);
		return -1;
	}
	g_free (summary_file_path);
	
	for (;;)  {
		/* read subject */
		file_eof = (read (fd, &field_lgth, sizeof (gint)) <= 0);
		if (file_eof) break;
		
		
		/* allcate a summary if needed */
		if (!summary) 
			summary = camel_folder_summary_new ();
		/* allocate a message info struct */
		msg_info = g_new0 (CamelMessageInfo, 1);	
	
		if (!file_eof && (field_lgth > 0)) {			
			msg_info->subject = g_new0 (gchar, field_lgth + 1);
			read (fd, msg_info->subject, field_lgth);
		} else 
			msg_info->subject = NULL;
		
		/* read uid */
		if (!file_eof) file_eof = (read (fd, &field_lgth, sizeof (gint)) <= 0);
		if (!file_eof && (field_lgth > 0)) {		
			msg_info->uid = g_new0 (gchar, field_lgth + 1);
			read (fd, msg_info->uid, field_lgth);
		} else 
			msg_info->uid = NULL;
		
		/* read date */
		if (!file_eof) file_eof = (read (fd, &field_lgth, sizeof (gint)) <= 0);
		if (!file_eof && (field_lgth > 0)) {			
			msg_info->date = g_new0 (gchar, field_lgth + 1);
			read (fd, msg_info->date, field_lgth);
		} else 
			msg_info->date = NULL;
		
		/* read sender */
		if (!file_eof) file_eof = (read (fd, &field_lgth, sizeof (gint)) <= 0);
		if (!file_eof && (field_lgth > 0)) {			
			msg_info->sender = g_new0 (gchar, field_lgth + 1);
			read (fd, msg_info->sender, field_lgth);
		} else 
			msg_info->sender = NULL;

		summary->message_info_list = g_list_prepend (summary->message_info_list, 
							     msg_info);
	}		
	
	CAMEL_FOLDER (mh_folder)->summary = summary;
			
	close (fd);
	return 1;
}


