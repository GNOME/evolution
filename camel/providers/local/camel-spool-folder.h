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

#include <camel/camel-folder.h>
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
	CamelFolder parent_object;
	struct _CamelSpoolFolderPrivate *priv;

	guint32 flags;		/* open mode flags */

	int locked;		/* lock counter */
	CamelLockType locktype;	/* what type of lock we have */
	int lockfd;		/* lock fd used for fcntl/etc locking */
	int lockid;		/* lock id for dot locking */

	char *base_path;	/* base path of the spool folder */
	char *folder_path;	/* the path to the folder itself */
#if 0
	char *summary_path;	/* where the summary lives */
	char *index_path;	/* where the index file lives */

	ibex *index;		   /* index for this folder */
#endif
	CamelFolderSearch *search; /* used to run searches, we just use the real thing (tm) */
	CamelFolderChangeInfo *changes;	/* used to store changes to the folder during processing */
} CamelSpoolFolder;

typedef struct {
	CamelFolderClass parent_class;

	/* Virtual methods */	

	/* summary factory, only used at init */
	CamelSpoolSummary *(*create_summary)(const char *path, const char *folder, CamelIndex *index);

	/* Lock the folder for my operations */
	int (*lock)(CamelSpoolFolder *, CamelLockType type, CamelException *ex);

	/* Unlock the folder for my operations */
	void (*unlock)(CamelSpoolFolder *);
} CamelSpoolFolderClass;


/* public methods */
/* flags are taken from CAMEL_STORE_FOLDER_* flags */
CamelSpoolFolder *camel_spool_folder_construct(CamelSpoolFolder *lf, CamelStore *parent_store,
					       const char *full_name, const char *path, guint32 flags, CamelException *ex);

/* Standard Camel function */
CamelType camel_spool_folder_get_type(void);

CamelFolder *camel_spool_folder_new(CamelStore *parent_store, const char *full_name, const char *path,
				    guint32 flags, CamelException *ex);

/* Lock the folder for internal use.  May be called repeatedly */
/* UNIMPLEMENTED */
int camel_spool_folder_lock(CamelSpoolFolder *lf, CamelLockType type, CamelException *ex);
int camel_spool_folder_unlock(CamelSpoolFolder *lf);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_SPOOL_FOLDER_H */
