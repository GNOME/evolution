/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *  camel-private.h: Private info for class implementers.
 *
 * Authors: Michael Zucchi <notzed@ximian.com>
 *
 * Copyright 1999, 2000 Ximian, Inc. (www.ximian.com)
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

#ifndef CAMEL_PRIVATE_H
#define CAMEL_PRIVATE_H 1

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

/* need a way to configure and save this data, if this header is to
   be installed.  For now, dont install it */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <pthread.h>
#include <e-util/e-msgport.h>

struct _CamelFolderPrivate {
	EMutex *lock;
	EMutex *change_lock;
	/* must require the 'change_lock' to access this */
	int frozen;
	struct _CamelFolderChangeInfo *changed_frozen; /* queues changed events */
};

#define CAMEL_FOLDER_LOCK(f, l) (e_mutex_lock(((CamelFolder *)f)->priv->l))
#define CAMEL_FOLDER_UNLOCK(f, l) (e_mutex_unlock(((CamelFolder *)f)->priv->l))


struct _CamelStorePrivate {
	EMutex *folder_lock;	/* for locking folder operations */
};

#define CAMEL_STORE_LOCK(f, l) (e_mutex_lock(((CamelStore *)f)->priv->l))
#define CAMEL_STORE_UNLOCK(f, l) (e_mutex_unlock(((CamelStore *)f)->priv->l))


struct _CamelTransportPrivate {
	GMutex *send_lock;   /* for locking send operations */
};

#define CAMEL_TRANSPORT_LOCK(f, l) (g_mutex_lock(((CamelTransport *)f)->priv->l))
#define CAMEL_TRANSPORT_UNLOCK(f, l) (g_mutex_unlock(((CamelTransport *)f)->priv->l))


struct _CamelServicePrivate {
	EMutex *connect_lock;	/* for locking connection operations */
	EMutex *connect_op_lock;/* for locking the connection_op */
};

#define CAMEL_SERVICE_LOCK(f, l) (e_mutex_lock(((CamelService *)f)->priv->l))
#define CAMEL_SERVICE_UNLOCK(f, l) (e_mutex_unlock(((CamelService *)f)->priv->l))
#define CAMEL_SERVICE_ASSERT_LOCKED(f, l) (e_mutex_assert_locked (((CamelService *)f)->priv->l))


struct _CamelSessionPrivate {
	GMutex *lock;		/* for locking everything basically */
	GMutex *thread_lock;	/* locking threads */

	int thread_id;
	GHashTable *thread_active;
	EThread *thread_queue;

	GHashTable *thread_msg_op;
};

#define CAMEL_SESSION_LOCK(f, l) (g_mutex_lock(((CamelSession *)f)->priv->l))
#define CAMEL_SESSION_UNLOCK(f, l) (g_mutex_unlock(((CamelSession *)f)->priv->l))


/* most of this stuff really is private, but the lock can be used by subordinate classes */
struct _CamelFolderSummaryPrivate {
	GHashTable *filter_charset;	/* CamelMimeFilterCharset's indexed by source charset */

	struct _CamelMimeFilterIndex *filter_index;
	struct _CamelMimeFilterBasic *filter_64;
	struct _CamelMimeFilterBasic *filter_qp;
	struct _CamelMimeFilterBasic *filter_uu;
	struct _CamelMimeFilterSave *filter_save;
	struct _CamelMimeFilterHTML *filter_html;

	struct _CamelStreamFilter *filter_stream;

	struct _CamelIndex *index;
	
	GMutex *summary_lock;	/* for the summary hashtable/array */
	GMutex *io_lock;	/* load/save lock, for access to saved_count, etc */
	GMutex *filter_lock;	/* for accessing any of the filtering/indexing stuff, since we share them */
	GMutex *alloc_lock;	/* for setting up and using allocators */
	GMutex *ref_lock;	/* for reffing/unreffing messageinfo's ALWAYS obtain before summary_lock */
};

#define CAMEL_SUMMARY_LOCK(f, l) (g_mutex_lock(((CamelFolderSummary *)f)->priv->l))
#define CAMEL_SUMMARY_UNLOCK(f, l) (g_mutex_unlock(((CamelFolderSummary *)f)->priv->l))


struct _CamelStoreSummaryPrivate {
	GMutex *summary_lock;	/* for the summary hashtable/array */
	GMutex *io_lock;	/* load/save lock, for access to saved_count, etc */
	GMutex *alloc_lock;	/* for setting up and using allocators */
	GMutex *ref_lock;	/* for reffing/unreffing messageinfo's ALWAYS obtain before summary_lock */
};

#define CAMEL_STORE_SUMMARY_LOCK(f, l) (g_mutex_lock(((CamelStoreSummary *)f)->priv->l))
#define CAMEL_STORE_SUMMARY_UNLOCK(f, l) (g_mutex_unlock(((CamelStoreSummary *)f)->priv->l))


struct _CamelVeeFolderPrivate {
	GList *folders;			/* lock using subfolder_lock before changing/accessing */
	GList *folders_changed;		/* for list of folders that have changed between updates */
	
	GMutex *summary_lock;		/* for locking vfolder summary */
	GMutex *subfolder_lock;		/* for locking the subfolder list */
	GMutex *changed_lock;		/* for locking the folders-changed list */
};

#define CAMEL_VEE_FOLDER_LOCK(f, l) (g_mutex_lock(((CamelVeeFolder *)f)->priv->l))
#define CAMEL_VEE_FOLDER_UNLOCK(f, l) (g_mutex_unlock(((CamelVeeFolder *)f)->priv->l))


struct _CamelDataWrapperPrivate {
	pthread_mutex_t stream_lock;
};

#define CAMEL_DATA_WRAPPER_LOCK(dw, l)   (pthread_mutex_lock(&((CamelDataWrapper *)dw)->priv->l))
#define CAMEL_DATA_WRAPPER_UNLOCK(dw, l) (pthread_mutex_unlock(&((CamelDataWrapper *)dw)->priv->l))


/* most of this stuff really is private, but the lock can be used by subordinate classes */
struct _CamelCertDBPrivate {
	GMutex *db_lock;	/* for the db hashtable/array */
	GMutex *io_lock;	/* load/save lock, for access to saved_count, etc */
	GMutex *alloc_lock;	/* for setting up and using allocators */
	GMutex *ref_lock;	/* for reffing/unreffing certs */
};

#define CAMEL_CERTDB_LOCK(db, l) (g_mutex_lock (((CamelCertDB *) db)->priv->l))
#define CAMEL_CERTDB_UNLOCK(db, l) (g_mutex_unlock (((CamelCertDB *) db)->priv->l))

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_PRIVATE_H */
