/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camelFolderSummary.h : Abstract class for a folder summary */

/* 
 *
 * Author : 
 *  Bertrand Guiheneuf <Bertrand.Guiheneuf@aful.org>
 *
 * Copyright 1999, 2000 HelixCode (http://www.helixcode.com) .
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


#ifndef CAMEL_FOLDER_SUMMARY_H
#define CAMEL_FOLDER_SUMMARY_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <gtk/gtk.h>



#define CAMEL_FOLDER_SUMMARY_TYPE     (camel_folder_summary_get_type ())
#define CAMEL_FOLDER_SUMMARY(obj)     (GTK_CHECK_CAST((obj), CAMEL_FOLDER_SUMMARY_TYPE, CamelFolderSummary))
#define CAMEL_FOLDER_SUMMARY_CLASS(k) (GTK_CHECK_CLASS_CAST ((k), CAMEL_FOLDER_SUMMARY_TYPE, CamelFolderSummaryClass))
#define CAMEL_IS_FOLDER_SUMMARY(o)    (GTK_CHECK_TYPE((o), CAMEL_FOLDER_SUMMARY_TYPE))

typedef struct {
	gchar *name;
	gint nb_message;
	gint nb_unread_message;
	gint nb_deleted_message;
	
	GHashTable *extended_fields;
} CamelFolderInfo;

typedef struct {
	gchar *subject;
	gchar *uid;
	gchar *date;
	gchar *sender;

	GHashTable *extended_fields;
} CamelMessageInfo;


typedef struct 
{
	GtkObject parent_object;

	GList *subfolder_info_list; /* informations on subfolders */
	GList *message_info_list;   /* informations on messages */

} CamelFolderSummary;



typedef struct {
	GtkObjectClass parent_class;
	
	/* Virtual methods */	
	const GList *  (*get_subfolder_info_list) (CamelFolderSummary *summary);
	const GList *  (*get_message_info_list) (CamelFolderSummary *summary);

} CamelFolderSummaryClass;



/* Standard Gtk function */
GtkType camel_folder_summary_get_type (void);


/* public methods */
CamelFolderSummary *camel_folder_summary_new ();
const GList *camel_folder_summary_get_subfolder_info_list (CamelFolderSummary *summary);
const GList *camel_folder_summary_get_message_info_list (CamelFolderSummary *summary);



#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_FOLDER_SUMMARY_H */
