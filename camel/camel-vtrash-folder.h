/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */


#ifndef _CAMEL_VTRASH_FOLDER_H
#define _CAMEL_VTRASH_FOLDER_H

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <camel/camel-folder.h>
#include <camel/camel-vee-folder.h>

#define CAMEL_VTRASH_FOLDER(obj)         CAMEL_CHECK_CAST (obj, camel_vtrash_folder_get_type (), CamelVTrashFolder)
#define CAMEL_VTRASH_FOLDER_CLASS(klass) CAMEL_CHECK_CLASS_CAST (klass, camel_vtrash_folder_get_type (), CamelVTrashFolderClass)
#define CAMEL_IS_VTRASH_FOLDER(obj)      CAMEL_CHECK_TYPE (obj, camel_vtrash_folder_get_type ())

typedef struct _CamelVTrashFolder      CamelVTrashFolder;
typedef struct _CamelVTrashFolderClass CamelVTrashFolderClass;

#define CAMEL_VTRASH_NAME "Trash"
#define CAMEL_VJUNK_NAME "Junk"

enum _camel_vtrash_folder_t {
	CAMEL_VTRASH_FOLDER_TRASH,
	CAMEL_VTRASH_FOLDER_JUNK,
	CAMEL_VTRASH_FOLDER_LAST
};

struct _CamelVTrashFolder {
	CamelVeeFolder parent;

	guint32 bit;
};

struct _CamelVTrashFolderClass {
	CamelVeeFolderClass parent_class;
	
};

CamelType       camel_vtrash_folder_get_type    (void);

CamelFolder    *camel_vtrash_folder_new		(CamelStore *parent_store, enum _camel_vtrash_folder_t type);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ! _CAMEL_VTRASH_FOLDER_H */
