/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camelFolder.h : Abstract class for an email folder */

/* 
 *
 * Author : 
 *  Bertrand Guiheneuf <bertrand@helixcode.com>
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


#ifndef CAMEL_FOLDER_H
#define CAMEL_FOLDER_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <gtk/gtk.h>

typedef struct _CamelFolder CamelFolder;

#include "camel-store.h"
#include "camel-mime-message.h"
#include "camel-folder-summary.h"
#include "camel-exception.h"

#define CAMEL_FOLDER_TYPE     (camel_folder_get_type ())
#define CAMEL_FOLDER(obj)     (GTK_CHECK_CAST((obj), CAMEL_FOLDER_TYPE, CamelFolder))
#define CAMEL_FOLDER_CLASS(k) (GTK_CHECK_CLASS_CAST ((k), CAMEL_FOLDER_TYPE, CamelFolderClass))
#define CAMEL_IS_FOLDER(o)    (GTK_CHECK_TYPE((o), CAMEL_FOLDER_TYPE))

typedef enum {
	FOLDER_OPEN,
	FOLDER_CLOSE
} CamelFolderState;

typedef enum {
	FOLDER_OPEN_UNKNOWN = 0,   /* folder open mode is unknown */
	FOLDER_OPEN_READ    = 1,   /* folder is read only         */ 
	FOLDER_OPEN_WRITE   = 2,   /* folder is write only        */ 
	FOLDER_OPEN_RW      = 3    /* folder is read/write        */ 
} CamelFolderOpenMode;


typedef void (*CamelFolderAsyncCallback) ();


struct _CamelFolder
{
	GtkObject parent_object;
	
	gboolean can_hold_folders;
	gboolean can_hold_messages;
	CamelFolderOpenMode open_mode;
	CamelFolderState open_state;
	gchar *name;
	gchar *full_name;
	CamelStore *parent_store;
	CamelFolder *parent_folder;
	GList *permanent_flags;

	gboolean has_summary_capability;
	CamelFolderSummary *summary;

	gboolean has_uid_capability;

	gboolean has_search_capability;
};



typedef struct {
	GtkObjectClass parent_class;
	
	/* Virtual methods */	
	void   (*init_with_store)  (CamelFolder *folder, 
				    CamelStore *parent_store, 
				    CamelException *ex);

	void   (*open) (CamelFolder *folder, 
			CamelFolderOpenMode mode, 
			CamelException *ex);
	
	void   (*close) (CamelFolder *folder, 
			 gboolean expunge, 
			 CamelException *ex);

	void   (*open_async) (CamelFolder *folder, 
			      CamelFolderOpenMode mode, 
			      CamelFolderAsyncCallback callback, 
			      gpointer user_data, 
			      CamelException *ex);

	void   (*close_async) (CamelFolder *folder, 
			       gboolean expunge, 
			       CamelFolderAsyncCallback callback, 
			       gpointer user_data, 
			       CamelException *ex);

	void   (*set_name)  (CamelFolder *folder, 
			     const gchar *name, 
			     CamelException *ex);

	/*  	void   (*set_full_name) (CamelFolder *folder, const gchar *name); */
	
	const gchar *  (*get_name)  (CamelFolder *folder, 
				     CamelException *ex);

	const gchar *  (*get_full_name)  (CamelFolder *folder, 
					  CamelException *ex);

	gboolean   (*can_hold_folders)   (CamelFolder *folder, 
					  CamelException *ex);

	gboolean   (*can_hold_messages)  (CamelFolder *folder, 
					  CamelException *ex);

	gboolean   (*exists)  (CamelFolder *folder, 
			       CamelException *ex);

	gboolean   (*is_open) (CamelFolder *folder, 
			       CamelException *ex);

	CamelFolder *  (*get_subfolder)  (CamelFolder *folder, 
					  const gchar *folder_name, 
					  CamelException *ex);

	gboolean   (*create)  (CamelFolder *folder,
			       CamelException *ex);

	gboolean   (*delete)  (CamelFolder *folder, 
			       gboolean recurse, 
			       CamelException *ex);
	
	gboolean   (*delete_messages) (CamelFolder *folder, 
				       CamelException *ex);

	CamelFolder *  (*get_parent_folder)   (CamelFolder *folder, 
					       CamelException *ex);

	CamelStore *  (*get_parent_store) (CamelFolder *folder, 
					   CamelException *ex);

	CamelFolderOpenMode (*get_mode)   (CamelFolder *folder, 
					   CamelException *ex);

	GList *  (*list_subfolders)   (CamelFolder *folder, 
				       CamelException *ex);

	GList *  (*expunge)  (CamelFolder *folder, 
			   CamelException *ex);

	gboolean (*has_message_number_capability) (CamelFolder *folder, CamelException *ex);

	CamelMimeMessage * (*get_message_by_number) (CamelFolder *folder, 
						     gint number, 
						     CamelException *ex);
	
	gint   (*get_message_count)   (CamelFolder *folder, 
				       CamelException *ex);

	void (*append_message)  (CamelFolder *folder, 
				 CamelMimeMessage *message, 
				 CamelException *ex);
	
	const GList * (*list_permanent_flags) (CamelFolder *folder, 
					       CamelException *ex);

	void   (*copy_message_to) (CamelFolder *folder, 
				   CamelMimeMessage *message, 
				   CamelFolder *dest_folder, 
				   CamelException *ex);
	
	gboolean (*has_uid_capability) (CamelFolder *folder);

	const gchar * (*get_message_uid)  (CamelFolder *folder, 
					   CamelMimeMessage *message, 
					   CamelException *ex);

	CamelMimeMessage * (*get_message_by_uid)  (CamelFolder *folder, 
						   const gchar *uid, 
						   CamelException *ex);

	GList * (*get_uid_list)  (CamelFolder *folder, 
				  CamelException *ex);

	gboolean (*has_search_capability) (CamelFolder *folder, CamelException *ex);
	GList *(*search_by_expression) (CamelFolder *folder, const char *expression, CamelException *ex);

} CamelFolderClass;



