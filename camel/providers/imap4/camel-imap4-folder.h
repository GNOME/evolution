/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  Camel
 *  Copyright (C) 1999-2004 Jeffrey Stedfast
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 */


#ifndef __CAMEL_IMAP4_FOLDER_H__
#define __CAMEL_IMAP4_FOLDER_H__

#include <camel/camel-store.h>
#include <camel/camel-folder.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define CAMEL_TYPE_IMAP4_FOLDER            (camel_imap4_folder_get_type ())
#define CAMEL_IMAP4_FOLDER(obj)            (CAMEL_CHECK_CAST ((obj), CAMEL_TYPE_IMAP4_FOLDER, CamelIMAP4Folder))
#define CAMEL_IMAP4_FOLDER_CLASS(klass)    (CAMEL_CHECK_CLASS_CAST ((klass), CAMEL_TYPE_IMAP4_FOLDER, CamelIMAP4FolderClass))
#define CAMEL_IS_IMAP4_FOLDER(obj)         (CAMEL_CHECK_TYPE ((obj), CAMEL_TYPE_IMAP4_FOLDER))
#define CAMEL_IS_IMAP4_FOLDER_CLASS(klass) (CAMEL_CHECK_CLASS_TYPE ((klass), CAMEL_TYPE_IMAP4_FOLDER))
#define CAMEL_IMAP4_FOLDER_GET_CLASS(obj)  (CAMEL_CHECK_GET_CLASS ((obj), CAMEL_TYPE_IMAP4_FOLDER, CamelIMAP4FolderClass))

typedef struct _CamelIMAP4Folder CamelIMAP4Folder;
typedef struct _CamelIMAP4FolderClass CamelIMAP4FolderClass;

struct _CamelIMAP4Folder {
	CamelFolder parent_object;
	
	CamelFolderSearch *search;
	
	char *cachedir;
	char *utf7_name;
};

struct _CamelIMAP4FolderClass {
	CamelFolderClass parent_class;
	
};


CamelType camel_imap4_folder_get_type (void);

CamelFolder *camel_imap4_folder_new (CamelStore *store, const char *full_name, CamelException *ex);

const char *camel_imap4_folder_utf7_name (CamelIMAP4Folder *folder);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __CAMEL_IMAP4_FOLDER_H__ */
