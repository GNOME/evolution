/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * Author : Bertrand Guiheneuf <bertrand@helixcode.com> 
 *
 * Copyright (C) 1999 - 2000 Helix Code .

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

#include "camel-exception.h"
#include "camel-mbox-folder.h"
#include "camel-mbox-summary.h"
#include "md5-utils.h"


#include <sys/stat.h> 
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>

static CamelFolderSummaryClass *parent_class = NULL;

static int count_messages (CamelFolderSummary *summary);
static int count_subfolders (CamelFolderSummary *summary);
static GPtrArray *get_subfolder_info (CamelFolderSummary *summary,
				      int first, int count);
static GPtrArray *get_message_info (CamelFolderSummary *summary,
				    int first, int count);
static void finalize (GtkObject *object);

static void
camel_mbox_summary_class_init (CamelMboxSummaryClass *camel_mbox_summary_class)
{
	GtkObjectClass *gtk_object_class =
		GTK_OBJECT_CLASS (camel_mbox_summary_class);
	CamelFolderSummaryClass *camel_folder_summary_class =
		CAMEL_FOLDER_SUMMARY_CLASS (camel_mbox_summary_class);

	parent_class = gtk_type_class (camel_folder_summary_get_type ());

	/* virtual method override */
	camel_folder_summary_class->count_messages = count_messages;
	camel_folder_summary_class->count_subfolders = count_subfolders;
	camel_folder_summary_class->get_subfolder_info = get_subfolder_info;
	camel_folder_summary_class->get_message_info = get_message_info;

	gtk_object_class->finalize = finalize;
}


