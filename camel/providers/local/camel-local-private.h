/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *  camel-local-private.h: Private info for local provider.
 *
 * Authors: Michael Zucchi <notzed@helixcode.com>
 *
 * Copyright 1999, 2000 Helix Code, Inc. (http://www.helixcode.com)
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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
#endif /* __cplusplus }*/

/* need a way to configure and save this data, if this header is to
   be installed.  For now, dont install it */

#include "config.h"

#ifdef ENABLE_THREADS
#include <pthread.h>
#endif

struct _CamelLocalFolderPrivate {
#ifdef ENABLE_THREADS
	GMutex *search_lock;	/* for locking the search object */
#endif
};

#ifdef ENABLE_THREADS
#define CAMEL_LOCAL_FOLDER_LOCK(f, l) (g_mutex_lock(((CamelLocalFolder *)f)->priv->l))
#define CAMEL_LOCAL_FOLDER_UNLOCK(f, l) (g_mutex_unlock(((CamelLocalFolder *)f)->priv->l))
#else
#define CAMEL_LOCAL_FOLDER_LOCK(f, l)
#define CAMEL_LOCAL_FOLDER_UNLOCK(f, l)
#endif

struct _CamelSpoolFolderPrivate {
#ifdef ENABLE_THREADS
	GMutex *search_lock;	/* for locking the search object */
#endif
};

#ifdef ENABLE_THREADS
#define CAMEL_SPOOL_FOLDER_LOCK(f, l) (g_mutex_lock(((CamelSpoolFolder *)f)->priv->l))
#define CAMEL_SPOOL_FOLDER_UNLOCK(f, l) (g_mutex_unlock(((CamelSpoolFolder *)f)->priv->l))
#else
#define CAMEL_SPOOL_FOLDER_LOCK(f, l)
#define CAMEL_SPOOL_FOLDER_UNLOCK(f, l)
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_H */

