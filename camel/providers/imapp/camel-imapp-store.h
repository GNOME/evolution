/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-imap-store.h : class for an imap store */

/* 
 * Authors: Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 2002 Ximian, Inc. (www.ximian.com)
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


#ifndef CAMEL_IMAPP_STORE_H
#define CAMEL_IMAPP_STORE_H 1

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <camel/camel-types.h>
#include <camel/camel-store.h>
#include "camel-imapp-driver.h"
#include "libedataserver/e-memory.h"

#define CAMEL_IMAPP_STORE_TYPE     (camel_imapp_store_get_type ())
#define CAMEL_IMAPP_STORE(obj)     (CAMEL_CHECK_CAST((obj), CAMEL_IMAPP_STORE_TYPE, CamelIMAPPStore))
#define CAMEL_IMAPP_STORE_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), CAMEL_IMAPP_STORE_TYPE, CamelIMAPPStoreClass))
#define CAMEL_IS_IMAP_STORE(o)    (CAMEL_CHECK_TYPE((o), CAMEL_IMAPP_STORE_TYPE))

struct _pending_fetch {
	struct _pending_fetch *next;
	struct _pending_fetch *prev;

	struct _CamelMessageInfo *info;
};

typedef struct {
	CamelStore parent_object;

	struct _CamelIMAPPStoreSummary *summary; /* in-memory list of folders */
	struct _CamelIMAPPDriver *driver; /* IMAP processing engine */
	struct _CamelDataCache *cache;

	/* if we had a login error, what to show to user */
	char *login_error;

	GPtrArray *pending_list;
} CamelIMAPPStore;

typedef struct {
	CamelStoreClass parent_class;

} CamelIMAPPStoreClass;

/* Standard Camel function */
CamelType camel_imapp_store_get_type (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_IMAPP_STORE_H */


