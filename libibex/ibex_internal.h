/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2000 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <e-util/e-msgport.h>
#include <glib.h>

#include "ibex.h"
#include "block.h"
#include "wordindex.h"

#ifdef ENABLE_THREADS
#include <pthread.h>
#endif

struct ibex {
	struct ibex *next;	/* for list of open ibex's */
	struct ibex *prev;

	int usecount;

	char *name;
	int flags;
	int mode;
	struct _memcache *blocks;
	struct _IBEXWord *words;
	int predone;

#ifdef ENABLE_THREADS
	pthread_mutex_t lock;
#endif
	
};

#define IBEX_OPEN_THRESHOLD (15)

#ifdef ENABLE_THREADS
/*#define IBEX_LOCK(ib) (printf(__FILE__ "%d: %s: locking ibex\n", __LINE__, __FUNCTION__), g_mutex_lock(ib->lock))
  #define IBEX_UNLOCK(ib) (printf(__FILE__ "%d: %s: unlocking ibex\n", __LINE__, __FUNCTION__), g_mutex_unlock(ib->lock))*/
#define IBEX_LOCK(ib) (pthread_mutex_lock(&ib->lock))
#define IBEX_UNLOCK(ib) (pthread_mutex_unlock(&ib->lock))
#define IBEX_TRYLOCK(ib) (pthread_mutex_trylock(&ib->lock))
#else
#define IBEX_LOCK(ib) 
#define IBEX_UNLOCK(ib) 
#define IBEX_TRYLOCK(ib) (0)
#endif

