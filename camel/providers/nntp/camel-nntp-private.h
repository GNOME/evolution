/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *  camel-nntp-private.h: Private info for nntp.
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

#ifndef CAMEL_NNTP_PRIVATE_H
#define CAMEL_NNTP_PRIVATE_H 1

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

/* need a way to configure and save this data, if this header is to
   be installed.  For now, dont install it */

#include "config.h"

#ifdef ENABLE_THREADS
#include "e-util/e-msgport.h"
#endif

struct _CamelNNTPStorePrivate {
#ifdef ENABLE_THREADS
	EMutex *command_lock;	/* for locking the command stream for a complete operation */
#endif
};

#ifdef ENABLE_THREADS
#define CAMEL_NNTP_STORE_LOCK(f, l) (e_mutex_lock(((CamelNNTPStore *)f)->priv->l))
#define CAMEL_NNTP_STORE_UNLOCK(f, l) (e_mutex_unlock(((CamelNNTPStore *)f)->priv->l))
#else
#define CAMEL_NNTP_STORE_LOCK(f, l)
#define CAMEL_NNTP_STORE_UNLOCK(f, l)
#endif

struct _CamelNNTPFolderPrivate {
#ifdef ENABLE_THREADS
	GMutex *search_lock;	/* for locking the search object */
	GMutex *cache_lock;     /* for locking the cache object */
#endif
};

#ifdef ENABLE_THREADS
#define CAMEL_NNTP_FOLDER_LOCK(f, l) (g_mutex_lock(((CamelNNTPFolder *)f)->priv->l))
#define CAMEL_NNTP_FOLDER_UNLOCK(f, l) (g_mutex_unlock(((CamelNNTPFolder *)f)->priv->l))
#else
#define CAMEL_NNTP_FOLDER_LOCK(f, l)
#define CAMEL_NNTP_FOLDER_UNLOCK(f, l)
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_NNTP_PRIVATE_H */

