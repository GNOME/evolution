/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Authors:
 * 	Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999 Ximian Inc.
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

#ifndef CAMEL_MH_FOLDER_H
#define CAMEL_MH_FOLDER_H 1

#ifdef __cplusplus
extern "C" {
#pragma }
#endif				/* __cplusplus } */
#include "camel-local-folder.h"

#define CAMEL_MH_FOLDER_TYPE     (camel_mh_folder_get_type ())
#define CAMEL_MH_FOLDER(obj)     (CAMEL_CHECK_CAST((obj), CAMEL_MH_FOLDER_TYPE, CamelMhFolder))
#define CAMEL_MH_FOLDER_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), CAMEL_MH_FOLDER_TYPE, CamelMhFolderClass))
#define CAMEL_IS_MH_FOLDER(o)    (CAMEL_CHECK_TYPE((o), CAMEL_MH_FOLDER_TYPE))

typedef struct {
	CamelLocalFolder parent_object;

} CamelMhFolder;

typedef struct {
	CamelLocalFolderClass parent_class;
	
	/* Virtual methods */
	
} CamelMhFolderClass;

/* public methods */
CamelFolder *camel_mh_folder_new(CamelStore *parent_store, const char *full_name, guint32 flags, CamelException *ex);

/* Standard Camel function */
CamelType camel_mh_folder_get_type(void);

#ifdef __cplusplus
}
#endif				/* __cplusplus */
#endif				/* CAMEL_MH_FOLDER_H */
