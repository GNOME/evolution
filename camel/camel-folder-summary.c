/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camelFolderSummary.c : Abstract class for a folder_summary */


/* 
 *
 * Author : 
 *  Bertrand Guiheneuf <Bertrand.Guiheneuf@aful.org>
 *
 * Copyright 1999 International GNOME Support (http://www.gnome-support.com) .
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

static GtkObjectClass *parent_class=NULL;

/* Returns the class for a CamelFolderSummary */
#define CFS_CLASS(so) CAMEL_FOLDER_SUMMARY_CLASS (GTK_OBJECT(so)->klass)


static const GList *_get_subfolder_info_list (CamelFolderSummary *summary);
static const GList *_get_message_info_list (CamelFolderSummary *summary);

static void _finalize (GtkObject *object);

static void
camel_folder_summary_class_init (CamelFolderSummaryClass *camel_folder_summary_class)
{
	GtkObjectClass *gtk_object_class = GTK_OBJECT_CLASS (camel_folder_summary_class);

	parent_class = gtk_type_class (gtk_object_get_type ());
	
	/* virtual method definition */
	camel_folder_summary_class->get_subfolder_info_list = _get_subfolder_info_list;
	camel_folder_summary_class->get_message_info_list = _get_message_info_list;


	/* virtual method overload */
	gtk_object_class->finalize = _finalize;
}





static void
camel_folder_summary_init (gpointer   object,  gpointer   klass)
{
	CamelFolderSummary *summary = CAMEL_FOLDER_SUMMARY (object);

	CAMEL_LOG_FULL_DEBUG ( "camel_folder_summary_init:: Entering\n");
	summary->subfolder_info_list = NULL;
	summary->message_info_list = NULL;
	CAMEL_LOG_FULL_DEBUG ( "camel_folder_summary_init:: Leaving\n");
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
			(GtkObjectInitFunc) camel_folder_summary_init,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		camel_folder_summary_type = gtk_type_unique (gtk_object_get_type (), &camel_folder_summary_info);
	}
	
	return camel_folder_summary_type;
}


static void           
_finalize (GtkObject *object)
{
	CamelFolderSummary *camel_folder_summary = CAMEL_FOLDER_SUMMARY (object);

	CAMEL_LOG_FULL_DEBUG ("Entering CamelFolderSummary::finalize\n");
	CAMEL_LOG_FULL_DEBUG  ("CamelFolderSummary::finalize, finalizing object %p\n", object);
	
	parent_class->finalize (object);
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelFolderSummary::finalize\n");
}


CamelFolderSummary *
camel_folder_summary_new ()
{
	return gtk_type_new (CAMEL_FOLDER_SUMMARY_TYPE);
}

static const GList *
_get_subfolder_info_list (CamelFolderSummary *summary)
{
	return summary->subfolder_info_list;
}


const GList *
camel_folder_summary_get_subfolder_info_list (CamelFolderSummary *summary)
{
	return CFS_CLASS (summary)->get_subfolder_info_list (summary);
}




static const GList *
_get_message_info_list (CamelFolderSummary *summary)
{
	return summary->message_info_list;
}

const GList *
camel_folder_summary_get_message_info_list (CamelFolderSummary *summary)
{
	return CFS_CLASS (summary)->get_message_info_list (summary);
}


