/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Author: Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 2001 Ximian Inc (www.ximian.com/)
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

#ifndef CAMEL_SPOOL_FOLDER_H
#define CAMEL_SPOOL_FOLDER_H 1

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include "camel-mbox-folder.h"
#include <camel/camel-folder-search.h>
#include <camel/camel-index.h>
#include "camel-spool-summary.h"
#include "camel-lock.h"

/*  #include "camel-store.h" */

#define CAMEL_SPOOL_FOLDER_TYPE     (camel_spool_folder_get_type ())
#define CAMEL_SPOOL_FOLDER(obj)     (CAMEL_CHECK_CAST((obj), CAMEL_SPOOL_FOLDER_TYPE, CamelSpoolFolder))
#define CAMEL_SPOOL_FOLDER_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), CAMEL_SPOOL_FOLDER_TYPE, CamelSpoolFolderClass))
#define CAMEL_IS_SPOOL_FOLDER(o)    (CAMEL_CHECK_TYPE((o), CAMEL_SPOOL_FOLDER_TYPE))

typedef struct {
	CamelMboxFolder parent;

	struct _CamelSpoolFolderPrivate *priv;

	int lockid;		/* lock id for dot locking */
} CamelSpoolFolder;

typedef struct {
	CamelMboxFolderClass parent_class;
} CamelSpoolFolderClass;

/* Standard Camel function */
CamelType camel_spool_folder_get_type(void);

CamelFolder *camel_spool_folder_new(CamelStore *parent_store, const char *full_name, guint32 flags, CamelException *ex);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_SPOOL_FOLDER_H */
