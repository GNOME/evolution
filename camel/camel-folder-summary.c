/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camelFolderSummary.c : Abstract class for a folder_summary */


/* 
 *
 * Author : 
 *  Bertrand Guiheneuf <bertrand@helixcode.com>
 *
 * Copyright 1999, 2000 Helix Code, Inc. (http://www.helixcode.com)
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
#include "camel-folder-summary.h"
#include "camel-log.h"

static GtkObjectClass *parent_class = NULL;

/* Returns the class for a CamelFolderSummary */
#define CFS_CLASS(so) CAMEL_FOLDER_SUMMARY_CLASS (GTK_OBJECT(so)->klass)


static int count_messages (CamelFolderSummary *summary);
static int count_subfolders (CamelFolderSummary *summary);
static GPtrArray *get_subfolder_info (CamelFolderSummary *summary,
				      int first, int count);
static GPtrArray *get_message_info (CamelFolderSummary *summary,
				    int first, int count);

static void
camel_folder_summary_class_init (CamelFolderSummaryClass *camel_folder_summary_class)
{
	parent_class = gtk_type_class (gtk_object_get_type ());

	/* virtual method definition */
	camel_folder_summary_class->count_messages = count_messages;
	camel_folder_summary_class->count_subfolders = count_subfolders;
	camel_folder_summary_class->get_subfolder_info = get_subfolder_info;
	camel_folder_summary_class->get_message_info = get_message_info;
}



GtkType
camel_folder_summary_get_type (void)
{
	static GtkType camel_folder_summary_type = 0;
	
	if (!camel_folder_summary_type)	{
		GtkTypeInfo camel_folder_summary_info =	
		{
			"CamelFolderSummary",
			sizeof (CamelFolderSummary),
			sizeof (CamelFolderSummaryClass),
			(GtkClassInitFunc) camel_folder_summary_class_init,
			(GtkObjectInitFunc) NULL,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		camel_folder_summary_type = gtk_type_unique (gtk_object_get_type (), &camel_folder_summary_info);
	}
	
	return camel_folder_summary_type;
}


static int
count_messages (CamelFolderSummary *summary)
{
	g_warning ("CamelFolderSummary::count_messages not implemented for `%s'", gtk_type_name (GTK_OBJECT_TYPE (summary)));
	return 0;
}

/**
 * camel_folder_summary_count_messages: return the number of messages
 * in the folder.
 * @summary: the summary
 *
 * Return value: the number of messages in the folder.
 **/
int
camel_folder_summary_count_messages (CamelFolderSummary *summary)
{
	return CFS_CLASS (summary)->count_messages (summary);
}


static int
count_subfolders (CamelFolderSummary *summary)
{
	g_warning ("CamelFolderSummary::count_subfolders not implemented for `%s'", gtk_type_name (GTK_OBJECT_TYPE (summary)));
	return 0;
}

/**
 * camel_folder_summary_count_subfolders: return the number of subfolders
 * in the folder.
 * @summary: the summary
 *
 * Return value: the number of subfolders in the folder.
 **/
int
camel_folder_summary_count_subfolders (CamelFolderSummary *summary)
{
	return CFS_CLASS (summary)->count_subfolders (summary);
}


static GPtrArray *
get_subfolder_info (CamelFolderSummary *summary, int first, int count)
{
	g_warning ("CamelFolderSummary::get_subfolder_info not implemented for `%s'", gtk_type_name (GTK_OBJECT_TYPE (summary)));
	return NULL;
}

/**
 * camel_folder_summary_get_subfolder_info: return an array of subfolders
 * @summary: a summary
 * @first: the index of the first subfolder to return information for
 * (starting from 0)
 * @count: the number of subfolders to return information for
 *
 * Returns an array of pointers to CamelFolderInfo objects. The caller
 * must free the array when it is done with it, but should not modify
 * the elements.
 *
 * Return value: an array containing information about the subfolders.
 **/
GPtrArray *
camel_folder_summary_get_subfolder_info (CamelFolderSummary *summary,
					 int first, int count)
{
	return CFS_CLASS (summary)->get_subfolder_info (summary, first, count);
}


static GPtrArray *
get_message_info (CamelFolderSummary *summary, int first, int count)
{
	g_warning ("CamelFolderSummary::get_message_info not implemented for `%s'", gtk_type_name (GTK_OBJECT_TYPE (summary)));
	return NULL;
}

/**
 * camel_folder_summary_get_message_info: return an array of messages
 * @summary: a summary
 * @first: the index of the first message to return information for
 * (starting from 0)
 * @count: the number of messages to return information for
 *
 * Returns an array of pointers to CamelMessageInfo objects. The caller
 * must free the array when it is done with it, but should not modify
 * the elements.
 *
 * Return value: an array containing information about the messages.
 **/
GPtrArray *
camel_folder_summary_get_message_info (CamelFolderSummary *summary,
				       int first, int count)
{
	return CFS_CLASS (summary)->get_message_info (summary, first, count);
}