/* Standard Gtk function */
GtkType camel_folder_get_type (void);


/* public methods */



CamelFolder *camel_folder_get_subfolder (CamelFolder *folder, 
				      gchar *folder_name, 
				      CamelException *ex);

void camel_folder_open (CamelFolder *folder, 
			CamelFolderOpenMode mode, 
			CamelException *ex);


void camel_folder_close (CamelFolder *folder, 
			 gboolean expunge, 
			 CamelException *ex);

gboolean camel_folder_create (CamelFolder *folder, CamelException *ex);
CamelFolder *camel_folder_get_parent_folder (CamelFolder *folder, CamelException *ex);
CamelStore *camel_folder_get_parent_store (CamelFolder *folder, CamelException *ex);
GList *camel_folder_list_subfolders (CamelFolder *folder, CamelException *ex);


/* delete operations */
gboolean camel_folder_delete (CamelFolder *folder, gboolean recurse, CamelException *ex);
gboolean camel_folder_delete_messages (CamelFolder *folder, CamelException *ex);
GList *camel_folder_expunge (CamelFolder *folder, CamelException *ex);


/* folder name manipulation operations */
void camel_folder_set_name (CamelFolder *folder, 
			    const gchar *name, 
			    CamelException *ex);
const gchar *camel_folder_get_name (CamelFolder *folder, CamelException *ex);
const gchar *camel_folder_get_full_name (CamelFolder *folder, CamelException *ex);


/* various properties accessors */
gboolean camel_folder_exists (CamelFolder *folder, CamelException *ex);
const GList *camel_folder_list_permanent_flags (CamelFolder *folder, CamelException *ex);
CamelFolderOpenMode camel_folder_get_mode (CamelFolder *folder, CamelException *ex);
gboolean camel_folder_is_open (CamelFolder *folder, CamelException *ex);



/* message manipulation */
void  camel_folder_append_message  (CamelFolder *folder, 
				    CamelMimeMessage *message, CamelException *ex);
void camel_folder_copy_message_to (CamelFolder *folder, 
				   CamelMimeMessage *message, 
				   CamelFolder *dest_folder, 
				   CamelException *ex);


/* summary related operations */
gboolean camel_folder_has_summary_capability (CamelFolder *folder, CamelException *ex);
CamelFolderSummary *camel_folder_get_summary (CamelFolder *folder, CamelException *ex);


/* number based access operations */
gboolean camel_folder_has_message_number_capability (CamelFolder *folder, CamelException *ex);
CamelMimeMessage *camel_folder_get_message_by_number (CamelFolder *folder, 
						      gint number, CamelException *ex);
gint camel_folder_get_message_count (CamelFolder *folder, CamelException *ex);


/* uid based access operations */
gboolean camel_folder_has_uid_capability (CamelFolder *folder, CamelException *ex);
const gchar * camel_folder_get_message_uid (CamelFolder *folder, CamelMimeMessage *message, CamelException *ex);
CamelMimeMessage *camel_folder_get_message_by_uid  (CamelFolder *folder, const gchar *uid, CamelException *ex);
GList *camel_folder_get_uid_list  (CamelFolder *folder, CamelException *ex);

/* search api */
gboolean camel_folder_has_search_capability            (CamelFolder *folder,
							CamelException *ex);
GList *camel_folder_search_by_expression  (CamelFolder *folder,
					   const char *expression,
					   CamelException *ex);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_FOLDER_H */

