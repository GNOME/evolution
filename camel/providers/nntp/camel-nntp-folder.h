/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-nntp-folder.h : NNTP group (folder) support. */

/* 
 *
 * Author : Chris Toshok <toshok@ximian.com> 
 *
 * Copyright (C) 2000 Ximian .
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of version 2 of the GNU General Public 
 * License as published by the Free Software Foundation.
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

#include "camel/camel-folder.h"
#include "camel/camel-disco-folder.h"

/*  #include "camel-store.h" */

#define CAMEL_NNTP_FOLDER_TYPE     (camel_nntp_folder_get_type ())
#define CAMEL_NNTP_FOLDER(obj)     (CAMEL_CHECK_CAST((obj), CAMEL_NNTP_FOLDER_TYPE, CamelNNTPFolder))
#define CAMEL_NNTP_FOLDER_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), CAMEL_NNTP_FOLDER_TYPE, CamelNNTPFolderClass))
#define CAMEL_IS_NNTP_FOLDER(o)    (CAMEL_CHECK_TYPE((o), CAMEL_NNTP_FOLDER_TYPE))

typedef struct _CamelNNTPFolder {
	CamelDiscoFolder parent;

	struct _CamelNNTPFolderPrivate *priv;

	struct _CamelFolderChangeInfo *changes;
	char *storage_path;
	CamelFolderSearch *search;
} CamelNNTPFolder;

typedef struct _CamelNNTPFolderClass {
	CamelDiscoFolderClass parent;

	/* Virtual methods */	
	
} CamelNNTPFolderClass;

/* public methods */

/* Standard Camel function */
CamelType camel_nntp_folder_get_type (void);

CamelFolder *camel_nntp_folder_new (CamelStore *parent, const char *folder_name, CamelException *ex);

void camel_nntp_folder_selected(CamelNNTPFolder *folder, char *line, CamelException *ex);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_NNTP_FOLDER_H */
