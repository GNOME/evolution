/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-nntp-store.h : class for an nntp store */

/* 
 *
 * Copyright (C) 2000 Ximian, Inc. <toshok@ximian.com>
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


#ifndef CAMEL_NNTP_STORE_H
#define CAMEL_NNTP_STORE_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <camel/camel-disco-store.h>

#include "camel-nntp-stream.h"
#include "camel-nntp-store-summary.h"

struct _CamelNNTPFolder;
struct _CamelException;

#define CAMEL_NNTP_STORE_TYPE     (camel_nntp_store_get_type ())
#define CAMEL_NNTP_STORE(obj)     (CAMEL_CHECK_CAST((obj), CAMEL_NNTP_STORE_TYPE, CamelNNTPStore))
#define CAMEL_NNTP_STORE_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), CAMEL_NNTP_STORE_TYPE, CamelNNTPStoreClass))
#define CAMEL_IS_NNTP_STORE(o)    (CAMEL_CHECK_TYPE((o), CAMEL_NNTP_STORE_TYPE))

#define CAMEL_NNTP_EXT_SEARCH     (1<<0)
#define CAMEL_NNTP_EXT_SETGET     (1<<1)
#define CAMEL_NNTP_EXT_OVER       (1<<2)
#define CAMEL_NNTP_EXT_XPATTEXT   (1<<3)
#define CAMEL_NNTP_EXT_XACTIVE    (1<<4)
#define CAMEL_NNTP_EXT_LISTMOTD   (1<<5)
#define CAMEL_NNTP_EXT_LISTSUBSCR (1<<6)
#define CAMEL_NNTP_EXT_LISTPNAMES (1<<7)

typedef struct _CamelNNTPStore CamelNNTPStore;
typedef struct _CamelNNTPStoreClass CamelNNTPStoreClass;

enum _xover_t {
	XOVER_STRING = 0,
	XOVER_MSGID,
	XOVER_SIZE,
};

struct _xover_header {
	struct _xover_header *next;

	const char *name;
	unsigned int skip:8;
	enum _xover_t type:8;
};

struct _CamelNNTPStore {
	CamelDiscoStore parent_object;	
	
	struct _CamelNNTPStorePrivate *priv;
	
	guint32 extensions;
	
	unsigned int posting_allowed:1;
	unsigned int do_short_folder_notation:1;
	unsigned int folder_hierarchy_relative:1;

	struct _CamelNNTPStoreSummary *summary;
	
	struct _CamelNNTPStream *stream;
	struct _CamelStreamMem *mem;
	
	struct _CamelDataCache *cache;
	
	char *current_folder, *storage_path, *base_url;

	struct _xover_header *xover;
};

struct _CamelNNTPStoreClass {
	CamelDiscoStoreClass parent_class;

};

/* Standard Camel function */
CamelType camel_nntp_store_get_type (void);

int camel_nntp_raw_commandv (CamelNNTPStore *store, struct _CamelException *ex, char **line, const char *fmt, va_list ap);
int camel_nntp_raw_command(CamelNNTPStore *store, struct _CamelException *ex, char **line, const char *fmt, ...);
int camel_nntp_command (CamelNNTPStore *store, struct _CamelException *ex, struct _CamelNNTPFolder *folder, char **line, const char *fmt, ...);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_NNTP_STORE_H */