GtkType
camel_mbox_summary_get_type (void)
{
	static GtkType camel_mbox_summary_type = 0;

	if (!camel_mbox_summary_type) {
		GtkTypeInfo camel_mbox_summary_info =	
		{
			"CamelMboxSummary",
			sizeof (CamelMboxSummary),
			sizeof (CamelMboxSummaryClass),
			(GtkClassInitFunc) camel_mbox_summary_class_init,
			(GtkObjectInitFunc) NULL,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		camel_mbox_summary_type = gtk_type_unique (camel_folder_summary_get_type (), &camel_mbox_summary_info);
	}

	return camel_mbox_summary_type;
}

static void
finalize (GtkObject *object)
{
	CamelMboxSummary *summary = CAMEL_MBOX_SUMMARY (object);
	CamelMboxSummaryInformation *info;
	int i;

	for (i = 0; i < summary->message_info->len; i++) {
		info = &(((CamelMboxSummaryInformation *)summary->message_info->data)[i]);
		g_free (info->headers.subject);
		g_free (info->headers.sender);
		g_free (info->headers.to);
		g_free (info->headers.sent_date);
		g_free (info->headers.received_date);
		g_free (info->headers.uid);
	}
	g_array_free (summary->message_info, TRUE);		

	GTK_OBJECT_CLASS (parent_class)->finalize (object);
}	

static int
count_messages (CamelFolderSummary *summary)
{
	return CAMEL_MBOX_SUMMARY (summary)->nb_message;
}

static int
count_subfolders (CamelFolderSummary *summary)
{
	/* XXX */
	g_warning ("CamelMboxSummary::count_subfolders not implemented");
	return 0;
}

static GPtrArray *
get_subfolder_info (CamelFolderSummary *summary, int first, int count)
{
	/* XXX */
	g_warning ("CamelMboxSummary::count_subfolders not implemented");
	return 0;
}

static GPtrArray *
get_message_info (CamelFolderSummary *summary, int first, int count)
{
	CamelMboxSummary *mbox_summary = CAMEL_MBOX_SUMMARY (summary);
	CamelMboxSummaryInformation *info;
	GPtrArray *arr;

	/* XXX bounds check */

	arr = g_ptr_array_new ();
	for (; count; count--) {
		info = &((CamelMboxSummaryInformation *)mbox_summary->message_info->data)[first++];
		g_ptr_array_add (arr, info);
	}

	return arr;
}

/**
 * camel_mbox_summary_save:
 * @summary: 
 * @filename: 
 * @ex: 
 * 
 * save the summary into a file 
 **/
void 
camel_mbox_summary_save (CamelMboxSummary *summary, const gchar *filename,
			 CamelException *ex)
{
	CamelMboxSummaryInformation *msg_info;
	guint cur_msg;
	guint field_length;
	gint fd;
	gint write_result; /* XXX use this */
	guint32 data;

	fd = open (filename, O_WRONLY | O_CREAT | O_TRUNC,
		   S_IRUSR | S_IWUSR);
	if (fd == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_FOLDER_INSUFFICIENT_PERMISSION,
				      "could not create the mbox summary "
				      "file\n\t%s\nFull error is : %s\n",
				      filename,
				      strerror (errno));
		return;
	}

	/* We write the file out in network byte order, not because
	 * that makes sense, but because it's easy.
	 */

	data = htonl (CAMEL_MBOX_SUMMARY_VERSION);
	write (fd, &data, sizeof (data));

	data = htonl (summary->nb_message);
	write (fd, &data, sizeof (data));
	data = htonl (summary->next_uid);
	write (fd, &data, sizeof (data));
	data = htonl (summary->mbox_file_size);
	write (fd, &data, sizeof (data));
	data = htonl (summary->mbox_modtime);
	write (fd, &data, sizeof (data));

	for (cur_msg = 0; cur_msg < summary->nb_message; cur_msg++) {
		msg_info = (CamelMboxSummaryInformation *)
			(summary->message_info->data) + cur_msg;

		/* Write meta-info. */
		data = htonl (msg_info->position);
		write (fd, &data, sizeof (data));
		data = htonl (msg_info->size);
		write (fd, &data, sizeof (data));
		data = htonl (msg_info->x_evolution_offset);
		write (fd, &data, sizeof (data));
		data = htonl (msg_info->uid);
		write (fd, &data, sizeof (data));
		write (fd, &msg_info->status, 1);

		/* Write subject. */
		if (msg_info->headers.subject)
			field_length = strlen (msg_info->headers.subject);
		else
			field_length = 0;
		data = htonl (field_length);
		write (fd, &data, sizeof (data));
		if (msg_info->headers.subject)
			write (fd, msg_info->headers.subject, field_length);

		/* Write sender. */
		if (msg_info->headers.sender)
			field_length = strlen (msg_info->headers.sender);
		else
			field_length = 0;
		data = htonl (field_length);
		write (fd, &data, sizeof (data));
		if (msg_info->headers.sender)
			write (fd, msg_info->headers.sender, field_length);

		/* Write to. */
		if (msg_info->headers.to)
			field_length = strlen (msg_info->headers.to);
		else
			field_length = 0;
		data = htonl (field_length);
		write (fd, &data, sizeof (data));
		if (msg_info->headers.to)
			write (fd, msg_info->headers.to, field_length);

		/* Write sent date. */
		if (msg_info->headers.sent_date)
			field_length = strlen (msg_info->headers.sent_date);
		else
			field_length = 0;
		data = htonl (field_length);
		write (fd, &data, sizeof (data));
		if (msg_info->headers.sent_date)
			write (fd, msg_info->headers.sent_date, field_length);

		/* Write received date. */
		if (msg_info->headers.received_date)
			field_length = strlen (msg_info->headers.received_date);
		else
			field_length = 0;
		data = htonl (field_length);
		write (fd, &data, sizeof (data));
		if (msg_info->headers.received_date)
			write (fd, msg_info->headers.received_date, field_length);
	}

	close (fd);
}



/**
 * camel_mbox_summary_load:
 * @filename: 
 * @ex: 
 * 
 * load the summary from a file 
 * 
 * Return value: 
 **/
