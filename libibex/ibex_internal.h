/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <glib.h>

#include "ibex.h"
#include "block.h"
#include "wordindex.h"

struct ibex {
	char *path;
	struct _memcache *blocks;
	struct _IBEXWord *words;
	int predone;

	/* sigh i hate glib's mutex stuff too */
#ifdef ENABLE_THREADS
	GMutex *lock;
#endif
	
};

#ifdef ENABLE_THREADS
/*#define IBEX_LOCK(ib) (printf(__FILE__ "%d: %s: locking ibex\n", __LINE__, __FUNCTION__), g_mutex_lock(ib->lock))
  #define IBEX_UNLOCK(ib) (printf(__FILE__ "%d: %s: unlocking ibex\n", __LINE__, __FUNCTION__), g_mutex_unlock(ib->lock))*/
#define IBEX_LOCK(ib) (g_mutex_lock(ib->lock))
#define IBEX_UNLOCK(ib) (g_mutex_unlock(ib->lock))
#else
#define IBEX_LOCK(ib) 
#define IBEX_UNLOCK(ib) 
#endif

