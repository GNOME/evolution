/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Author: Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999 Ximian (www.ximian.com/).
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

#ifndef CAMEL_LOCAL_FOLDER_H
#define CAMEL_LOCAL_FOLDER_H 1

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <camel/camel-folder.h>
#include <camel/camel-folder-search.h>
#include <camel/camel-index.h>
#include "camel-local-summary.h"
#include "camel-lock.h"

/*  #include "camel-store.h" */

#define CAMEL_LOCAL_FOLDER_TYPE     (camel_local_folder_get_type ())
#define CAMEL_LOCAL_FOLDER(obj)     (CAMEL_CHECK_CAST((obj), CAMEL_LOCAL_FOLDER_TYPE, CamelLocalFolder))
#define CAMEL_LOCAL_FOLDER_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), CAMEL_LOCAL_FOLDER_TYPE, CamelLocalFolderClass))
#define CAMEL_IS_LOCAL_FOLDER(o)    (CAMEL_CHECK_TYPE((o), CAMEL_LOCAL_FOLDER_TYPE))

enum {
	CAMEL_LOCAL_FOLDER_ARG_INDEX_BODY = CAMEL_FOLDER_ARG_LAST,

	CAMEL_LOCAL_FOLDER_ARG_LAST = CAMEL_FOLDER_ARG_LAST + 0x100
};

enum {
	CAMEL_LOCAL_FOLDER_INDEX_BODY = CAMEL_LOCAL_FOLDER_ARG_INDEX_BODY | CAMEL_ARG_BOO,
};

typedef struct {
	CamelFolder parent_object;
	struct _CamelLocalFolderPrivate *priv;

	guint32 flags;		/* open mode flags */

	int locked;		/* lock counter */
	CamelLockType locktype;	/* what type of lock we have */

	char *base_path;	/* base path of the local folder */
	char *folder_path;	/* the path to the folder itself */
	char *summary_path;	/* where the summary lives */
	char *index_path;	/* where the index file lives */

	CamelIndex *index;	   /* index for this folder */
	CamelFolderSearch *search; /* used to run searches, we just use the real thing (tm) */
	CamelFolderChangeInfo *changes;	/* used to store changes to the folder during processing */
} CamelLocalFolder;

typedef struct {
	CamelFolderClass parent_class;

	/* Virtual methods */	
	
	/* path construction, only used at init */
	char * (* get_full_path)(CamelLocalFolder *lf, const char *toplevel_dir, const char *full_name);
	char * (* get_meta_path)(CamelLocalFolder *lf, const char *toplevel_dir, const char *full_name, const char *ext);
	
	/* summary factory, only used at init */
	CamelLocalSummary *(*create_summary)(CamelLocalFolder *lf, const char *path, const char *folder, CamelIndex *index);

	/* Lock the folder for my operations */
	int (*lock)(CamelLocalFolder *, CamelLockType type, CamelException *ex);

	/* Unlock the folder for my operations */
	void (*unlock)(CamelLocalFolder *);
} CamelLocalFolderClass;


/* public methods */
/* flags are taken from CAMEL_STORE_FOLDER_* flags */
CamelLocalFolder *camel_local_folder_construct(CamelLocalFolder *lf, CamelStore *parent_store,
					       const char *full_name, guint32 flags, CamelException *ex);

/* Standard Camel function */
CamelType camel_local_folder_get_type(void);

/* Lock the folder for internal use.  May be called repeatedly */
/* UNIMPLEMENTED */
int camel_local_folder_lock(CamelLocalFolder *lf, CamelLockType type, CamelException *ex);
int camel_local_folder_unlock(CamelLocalFolder *lf);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_LOCAL_FOLDER_H */