CamelMboxSummary *
camel_mbox_summary_load (const gchar *filename, CamelException *ex)
{
	CamelMboxSummaryInformation *msg_info;
	guint cur_msg;
	guint field_length;
	gint fd;
	CamelMboxSummary *summary;
	gint read_result;
	guint32 data;

	fd = open (filename, O_RDONLY);
	if (fd == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_FOLDER_INSUFFICIENT_PERMISSION,
				      "could not open the mbox summary file\n"
				      "\t%s\nFull error is : %s\n",
				      filename, strerror (errno));
		return NULL;
	}

	/* Verify version number. */
	read (fd, &data, sizeof(data));
	data = ntohl (data);

	if (data != CAMEL_MBOX_SUMMARY_VERSION) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_FOLDER_SUMMARY_INVALID,
				      "This folder summary was written by "
				      "%s version of this software.",
				      data < CAMEL_MBOX_SUMMARY_VERSION ?
				      "an older" : "a newer");
		return NULL;
	}

	summary = CAMEL_MBOX_SUMMARY (gtk_object_new (camel_mbox_summary_get_type (), NULL));

	read (fd, &data, sizeof(data));
	summary->nb_message = ntohl (data);
	read (fd, &data, sizeof(data));
	summary->next_uid = ntohl (data);
	read (fd, &data, sizeof(data));
	summary->mbox_file_size = ntohl (data);
	read (fd, &data, sizeof(data));
	summary->mbox_modtime = ntohl (data);

	summary->message_info =
		g_array_new (FALSE, FALSE,
			     sizeof (CamelMboxSummaryInformation));
	g_array_set_size (summary->message_info, summary->nb_message);

	for (cur_msg = 0; cur_msg < summary->nb_message; cur_msg++)  {
		msg_info = (CamelMboxSummaryInformation *)
			(summary->message_info->data) + cur_msg;

		/* Read the meta-info. */
		read (fd, &data, sizeof(data));
		msg_info->position = ntohl (data);
		read (fd, &data, sizeof(data));
		msg_info->size = ntohl (data);
		read (fd, &data, sizeof(data));
		msg_info->x_evolution_offset = ntohl (data);
		read (fd, &data, sizeof(data));
		msg_info->uid = ntohl (data);
		msg_info->headers.uid = g_strdup_printf ("%d", msg_info->uid);
		read (fd, &msg_info->status, 1);

		/* Read the subject. */
		read (fd, &field_length, sizeof (field_length));
		field_length = ntohl (field_length);
		if (field_length > 0) {			
			msg_info->headers.subject =
				g_new0 (gchar, field_length + 1);
			read (fd, msg_info->headers.subject, field_length);
		} else 
			msg_info->headers.subject = NULL;
		
		/* Read the sender. */
		read (fd, &field_length, sizeof (field_length));
		field_length = ntohl (field_length);
		if (field_length > 0) {			
			msg_info->headers.sender =
				g_new0 (gchar, field_length + 1);
			read (fd, msg_info->headers.sender, field_length);
		} else 
			msg_info->headers.sender = NULL;
		
		/* Read the "to" field. */
		read (fd, &field_length, sizeof (field_length));
		field_length = ntohl (field_length);
		if (field_length > 0) {			
			msg_info->headers.to =
				g_new0 (gchar, field_length + 1);
			read (fd, msg_info->headers.to, field_length);
		} else 
			msg_info->headers.to = NULL;

		/* Read the sent date field. */
		read (fd, &field_length, sizeof (field_length));
		field_length = ntohl (field_length);
		if (field_length > 0) {			
			msg_info->headers.sent_date =
				g_new0 (gchar, field_length + 1);
			read (fd, msg_info->headers.sent_date, field_length);
		} else 
			msg_info->headers.sent_date = NULL;

		/* Read the received date field. */
		read (fd, &field_length, sizeof (field_length));
		field_length = ntohl (field_length);
		if (field_length > 0) {			
			msg_info->headers.received_date =
				g_new0 (gchar, field_length + 1);
			read (fd, msg_info->headers.received_date,
			      field_length);
		} else 
			msg_info->headers.received_date = NULL;
	}		

	close (fd);
	return summary;
}


/**
 * camel_mbox_summary_append_entries:
 * @summary: 
 * @entries: 
 * 
 * append an entry to a summary
 **/
void
camel_mbox_summary_append_entries (CamelMboxSummary *summary, GArray *entries)
{

	summary->message_info = g_array_append_vals (summary->message_info,
						     entries->data,
						     entries->len);
}
