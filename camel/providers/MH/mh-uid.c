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
#include "camel-stream.h"
#include "camel-stream-fs.h"
#include "camel-stream-buffered-fs.h"
#include "gmime-utils.h"
#include "md5-utils.h"
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
mh_uid_get_for_file (gchar *filename, guchar uid[16])
{
	CamelStream *message_stream;
	GArray *header_array;
	Rfc822Header *cur_header;
	int i;
	MD5Context ctx;


	message_stream = camel_stream_buffered_fs_new_with_name (filename, 
								 CAMEL_STREAM_BUFFERED_FS_READ);
	header_array = get_header_array_from_stream (message_stream);
	gtk_object_unref (GTK_OBJECT (message_stream));
	
	md5_init (&ctx);
	for (i=0; i<header_array->len; i++) {
		cur_header = (Rfc822Header *)header_array->data + i;
		if (!g_strcasecmp (cur_header->name, "subject")) {
			md5_update (&ctx, cur_header->value, strlen (cur_header->value));
		} else if (!g_strcasecmp (cur_header->name, "sender")) {
			md5_update (&ctx, cur_header->value, strlen (cur_header->value));
		} else if (!g_strcasecmp (cur_header->name, "date")) {
			md5_update (&ctx, cur_header->value, strlen (cur_header->value));			
		}
		
		g_free (cur_header->name);
		g_free (cur_header->value);
		
	}
	
	g_array_free (header_array, TRUE);
	
	md5_final (uid, &ctx);
}




void 
mh_save_uid_list (CamelMhFolder *mh_folder)
{
	GArray *uid_array;
	MhUidCouple *first_uid_couple;
	gchar *directory_path = mh_folder->directory_path;
	gchar *uidfile_path;
	int fd;
	int i;
	
	
	uidfile_path = g_strdup_printf ("%s/%s", directory_path, ".camel-uid-list");
	CAMEL_LOG_FULL_DEBUG ("In the process of writing %s\n", uidfile_path);
	fd = open (uidfile_path, O_WRONLY | O_CREAT );
	
	if (!fd) {
		CAMEL_LOG_FULL_DEBUG ("could not open file %s for writing. Exiting.\n", uidfile_path);
		g_free (uidfile_path);
		return;
	}
	g_free (uidfile_path);

	uid_array = mh_folder->uid_array;
	first_uid_couple = (MhUidCouple *)uid_array->data;

	/* write the number of uid contained in the file */
	write (fd, &(uid_array->len), sizeof (guint));
	CAMEL_LOG_FULL_DEBUG ("%d entrie present in the list\n", uid_array->len);
	/* now write the array of uid self */
	write (fd, first_uid_couple, sizeof (MhUidCouple) * uid_array->len);
	
	close (fd);
}


gint 
mh_load_uid_list (CamelMhFolder *mh_folder)
{
	GArray *new_uid_array;
	MhUidCouple *first_uid_couple;
	gchar *directory_path = mh_folder->directory_path;
	gchar *uidfile_path;
	int fd;
	guint uid_nb;
	struct stat stat_buf;
	gint stat_error = 0;
	
	uidfile_path = g_strdup_printf ("%s/%s", directory_path, ".camel-uid-list");

	/* tests if file exists */
	stat_error = stat (uidfile_path, &stat_buf);
	

	if (!((stat_error != -1) && S_ISREG (stat_buf.st_mode))) {
		CAMEL_LOG_FULL_DEBUG ("file %s does not exist. Exiting.\n", uidfile_path);
		g_free (uidfile_path);
		return -1;
	}

	fd = open (uidfile_path, O_RDONLY);
	g_free (uidfile_path);
	if (!fd) return -1;
	
	if (mh_folder->uid_array) g_array_free (mh_folder->uid_array, FALSE);

	/* read the number of uids in the file */
	read (fd, &uid_nb, sizeof (guint));
	CAMEL_LOG_FULL_DEBUG ("reading %d uid_entries\n", uid_nb);
	new_uid_array = g_array_new (FALSE, FALSE, sizeof (MhUidCouple));
	new_uid_array = g_array_set_size (new_uid_array, uid_nb);
	first_uid_couple = (MhUidCouple *)new_uid_array->data;
	
	
	read (fd, first_uid_couple, sizeof (MhUidCouple) * uid_nb);
	
	mh_folder->uid_array = new_uid_array;		

	return 1;
}


gint 
mh_generate_uid_list (CamelMhFolder *mh_folder)
{	
	GArray *new_uid_array;
	const gchar *directory_path;
	struct dirent *dir_entry;
	DIR *dir_handle;
	gchar *msg_path;
	guint msg_count;	
	MhUidCouple *uid_couple;
	guint file_number;

	g_assert (mh_folder);
	CAMEL_LOG_FULL_DEBUG ("in the process of creating uid list \n");
	directory_path = mh_folder->directory_path;
	if (!directory_path) {
		CAMEL_LOG_FULL_DEBUG ("folder has no directory path. Exiting\n");
		return -1;
	}
		
	msg_count = camel_folder_get_message_count (CAMEL_FOLDER (mh_folder));
	if (!msg_count) {
		CAMEL_LOG_FULL_DEBUG ("no message in %s. Exiting\n", directory_path);
		return -1;
	}
	
	new_uid_array = g_array_new (FALSE, FALSE, sizeof (MhUidCouple));
	new_uid_array = g_array_set_size (new_uid_array, msg_count);
	uid_couple = (MhUidCouple *)new_uid_array->data;

	dir_handle = opendir (directory_path);
	
	/* read first entry in the directory */
	dir_entry = readdir (dir_handle);
	while (dir_entry != NULL) {

		/* tests if the entry correspond to a message file */
		if (mh_is_a_message_file (dir_entry->d_name, directory_path)) {
						
			/* get the uid for this message */
			msg_path = g_strdup_printf ("%s/%s", directory_path, dir_entry->d_name);
			mh_uid_get_for_file (msg_path, uid_couple->uid);
			g_free (msg_path);

			/* convert filename into file number */
			uid_couple->file_number = atoi (dir_entry->d_name);
			uid_couple++;
		}
			
		/* read next entry */
		dir_entry = readdir (dir_handle);
	}

	closedir (dir_handle);
	mh_folder->uid_array = new_uid_array;
}
