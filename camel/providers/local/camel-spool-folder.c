/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; fill-column: 160 -*-
 * 
 * Authors: Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 2001-2003 Ximian, Inc. (www.ximian.com)
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

#include "camel-spool-folder.h"
#include "camel-spool-store.h"
#include "camel-stream-fs.h"
#include "camel-spool-summary.h"
#include "camel-data-wrapper.h"
#include "camel-mime-message.h"
#include "camel-stream-filter.h"
#include "camel-mime-filter-from.h"
#include "camel-exception.h"

#include "camel-lock-client.h"

#include "camel-local-private.h"

#define d(x) /*(printf("%s(%d): ", __FILE__, __LINE__),(x))*/

static CamelFolderClass *parent_class = NULL;

/* Returns the class for a CamelSpoolFolder */
#define CSPOOLF_CLASS(so) CAMEL_SPOOL_FOLDER_CLASS (CAMEL_OBJECT_GET_CLASS(so))
#define CF_CLASS(so) CAMEL_FOLDER_CLASS (CAMEL_OBJECT_GET_CLASS(so))
#define CSPOOLS_CLASS(so) CAMEL_STORE_CLASS (CAMEL_OBJECT_GET_CLASS(so))

static CamelLocalSummary *spool_create_summary(const char *path, const char *folder, CamelIndex *index);

static int spool_lock(CamelLocalFolder *lf, CamelLockType type, CamelException *ex);
static void spool_unlock(CamelLocalFolder *lf);

static void spool_finalize(CamelObject * object);

static void
camel_spool_folder_class_init(CamelSpoolFolderClass *klass)
{
	CamelLocalFolderClass *lklass = (CamelLocalFolderClass *)klass;

	parent_class = (CamelFolderClass *)camel_mbox_folder_get_type();

	/* virtual method overload */
	lklass->create_summary = spool_create_summary;
	lklass->lock = spool_lock;
	lklass->unlock = spool_unlock;
}

static void
spool_init(gpointer object, gpointer klass)
{
	CamelSpoolFolder *spool_folder = object;

	spool_folder->lockid = -1;
}

static void
spool_finalize(CamelObject * object)
{
	/*CamelSpoolFolder *spool_folder = CAMEL_SPOOL_FOLDER(object);*/
}

CamelType camel_spool_folder_get_type(void)
{
	static CamelType camel_spool_folder_type = CAMEL_INVALID_TYPE;

	if (camel_spool_folder_type == CAMEL_INVALID_TYPE) {
		camel_spool_folder_type = camel_type_register(camel_mbox_folder_get_type(), "CamelSpoolFolder",
							     sizeof(CamelSpoolFolder),
							     sizeof(CamelSpoolFolderClass),
							     (CamelObjectClassInitFunc) camel_spool_folder_class_init,
							     NULL,
							     (CamelObjectInitFunc) spool_init,
							     (CamelObjectFinalizeFunc) spool_finalize);
	}

	return camel_spool_folder_type;
}

CamelFolder *
camel_spool_folder_new(CamelStore *parent_store, const char *full_name, guint32 flags, CamelException *ex)
{
	CamelFolder *folder;

	d(printf("Creating spool folder: %s in %s\n", full_name, camel_local_store_get_toplevel_dir((CamelLocalStore *)parent_store)));

	folder = (CamelFolder *)camel_object_new(CAMEL_SPOOL_FOLDER_TYPE);

	if (parent_store->flags & CAMEL_STORE_FILTER_INBOX
	    && strcmp(full_name, "INBOX") == 0)
		folder->folder_flags |= CAMEL_FOLDER_FILTER_RECENT;
	flags &= CAMEL_STORE_FOLDER_BODY_INDEX;

	folder = (CamelFolder *)camel_local_folder_construct((CamelLocalFolder *)folder, parent_store, full_name, flags, ex);
	if (folder) {
		if (camel_url_get_param(((CamelService *)parent_store)->url, "xstatus"))
			camel_mbox_summary_xstatus((CamelMboxSummary *)folder->summary, TRUE);
	}

	return folder;
}

static CamelLocalSummary *
spool_create_summary(const char *path, const char *folder, CamelIndex *index)
{
	return (CamelLocalSummary *)camel_spool_summary_new(folder);
}

static int
spool_lock(CamelLocalFolder *lf, CamelLockType type, CamelException *ex)
{
	int retry = 0;
	CamelMboxFolder *mf = (CamelMboxFolder *)lf;
	CamelSpoolFolder *sf = (CamelSpoolFolder *)lf;

	mf->lockfd = open(lf->folder_path, O_RDWR, 0);
	if (mf->lockfd == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot create folder lock on %s: %s"),
				      lf->folder_path, g_strerror (errno));
		return -1;
	}

	while (retry < CAMEL_LOCK_RETRY) {
		if (retry > 0)
			sleep(CAMEL_LOCK_DELAY);

		camel_exception_clear(ex);

		if (camel_lock_fcntl(mf->lockfd, type, ex) == 0) {
			if (camel_lock_flock(mf->lockfd, type, ex) == 0) {
				if ((sf->lockid = camel_lock_helper_lock(lf->folder_path, ex)) != -1)
					return 0;
				camel_unlock_flock(mf->lockfd);
			}
			camel_unlock_fcntl(mf->lockfd);
		}
		retry++;
	}
	
	close (mf->lockfd);
	mf->lockfd = -1;
	
	return -1;
}

static void
spool_unlock(CamelLocalFolder *lf)
{
	CamelMboxFolder *mf = (CamelMboxFolder *)lf;
	CamelSpoolFolder *sf = (CamelSpoolFolder *)lf;

	camel_lock_helper_unlock(sf->lockid);
	sf->lockid = -1;
	camel_unlock_flock(mf->lockfd);
	camel_unlock_fcntl(mf->lockfd);

	close(mf->lockfd);
	mf->lockfd = -1;
}
