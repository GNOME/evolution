/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-nntp-folder.h : NNTP group (folder) support. */

/* 
 *
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


#ifndef CAMEL_NNTP_FOLDER_H
#define CAMEL_NNTP_FOLDER_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include "camel-folder.h"

/*  #include "camel-store.h" */

#define CAMEL_NNTP_FOLDER_TYPE     (camel_nntp_folder_get_type ())
#define CAMEL_NNTP_FOLDER(obj)     (CAMEL_CHECK_CAST((obj), CAMEL_NNTP_FOLDER_TYPE, CamelNNTPFolder))
#define CAMEL_NNTP_FOLDER_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), CAMEL_NNTP_FOLDER_TYPE, CamelNNTPFolderClass))
#define IS_CAMEL_NNTP_FOLDER(o)    (CAMEL_CHECK_TYPE((o), CAMEL_NNTP_FOLDER_TYPE))


typedef struct {
	CamelFolder parent_object;

	gchar *group_name;
	gchar *summary_file_path;  /* contains the messages summary */
	CamelFolderSummary *summary;
} CamelNNTPFolder;



typedef struct {
	CamelFolderClass parent_class;

	/* Virtual methods */	
	
} CamelNNTPFolderClass;


/* public methods */

/* Standard Camel function */
CamelType camel_nntp_folder_get_type (void);

CamelFolder *camel_nntp_folder_new (CamelStore *parent, const char *folder_name, CamelException *ex);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_NNTP_FOLDER_H */
