/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Michael Zucchi <notzed@ximian.com>
 *
 *  Copyright 1999-2003 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifndef CAMEL_LOCAL_PRIVATE_H
#define CAMEL_LOCAL_PRIVATE_H 1

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

/* need a way to configure and save this data, if this header is to
   be installed.  For now, dont install it */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <pthread.h>

struct _CamelLocalFolderPrivate {
	GMutex *search_lock;	/* for locking the search object */
};

#define CAMEL_LOCAL_FOLDER_LOCK(f, l) (g_mutex_lock(((CamelLocalFolder *)f)->priv->l))
#define CAMEL_LOCAL_FOLDER_UNLOCK(f, l) (g_mutex_unlock(((CamelLocalFolder *)f)->priv->l))

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_LOCAL_PRIVATE_H */

