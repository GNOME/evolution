/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-imap-folder.h : Class for a IMAP folder */

/* 
 * Authors: Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 2002 Ximian, Inc. (www.ximian.com)
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

#ifndef CAMEL_IMAPP_FOLDER_H
#define CAMEL_IMAPP_FOLDER_H 1

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <camel/camel-folder.h>

#define CAMEL_IMAPP_FOLDER_TYPE     (camel_imapp_folder_get_type ())
#define CAMEL_IMAPP_FOLDER(obj)     (CAMEL_CHECK_CAST((obj), CAMEL_IMAPP_FOLDER_TYPE, CamelIMAPPFolder))
#define CAMEL_IMAPP_FOLDER_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), CAMEL_IMAPP_FOLDER_TYPE, CamelIMAPPFolderClass))
#define CAMEL_IS_IMAP_FOLDER(o)    (CAMEL_CHECK_TYPE((o), CAMEL_IMAPP_FOLDER_TYPE))

typedef struct _CamelIMAPPFolder {
	CamelFolder parent_object;

	char *raw_name;
	CamelFolderChangeInfo *changes;

	guint32 exists;
	guint32 recent;
	guint32 uidvalidity;
	guint32 unseen;
	guint32 permanentflags;
} CamelIMAPPFolder;

typedef struct _CamelIMAPPFolderClass {
	CamelFolderClass parent_class;
} CamelIMAPPFolderClass;

/* Standard Camel function */
CamelType camel_imapp_folder_get_type (void);

/* public methods */
CamelFolder *camel_imapp_folder_new(CamelStore *parent, const char *path);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_IMAPP_FOLDER_H */
